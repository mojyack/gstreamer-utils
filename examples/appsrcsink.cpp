#include <vector>

#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <gst/video/gstvideodecoder.h>

#include "gstutil/auto-gst-object.hpp"
#include "gstutil/pipeline-helper.hpp"
#include "macros/autoptr.hpp"
#include "macros/unwrap.hpp"

namespace {
declare_autoptr(GstBuffer, GstBuffer, gst_buffer_unref);
declare_autoptr(GstCaps, GstCaps, gst_caps_unref);
declare_autoptr(GstSample, GstSample, gst_sample_unref);

auto appsrc   = (GstElement*)(nullptr);
auto caps_str = R"caps(application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, packetization-mode=(string)1, sprop-parameter-sets=(string)"Z/QADZGbKCg/YC1BgYGQAAADABAAAAMDyPFCmWA\=\,aOvsRIRA", profile-level-id=(string)f4000d, profile=(string)high-4:4:4, payload=(int)96, ssrc=(uint)3758284032, timestamp-offset=(uint)417728509, seqnum-offset=(uint)23687, a-framerate=(string)30)caps";

auto on_new_sample(GstAppSink* const appsink, const gpointer /*data*/) -> GstFlowReturn {
    auto payload = std::vector<std::byte>();
    // pull
    {
        auto sample = AutoGstSample(gst_app_sink_pull_sample(appsink));

        static auto first = true;
        if(first) {
            auto caps = AutoGstCaps(gst_sample_get_caps(sample.get()));
            PRINT("stream format: {}", gst_caps_serialize(caps.get(), GST_SERIALIZE_FLAG_NONE));
            first = false;
        }
        auto buffer = gst_sample_get_buffer(sample.get());
        auto info   = GstMapInfo();
        gst_buffer_map(buffer, &info, GST_MAP_READ);
        payload.resize(info.size);
        memcpy(payload.data(), info.data, info.size);
        gst_buffer_unmap(buffer, &info);
    }
    PRINT("pulled {} bytes", payload.size());
    // push
    {
        auto buffer = AutoGstBuffer(gst_buffer_new_allocate(NULL, payload.size(), NULL));
        auto info   = GstMapInfo();
        gst_buffer_map(buffer.get(), &info, GST_MAP_WRITE);
        payload.resize(info.size);
        memcpy(info.data, payload.data(), payload.size());
        gst_buffer_unmap(buffer.get(), &info);

        gst_app_src_push_buffer(GST_APP_SRC(appsrc), buffer.release());
    }

    return GST_FLOW_OK;
}
} // namespace

auto main(int argc, char* argv[]) -> int {
    gst_init(&argc, &argv);

    const auto pipeline = AutoGstObject(gst_pipeline_new(NULL));
    ensure(pipeline.get() != NULL);

    // sender
    unwrap_mut(videotestsrc, add_new_element_to_pipeine(pipeline.get(), "videotestsrc"));
    g_object_set(&videotestsrc, "is-live", TRUE, NULL);
    unwrap_mut(x264enc, add_new_element_to_pipeine(pipeline.get(), "x264enc"));
    unwrap_mut(rtph264pay, add_new_element_to_pipeine(pipeline.get(), "rtph264pay"));
    unwrap_mut(appsink, add_new_element_to_pipeine(pipeline.get(), "appsink"));
    g_object_set(&appsink, "emit-signals", TRUE, NULL);
    ensure(g_signal_connect(&appsink, "new-sample", G_CALLBACK(on_new_sample), NULL) > 0);
    g_object_set(&appsink, "async", FALSE, NULL);

    ensure(gst_element_link_pads(&videotestsrc, NULL, &x264enc, NULL) == TRUE);
    ensure(gst_element_link_pads(&x264enc, NULL, &rtph264pay, NULL) == TRUE);
    ensure(gst_element_link_pads(&rtph264pay, NULL, &appsink, NULL) == TRUE);

    // receiver
    auto payload_caps = AutoGstCaps(gst_caps_from_string(caps_str));
    ensure(payload_caps.get() != NULL);

    unwrap_mut(appsrc, add_new_element_to_pipeine(pipeline.get(), "appsrc"));
    g_object_set(&appsrc, "format", GST_FORMAT_TIME, NULL);
    unwrap_mut(rtph264depay, add_new_element_to_pipeine(pipeline.get(), "rtph264depay"));
    unwrap_mut(avdec_h264, add_new_element_to_pipeine(pipeline.get(), "avdec_h264"));
    unwrap_mut(videoconvert, add_new_element_to_pipeine(pipeline.get(), "videoconvert"));
    unwrap_mut(waylandsink, add_new_element_to_pipeine(pipeline.get(), "waylandsink"));
    g_object_set(&waylandsink, "async", FALSE, NULL);
    g_object_set(&waylandsink, "sync", FALSE, NULL);
    ensure(gst_element_link_pads_filtered(&appsrc, NULL, &rtph264depay, NULL, payload_caps.get()) == TRUE);
    ensure(gst_element_link_pads(&rtph264depay, NULL, &avdec_h264, NULL) == TRUE);
    ensure(gst_element_link_pads(&avdec_h264, NULL, &videoconvert, NULL) == TRUE);
    ensure(gst_element_link_pads(&videoconvert, NULL, &waylandsink, NULL) == TRUE);

    ::appsrc = &appsrc;

    ensure(run_pipeline(pipeline.get()));

    return 0;
}
