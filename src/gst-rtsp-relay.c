/*
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/wtfpl/COPYING for more details.
 *
 * Author: Alessandro Decina <alessandro.d@gmail.com>
 */

#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

#include "gst-rtsp-relay-media-factory.h"

static gboolean
timeout (GstRTSPServer *server, gboolean ignored)
{
  GstRTSPSessionPool *pool;

  pool = gst_rtsp_server_get_session_pool (server);
  gst_rtsp_session_pool_cleanup (pool);
  g_object_unref (pool);

  return TRUE;
}

int
main(int argc, char **argv)
{
  GMainLoop *loop;
  GstRTSPServer *server;
  GstRTSPMediaMapping *mapping;
  GstRTSPRelayMediaFactory *factory;

  gst_init (&argc, &argv);

  loop = g_main_loop_new (NULL, FALSE);

  server = gst_rtsp_server_new ();
  gst_rtsp_server_set_port (server, 8555);

  factory = gst_rtsp_relay_media_factory_new ("rtsp://127.0.0.1:8554/test");
  gst_rtsp_media_factory_set_shared (GST_RTSP_MEDIA_FACTORY (factory), TRUE);
  mapping = gst_rtsp_server_get_media_mapping (server);
  gst_rtsp_media_mapping_add_factory (mapping, "/test",
      GST_RTSP_MEDIA_FACTORY (factory));
  g_object_unref (mapping);

  gst_rtsp_server_attach (server, NULL);

  g_timeout_add_seconds (2, (GSourceFunc) timeout, server); 
  /* start serving */
  g_main_loop_run (loop);

  return 0;
}
