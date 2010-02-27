bin_PROGRAMS = gst-rtsp-relay

gst_rtsp_relay_SOURCES = \
	gst-rtsp-relay-media-factory.c \
	gst-rtsp-relay.c

gst_rtsp_relay_CFLAGS = $(GST_CFLAGS) $(GST_RTSP_SERVER_CFLAGS) -Wall -Werror
gst_rtsp_relay_LDADD = $(GST_LIBS) $(GST_RTSP_SERVER_LIBS) -lgstinterfaces-0.10
gst_rtsp_relay_LDFLAGS = -avoid-version -no-undefined -dynamic

noinst_HEADERS = \
	gst-rtsp-relay-media-factory.h
