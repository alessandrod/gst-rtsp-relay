/* GStreamer
 * Copyright (C) 2010 Alessandro Decina <alessandro.d@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gst-rtsp-relay-media-factory.h"

#define DEFAULT_LOCATION NULL
#define DEFAULT_FIND_DYNAMIC_STREAMS TRUE
#define DYNAMIC_STREAMS_TIMEOUT 5

enum
{
  PROP_0,
  PROP_LOCATION,
  PROP_FIND_DYNAMIC_STREAMS
};

enum
{
  SIGNAL_LAST
};

typedef struct
{
  GstCaps *caps;
  GstElement *payloader;
} DynamicPayloader;

GST_DEBUG_CATEGORY_STATIC (rtsp_relay_media_factory_debug);
#define GST_CAT_DEFAULT rtsp_relay_media_factory_debug

static void gst_rtsp_relay_media_factory_get_property (GObject *object, guint propid,
    GValue *value, GParamSpec *pspec);
static void gst_rtsp_relay_media_factory_set_property (GObject *object, guint propid,
    const GValue *value, GParamSpec *pspec);
static void gst_rtsp_relay_media_factory_finalize (GObject * obj);
static GstElement * gst_rtsp_relay_media_factory_get_element (GstRTSPMediaFactory *factory,
    const GstRTSPUrl *url);
static void rtspsrc_pad_blocked_cb_link_dynamic (GstPad *pad, gboolean blocked,
    gpointer user_data);

G_DEFINE_TYPE (GstRTSPRelayMediaFactory, gst_rtsp_relay_media_factory, GST_TYPE_RTSP_MEDIA_FACTORY);

static DynamicPayloader *
dynamic_payloader_new (GstElement *payloader, GstCaps *caps)
{
  DynamicPayloader *dynamic_payloader;

  dynamic_payloader = g_new0 (DynamicPayloader, 1);
  dynamic_payloader->payloader = gst_object_ref (payloader);
  dynamic_payloader->caps = caps;

  return dynamic_payloader;
}

static void
dynamic_payloader_free (DynamicPayloader *dynamic_payloader)
{
  gst_caps_unref (dynamic_payloader->caps);
  gst_object_unref (dynamic_payloader->payloader);
  g_free (dynamic_payloader);
}

static void
gst_rtsp_relay_media_factory_class_init (GstRTSPRelayMediaFactoryClass * klass)
{
  GObjectClass *gobject_class;
  GstRTSPMediaFactoryClass *media_factory_class = GST_RTSP_MEDIA_FACTORY_CLASS (klass);

  gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = gst_rtsp_relay_media_factory_get_property;
  gobject_class->set_property = gst_rtsp_relay_media_factory_set_property;
  gobject_class->finalize = gst_rtsp_relay_media_factory_finalize;

  media_factory_class->get_element = gst_rtsp_relay_media_factory_get_element;

  g_object_class_install_property (gobject_class, PROP_LOCATION,
      g_param_spec_string ("location", "Location", "Location",
          DEFAULT_LOCATION, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (gobject_class, PROP_FIND_DYNAMIC_STREAMS,
      g_param_spec_boolean ("find-dynamic-streams",
          "Find dynamic streams", "find dynamic streams",
          DEFAULT_FIND_DYNAMIC_STREAMS, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  GST_DEBUG_CATEGORY_INIT (rtsp_relay_media_factory_debug,
      "rtsprelaymediafactory", 0, "RTSP Relay Media Factory");
}

static void
gst_rtsp_relay_media_factory_init (GstRTSPRelayMediaFactory * factory)
{
  factory->lock = g_mutex_new ();
  factory->location = NULL;
  factory->pay_pads = 0;
  factory->rtspsrc_no_more_pads = FALSE;
  factory->rtspsrc_no_more_pads_cond = g_cond_new ();
  factory->unblocked_pads = 0;
  factory->dynamic_payloaders = NULL;
}

static void
gst_rtsp_relay_media_factory_finalize (GObject * obj)
{
  GstRTSPRelayMediaFactory *factory = GST_RTSP_RELAY_MEDIA_FACTORY (obj);

  g_free (factory->location);
  g_mutex_free (factory->lock);
  g_cond_free (factory->rtspsrc_no_more_pads_cond);
  g_list_foreach (factory->dynamic_payloaders, (GFunc) dynamic_payloader_free, NULL);
  g_list_free (factory->dynamic_payloaders);

  G_OBJECT_CLASS (gst_rtsp_relay_media_factory_parent_class)->finalize (obj);
}

static void
gst_rtsp_relay_media_factory_get_property (GObject *object, guint propid,
    GValue *value, GParamSpec *pspec)
{
  GstRTSPRelayMediaFactory *factory = GST_RTSP_RELAY_MEDIA_FACTORY (object);

  switch (propid) {
    case PROP_LOCATION:
      g_value_set_string (value, factory->location);
      break;
    case PROP_FIND_DYNAMIC_STREAMS:
      g_value_set_boolean (value, factory->find_dynamic_streams);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, propid, pspec);
  }
}

static void
gst_rtsp_relay_media_factory_set_property (GObject *object, guint propid,
    const GValue *value, GParamSpec *pspec)
{
  GstRTSPRelayMediaFactory *factory = GST_RTSP_RELAY_MEDIA_FACTORY (object);

  switch (propid) {
    case PROP_LOCATION:
      g_free (factory->location);
      factory->location = g_value_dup_string (value);
      break;
    case PROP_FIND_DYNAMIC_STREAMS:
      factory->find_dynamic_streams = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, propid, pspec);
  }
}

GstRTSPRelayMediaFactory * gst_rtsp_relay_media_factory_new (const char *url)
{
  GstRTSPRelayMediaFactory *factory;

  factory = g_object_new (GST_TYPE_RTSP_RELAY_MEDIA_FACTORY,
      "location", url, NULL);

  return factory;
}

static void
rtspsrc_pad_blocked_cb_block (GstPad *pad, gboolean blocked, gpointer data)
{
  GstRTSPRelayMediaFactory *factory = GST_RTSP_RELAY_MEDIA_FACTORY (data);

  GST_DEBUG_OBJECT (factory, "blocked pad %s %"GST_PTR_FORMAT,
      GST_PAD_NAME (pad), GST_PAD_CAPS (pad)); 

  g_mutex_lock (factory->lock);
  factory->unblocked_pads -= 1;
  g_cond_signal (factory->rtspsrc_no_more_pads_cond);
  g_mutex_unlock (factory->lock);
}

static void
rtspsrc_pad_added_cb_block (GstElement *rtspsrc, GstPad *pad,
    GstRTSPRelayMediaFactory *factory)
{
  GST_DEBUG_OBJECT (factory, "found new pad %s:%s, blocking",
      GST_DEBUG_PAD_NAME (pad));

  g_mutex_lock (factory->lock);
  factory->unblocked_pads += 1;
  g_mutex_unlock (factory->lock);

  gst_pad_set_blocked_async (pad, TRUE, rtspsrc_pad_blocked_cb_block, factory);
}

static void
do_dynamic_link (GstRTSPRelayMediaFactory *factory, GstPad *pad)
{
  GList *walk, *del;
  GstCaps *pad_caps, *intersect;
  gboolean found;
  GstPad *sink;
  GstPadLinkReturn link_ret;
  DynamicPayloader *dynamic_payloader;
  GST_DEBUG_OBJECT (factory, "trying to link dynamic %s:%s %"GST_PTR_FORMAT,
      GST_DEBUG_PAD_NAME (pad), GST_PAD_CAPS (pad));

  found = FALSE;
  walk = factory->dynamic_payloaders;
  while (walk && !found) {
    dynamic_payloader = (DynamicPayloader *) walk->data;

    pad_caps = gst_pad_get_caps (pad);
    intersect = gst_caps_intersect (dynamic_payloader->caps, pad_caps);

    GST_DEBUG_OBJECT (factory, "trying %s", gst_caps_to_string (intersect));

    if (!gst_caps_is_empty (intersect)) {
      GST_DEBUG_OBJECT (factory, "matches %s", gst_caps_to_string (intersect));

      sink = gst_element_get_static_pad (dynamic_payloader->payloader, "sink");
      link_ret = gst_pad_link (pad, sink);
      if (link_ret == GST_FLOW_OK) {
        found = TRUE;

        del = walk;
        walk = walk->next;
        factory->dynamic_payloaders =
            g_list_delete_link (factory->dynamic_payloaders, del);
      } else {
        GST_ERROR_OBJECT (factory, "couldn't link pads");
        walk = walk->next;
      }

      gst_object_unref (sink);
    } else {
      walk = walk->next;
    }

    gst_caps_unref (pad_caps);
    gst_caps_unref (intersect);

  }

  if (!found)
    GST_WARNING_OBJECT (factory, "couldn't find dynamic payloader");
  
  gst_pad_set_blocked_async (pad, FALSE,
      rtspsrc_pad_blocked_cb_link_dynamic, factory);
}

static void
rtspsrc_pad_blocked_cb_link_dynamic (GstPad *pad, gboolean blocked, gpointer user_data)
{
  GstRTSPRelayMediaFactory *factory = GST_RTSP_RELAY_MEDIA_FACTORY (user_data);
 
  if (!blocked) {
    GST_DEBUG_OBJECT (factory, "unblocked dynamic %s:%s %"GST_PTR_FORMAT,
        GST_DEBUG_PAD_NAME (pad), GST_PAD_CAPS (pad));
    return;
  }

  g_mutex_lock (factory->lock);
  do_dynamic_link (factory, pad);
  g_mutex_unlock (factory->lock);
}

static void
rtspsrc_pad_added_cb_link_dynamic (GstElement *rtspsrc, GstPad *pad,
    GstRTSPRelayMediaFactory *factory)
{
  if (g_strstr_len (GST_PAD_NAME (pad), -1, "recv_rtp_src") == NULL) {
    GST_DEBUG_OBJECT (factory, "ignoring pad %s:%s", GST_DEBUG_PAD_NAME (pad));

    return;
  }

  GST_DEBUG_OBJECT (factory, "got dynamic %s:%s, doing block",
      GST_DEBUG_PAD_NAME (pad));

  gst_pad_set_blocked_async (pad, TRUE,
      rtspsrc_pad_blocked_cb_link_dynamic, factory);
}

static GstElement *
create_payloader_from_pad (GstRTSPRelayMediaFactory *factory,
    GstPad *pad, guint payn)
{
  GstElement *payloader;
  char buf[10];
  GstPad *srcpad = NULL;

  payloader = gst_parse_bin_from_description ("identity", TRUE, NULL);

  g_snprintf (buf, 10, "pay%d", payn);
  gst_element_set_name (payloader, (const char *) &buf);

  srcpad = gst_element_get_static_pad (payloader, "src");
  gst_object_unref (srcpad);

  return payloader;
}

static GstCaps *
get_payloader_caps (GstCaps *caps)
{
  const gchar *encoding_name, *media;
  gint payload = -1;
  GstStructure *structure = gst_caps_get_structure (caps, 0);

  encoding_name = gst_structure_get_string (structure, "encoding-name");
  media = gst_structure_get_string (structure, "media");
  //gst_structure_get_int (structure, "payload", &payload);

  GstCaps *ret = gst_caps_new_simple (gst_structure_get_name (structure),
      "encoding-name", G_TYPE_STRING, encoding_name, NULL);
  if (media)
    gst_caps_set_simple (ret, "media", G_TYPE_STRING, media, NULL);

  if (payload != -1)
    gst_caps_set_simple (ret, "payload", G_TYPE_INT, payload, NULL);

  return ret;
}

static int
create_payloaders_from_element_pads (GstRTSPRelayMediaFactory *factory,
    GstElement *rtspsrc, GstBin *bin)
{
  gboolean done, error;
  GstIterator *iterator;
  GstIteratorResult itres;
  gpointer elem;
  GstPad *pad;
  guint payn;
  guint num_streams = 0;
  GstElement *payloader;
  GstCaps *caps;
  DynamicPayloader *dynamic_payloader;
  GList *walk;

  iterator = gst_element_iterate_src_pads (rtspsrc);

restart:
  if (factory->dynamic_payloaders) {
    g_list_foreach (factory->dynamic_payloaders,
        (GFunc) dynamic_payloader_free, NULL);
    g_list_free (factory->dynamic_payloaders);
  }
  factory->dynamic_payloaders = NULL;
  payn = 0;

  error = FALSE;
  done = FALSE;
  while (!done) {
    itres = gst_iterator_next (iterator, &elem);

    switch (itres) {
      case GST_ITERATOR_DONE:
        done = TRUE;
        break;

      case GST_ITERATOR_ERROR:
        GST_ERROR_OBJECT (factory, "error iterating srcpads");
        done = TRUE;
        error = TRUE;
        break;

      case GST_ITERATOR_RESYNC:
        goto restart;

      case GST_ITERATOR_OK:
        pad = GST_PAD (elem);
        if (g_strstr_len (GST_PAD_NAME (pad), -1, "recv_rtp_src")) {
          payloader = create_payloader_from_pad (factory, pad, payn++);
          caps = get_payloader_caps (GST_PAD_CAPS (pad));
          dynamic_payloader = dynamic_payloader_new (payloader, caps);
          factory->dynamic_payloaders =
              g_list_append (factory->dynamic_payloaders, dynamic_payloader);
        }
        gst_object_unref (pad);
        break;
    }
  }
  gst_iterator_free (iterator);

  if (!factory->dynamic_payloaders)
    return 0;

  num_streams = 0;
  for (walk = factory->dynamic_payloaders; walk != NULL; walk = walk->next) {
    gchar *capss;

    DynamicPayloader *dynamic_payloader = (DynamicPayloader *) walk->data;

    capss = gst_caps_to_string (dynamic_payloader->caps);
    GST_INFO_OBJECT (factory, "created new payloader %s caps %s",
        GST_OBJECT_NAME (dynamic_payloader->payloader), capss);
    g_free (capss);

    payloader = dynamic_payloader->payloader;
    gst_bin_add (bin, payloader);
    num_streams += 1;
  }
  
  return num_streams;
}

static void
rtspsrc_no_more_pads_cb (GstElement *element, gpointer data)
{
  GstRTSPRelayMediaFactory *factory = GST_RTSP_RELAY_MEDIA_FACTORY (data);

  GST_DEBUG_OBJECT (factory, "got no more pads");
  g_mutex_lock (factory->lock);
  factory->rtspsrc_no_more_pads = TRUE;
  g_cond_signal (factory->rtspsrc_no_more_pads_cond);
  g_mutex_unlock (factory->lock);
}

static guint
do_find_dynamic_streams (GstRTSPRelayMediaFactory *factory, GstBin *bin,
    GstElement *rtspsrc)
{
  GstPipeline *pipeline;
  GTimeVal cond_timeout;
  gint num_streams = 0;

  GST_INFO_OBJECT (factory, "finding dynamic streams");

  g_get_current_time (&cond_timeout);
  g_time_val_add (&cond_timeout,
      DYNAMIC_STREAMS_TIMEOUT * G_USEC_PER_SEC);

  g_object_connect (G_OBJECT (rtspsrc),
      "signal::pad-added", G_CALLBACK (rtspsrc_pad_added_cb_block), factory,
      "signal::no-more-pads", G_CALLBACK (rtspsrc_no_more_pads_cb), factory,
      NULL);

  /* set rtspsrc to PLAYING to find the streams */
  pipeline = GST_PIPELINE (gst_pipeline_new (NULL));
  gst_object_ref (bin);
  gst_object_sink (bin);
  gst_bin_add (GST_BIN (pipeline), GST_ELEMENT (bin));
  factory->rtspsrc_no_more_pads = FALSE;
  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PLAYING);

  /* wait for no-more-pads from rtspsrc */
  g_mutex_lock (factory->lock);
  while (TRUE) {
    if (factory->unblocked_pads == 0 && factory->rtspsrc_no_more_pads)
      break;

    if (!g_cond_timed_wait (factory->rtspsrc_no_more_pads_cond,
        factory->lock, &cond_timeout))
      break;
  }

  if (factory->rtspsrc_no_more_pads == FALSE) {
    GST_ERROR_OBJECT (factory, "timeout waiting for no more pads");
    g_mutex_unlock (factory->lock);

    goto out;
  }

  /* create the payloaders based on the pads created by rtspsrc */
  num_streams = create_payloaders_from_element_pads (factory, rtspsrc, bin);
  g_mutex_unlock (factory->lock);

  /* shut down the pipeline */
  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_NULL);

