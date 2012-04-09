bin_PROGRAMS = gst-rtsp-relay
lib_LTLIBRARIES = libgstrtsprelay.la

libgstrtsprelay_la_SOURCES = \
	gst-rtsp-relay-media-factory.c

libgstrtsprelay_la_CFLAGS = $(GST_CFLAGS) $(GST_RTSP_SERVER_CFLAGS) -fPIC -Wall -Werror
libgstrtsprelay_la_LIBADD = $(GST_LIBS) $(GST_RTSP_SERVER_LIBS) -lgstinterfaces-0.10 -lgstrtsp-0.10
libgstrtsprelay_la_LDFLAGS = -avoid-version -no-undefined -static

gst_rtsp_relay_SOURCES = \
	gst-rtsp-relay.c

gst_rtsp_relay_CFLAGS = $(GST_CFLAGS) $(GST_RTSP_SERVER_CFLAGS) -Wall -Werror
gst_rtsp_relay_LDADD = $(GST_LIBS) $(GST_RTSP_SERVER_LIBS) \
	-lgstinterfaces-0.10 \
	$(builddir)/libgstrtsprelay.la
gst_rtsp_relay_LDFLAGS = -avoid-version -no-undefined -dynamic

noinst_HEADERS = \
	gst-rtsp-relay-media-factory.h
