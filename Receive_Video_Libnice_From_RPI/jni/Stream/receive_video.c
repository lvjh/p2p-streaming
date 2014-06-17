#include "stream.h"
#include "../Login/login.h"
#include "gstreamer_utils.h"

void*  _video_receive_main(CustomData *data)
{
	// Create the nice agent
	data->agent = libnice_create_NiceAgent_with_gstreamer ( video_receive_gathering_done,
															data->context);
	// Create a new stream with one component
	data->stream_id = libnice_create_stream_id (data->agent);

	// Init Gstreamer
	_video_receive_init_gstreamer (data->agent,
								   data->stream_id,
								   data);

	// Start gathering candidates
	libnice_start_gather_candidate (data->agent,
									data->stream_id,
									data->context);

	__android_log_print (ANDROID_LOG_INFO, "tutorial-3", "[video] agent = %d", data->agent);
}

void  _video_receive_init_gstreamer (NiceAgent *magent, guint streamID, CustomData *data)
{
	GstElement *pipeline, *nicesrc, *capsfilter, *gstrtpjitterbuffer,
			   *rtph264depay, *h264parse, *decodebin2, *video_view;
	GstBus *bus;
	GstMessage *msg;
	GstStateChangeReturn ret;
	GSource *bus_source;

	// Initialize GStreamer
  	gst_init (NULL, NULL);

  	//Register gstreamer plugin libnice
	gst_plugin_register_static (
		GST_VERSION_MAJOR,
		GST_VERSION_MINOR,
		"nice",
		"Interactive UDP connectivity establishment",
		plugin_init, "0.1.4", "LGPL", "libnice",
		"http://telepathy.freedesktop.org/wiki/", "");

	// Create elements
	nicesrc = gst_element_factory_make ("nicesrc", NULL);
	capsfilter = gst_element_factory_make ("capsfilter", NULL);
	gstrtpjitterbuffer = gst_element_factory_make ("gstrtpjitterbuffer", NULL);
	rtph264depay = gst_element_factory_make ("rtph264depay", NULL);
	h264parse = gst_element_factory_make ("h264parse", NULL);
	decodebin2 = gst_element_factory_make ("decodebin2", NULL);
	video_view = gst_element_factory_make ("autovideosink", NULL);

	// Set element's properties
	g_object_set (nicesrc, "agent", magent, NULL);
	g_object_set (nicesrc, "stream", streamID, NULL);
	g_object_set (nicesrc, "component", 1, NULL);
	g_object_set (capsfilter, "caps",
				  gst_caps_from_string("application/x-rtp,"
				  " payload=(int)96"), NULL);
	g_object_set (video_view, "sync", FALSE, NULL);

	pipeline = gst_pipeline_new("Receive Video Pipeline");

	if (!pipeline || ! nicesrc || !capsfilter || !gstrtpjitterbuffer ||
			!rtph264depay|| !video_view || !decodebin2 || !h264parse)
	{
		g_printerr ("Not all elements could be created.\n");
		return;
	}

	gst_bin_add_many (GST_BIN (pipeline), nicesrc, capsfilter,
			 rtph264depay, h264parse, decodebin2, video_view, NULL);

	if (gst_element_link_many (nicesrc, capsfilter, rtph264depay,
			h264parse, decodebin2, NULL) != TRUE)
	{
		g_printerr ("Elements could not be linked.[01]\n");
		gst_object_unref (pipeline);
		return;
	}

	g_signal_connect (decodebin2, "pad_added", G_CALLBACK (on_pad_added), video_view);

	data->pipeline = pipeline;
	gst_element_set_state(data->pipeline, GST_STATE_READY);
	
	data->video_sink = gst_bin_get_by_interface(GST_BIN(data->pipeline),
						GST_TYPE_X_OVERLAY);
	if (!data->video_sink)
	{
		GST_ERROR ("Could not retrieve video sink");
		return;
	}

	bus = gst_element_get_bus (data->pipeline);
	bus_source = gst_bus_create_watch (bus);
	g_source_set_callback (bus_source,
			(GSourceFunc) gst_bus_async_signal_func, NULL, NULL);
	g_source_attach (bus_source, data->context);
	g_source_unref (bus_source);
	g_signal_connect (G_OBJECT (bus), "message::error",
			(GCallback)on_error, data);
	g_signal_connect (G_OBJECT (bus), "message::state-changed",
			(GCallback)on_state_changed, data);
	gst_object_unref (bus);

	//Create a GLib Main Loop and set it to run
	GST_DEBUG ("Entering main loop... (CustomData:%p)", data);
	check_initialization_complete (data);
}