out:
  g_object_disconnect (G_OBJECT (rtspsrc),
      "any_signal::pad-added", G_CALLBACK (rtspsrc_pad_added_cb_block), factory,
      "any_signal::no-more-pads", G_CALLBACK (rtspsrc_no_more_pads_cb), factory,
      NULL);

  gst_bin_remove (GST_BIN (pipeline), GST_ELEMENT (bin));
  gst_object_unref (pipeline);
  //gst_object_unref (bin);

  /* connect to pad-added again to link dynamic payloaders */
  g_object_connect (G_OBJECT (rtspsrc),
      "signal::pad-added", G_CALLBACK (rtspsrc_pad_added_cb_link_dynamic), factory,
      NULL);

  return num_streams;
}

static GstElement *
gst_rtsp_relay_media_factory_get_element (GstRTSPMediaFactory *media_factory,
    const GstRTSPUrl *url)
{
  GstBin *bin;
  GstElement *rtspsrc;
  guint num_streams;
  GstRTSPRelayMediaFactory *factory = GST_RTSP_RELAY_MEDIA_FACTORY (media_factory);

  GST_INFO_OBJECT (factory, "creating element");

  bin = GST_BIN (gst_bin_new (NULL));
  rtspsrc = gst_element_factory_make ("rtspsrc", NULL);
  g_object_set (G_OBJECT (rtspsrc), "location", factory->location, NULL);

  gst_bin_add (bin, GST_ELEMENT (rtspsrc));

  if (factory->find_dynamic_streams)
    num_streams = do_find_dynamic_streams (factory, bin, rtspsrc);
  else
    g_assert_not_reached ();

  if (num_streams == 0) {
    GST_WARNING_OBJECT (factory, "no streams found");

    gst_object_unref (bin);
    return NULL;
  }

  GST_INFO_OBJECT (factory, "created bin %s, %d streams",
      gst_object_get_name (GST_OBJECT (bin)), num_streams);

  return GST_ELEMENT (bin);
}

