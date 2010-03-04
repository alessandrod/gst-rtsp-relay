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

#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-media-factory.h>

#ifndef __GST_RTSP_RELAY_MEDIA_FACTORY_H__
#define __GST_RTSP_RELAY_MEDIA_FACTORY_H__

G_BEGIN_DECLS

/* types for the media factory */
#define GST_TYPE_RTSP_RELAY_MEDIA_FACTORY              (gst_rtsp_relay_media_factory_get_type ())
#define GST_IS_RTSP_RELAY_MEDIA_FACTORY(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_RTSP_RELAY_MEDIA_FACTORY))
#define GST_IS_RTSP_RELAY_MEDIA_FACTORY_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_RTSP_RELAY_MEDIA_FACTORY))
#define GST_RTSP_RELAY_MEDIA_FACTORY_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_RTSP_RELAY_MEDIA_FACTORY, GstRTSPRelayMediaFactoryClass))
#define GST_RTSP_RELAY_MEDIA_FACTORY(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_RTSP_RELAY_MEDIA_FACTORY, GstRTSPRelayMediaFactory))
#define GST_RTSP_RELAY_MEDIA_FACTORY_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_RTSP_RELAY_MEDIA_FACTORY, GstRTSPRelayMediaFactoryClass))
#define GST_RTSP_RELAY_MEDIA_FACTORY_CAST(obj)         ((GstRTSPRelayMediaFactory*)(obj))
#define GST_RTSP_RELAY_MEDIA_FACTORY_CLASS_CAST(klass) ((GstRTSPRelayMediaFactoryClass*)(klass))

typedef struct _GstRTSPRelayMediaFactory GstRTSPRelayMediaFactory;
typedef struct _GstRTSPRelayMediaFactoryClass GstRTSPRelayMediaFactoryClass;

struct _GstRTSPRelayMediaFactory {
  GstRTSPMediaFactory factory;

  GMutex *lock;
  gboolean find_dynamic_streams;
  GstClockTime timeout;
  char *location;
  gboolean rtspsrc_no_more_pads;
  GCond *dynamic_pads_cond;
  GList *dynamic_payloaders;
  gint pads_waiting_block;
  gboolean error;
};

struct _GstRTSPRelayMediaFactoryClass {
  GstRTSPMediaFactoryClass klass;
};

GType gst_rtsp_relay_media_factory_get_type (void);

/* creating the factory */
GstRTSPRelayMediaFactory * gst_rtsp_relay_media_factory_new (const char *url);

G_END_DECLS

#endif /* __GST_RTSP_RELAY_MEDIA_FACTORY_H__ */
