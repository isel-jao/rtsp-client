#include <iostream>
#include <string>
#include <thread>
#include <filesystem>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>
}

#include "cpp-httplib/httplib.h"

class RTSPtoHLS
{
private:
    std::string rtsp_url;
    std::string output_dir;
    std::string playlist_name;
    bool running;
    std::thread stream_thread;

public:
    RTSPtoHLS(const std::string &url, const std::string &out_dir = "hls",
              const std::string &playlist = "playlist.m3u8")
        : rtsp_url(url), output_dir(out_dir), playlist_name(playlist), running(false)
    {

        // Create output directory if it doesn't exist
        std::filesystem::create_directories(output_dir);
    }

    ~RTSPtoHLS()
    {
        stop();
    }

    bool start()
    {
        if (running)
            return false;
        running = true;
        stream_thread = std::thread(&RTSPtoHLS::stream_loop, this);
        return true;
    }

    void stop()
    {
        running = false;
        if (stream_thread.joinable())
        {
            stream_thread.join();
        }
    }

private:
    void stream_loop()
    {
        AVFormatContext *input_ctx = nullptr;
        AVFormatContext *output_ctx = nullptr;

        try
        {
            // Create input options dictionary
            AVDictionary *opts = nullptr;
            av_dict_set(&opts, "analyzeduration", "10000000", 0); // 10 seconds
            av_dict_set(&opts, "probesize", "10000000", 0);       // 10MB
            av_dict_set(&opts, "rtsp_transport", "tcp", 0);       // Force TCP

            // Open RTSP input
            if (avformat_open_input(&input_ctx, rtsp_url.c_str(), nullptr, &opts) < 0)
            {
                throw std::runtime_error("Could not open input stream");
            }

            av_dict_free(&opts);

            if (avformat_find_stream_info(input_ctx, nullptr) < 0)
            {
                throw std::runtime_error("Could not find stream information");
            }

            // Create output context for HLS
            std::string playlist_path = output_dir + "/" + playlist_name;
            avformat_alloc_output_context2(&output_ctx, nullptr, "hls", playlist_path.c_str());
            if (!output_ctx)
            {
                throw std::runtime_error("Could not create output context");
            }

            // Set HLS specific options
            av_opt_set(output_ctx->priv_data, "hls_time", "2", 0);      // Reduced segment duration
            av_opt_set(output_ctx->priv_data, "hls_list_size", "5", 0); // Keep more segments
            av_opt_set(output_ctx->priv_data, "hls_flags", "delete_segments+append_list", 0);
            av_opt_set(output_ctx->priv_data, "hls_segment_type", "mpegts", 0);
            av_opt_set(output_ctx->priv_data, "hls_segment_filename",
                       (output_dir + "/segment_%03d.ts").c_str(), 0);

            // Copy streams from input to output
            for (unsigned int i = 0; i < input_ctx->nb_streams; i++)
            {
                AVStream *in_stream = input_ctx->streams[i];
                AVStream *out_stream = avformat_new_stream(output_ctx, nullptr);
                if (!out_stream)
                {
                    throw std::runtime_error("Failed to allocate output stream");
                }

                if (avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar) < 0)
                {
                    throw std::runtime_error("Failed to copy codec params");
                }
            }

            // Open output file
            if (!(output_ctx->oformat->flags & AVFMT_NOFILE))
            {
                if (avio_open(&output_ctx->pb, playlist_path.c_str(), AVIO_FLAG_WRITE) < 0)
                {
                    throw std::runtime_error("Could not open output file");
                }
            }

            // Write header
            if (avformat_write_header(output_ctx, nullptr) < 0)
            {
                throw std::runtime_error("Error occurred when writing header");
            }

            // Main loop
            AVPacket packet;
            while (running)
            {
                if (av_read_frame(input_ctx, &packet) < 0)
                {
                    break;
                }

                av_write_frame(output_ctx, &packet);
                av_packet_unref(&packet);
            }

            // Write trailer
            av_write_trailer(output_ctx);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error: " << e.what() << std::endl;
        }

        // Cleanup
        if (output_ctx && !(output_ctx->oformat->flags & AVFMT_NOFILE))
        {
            avio_closep(&output_ctx->pb);
        }
        if (output_ctx)
        {
            avformat_free_context(output_ctx);
        }
        if (input_ctx)
        {
            avformat_close_input(&input_ctx);
        }
    }
};

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <rtsp_url>" << std::endl;
        return 1;
    }

    // Initialize streamer
    RTSPtoHLS streamer(argv[1]);

    // Start HTTP server
    httplib::Server svr;

    // Serve HLS content
    svr.set_mount_point("/hls", "./hls");

    // Start streaming
    if (!streamer.start())
    {
        std::cerr << "Failed to start streaming" << std::endl;
        return 1;
    }

    std::cout << "Streaming started. Access the stream at: http://localhost:8080/hls/playlist.m3u8" << std::endl;

    // Start HTTP server
    svr.listen("localhost", 8080);

    return 0;
}