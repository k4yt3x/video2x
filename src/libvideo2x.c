#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/rational.h>

int process_video(
    const char *input_filename,
    const char *output_filename,
    const char *shader_path,
    int output_width,
    int output_height
) {
    AVFormatContext *fmt_ctx = NULL;
    AVFormatContext *ofmt_ctx = NULL;
    AVCodecContext *dec_ctx = NULL;
    AVCodecContext *enc_ctx = NULL;
    AVFilterGraph *filter_graph = NULL;
    AVFilterContext *buffersrc_ctx = NULL;
    AVFilterContext *buffersink_ctx = NULL;
    AVFilterContext *libplacebo_ctx = NULL;
    int video_stream_index = -1;
    AVStream *video_stream = NULL;
    AVStream *out_stream = NULL;
    int ret;
#ifdef DEBUG
    av_log_set_level(AV_LOG_DEBUG);
#endif

    // Open input file
    if ((ret = avformat_open_input(&fmt_ctx, input_filename, NULL, NULL)) < 0) {
        fprintf(stderr, "Could not open input file '%s'\n", input_filename);
        exit(1);
    }

    if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
        fprintf(stderr, "Failed to retrieve input stream information\n");
        exit(1);
    }

    // Find the first video stream
    ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not find video stream in the input, aborting\n");
        exit(1);
    } else {
        video_stream_index = ret;
        video_stream = fmt_ctx->streams[video_stream_index];
    }

    // Set up the decoder
    const AVCodec *dec = avcodec_find_decoder(video_stream->codecpar->codec_id);
    if (!dec) {
        fprintf(stderr, "Failed to find decoder for stream #%u\n", video_stream_index);
        exit(1);
    }

    dec_ctx = avcodec_alloc_context3(dec);
    if (!dec_ctx) {
        fprintf(stderr, "Failed to allocate the decoder context\n");
        exit(1);
    }

    avcodec_parameters_to_context(dec_ctx, video_stream->codecpar);

    // Set decoder time base and frame rate
    dec_ctx->time_base = video_stream->time_base;
    dec_ctx->pkt_timebase = video_stream->time_base;
    AVRational frame_rate = av_guess_frame_rate(fmt_ctx, video_stream, NULL);
    dec_ctx->framerate = frame_rate;

    if ((ret = avcodec_open2(dec_ctx, dec, NULL)) < 0) {
        fprintf(stderr, "Failed to open decoder for stream #%u\n", video_stream_index);
        exit(1);
    }

    // Set up the output file
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, output_filename);
    if (!ofmt_ctx) {
        fprintf(stderr, "Could not create output context\n");
        exit(1);
    }

    // Create a new video stream
    const AVCodec *enc = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!enc) {
        fprintf(stderr, "Necessary encoder not found\n");
        exit(1);
    }

    out_stream = avformat_new_stream(ofmt_ctx, NULL);
    if (!out_stream) {
        fprintf(stderr, "Failed allocating output stream\n");
        exit(1);
    }

    enc_ctx = avcodec_alloc_context3(enc);
    if (!enc_ctx) {
        fprintf(stderr, "Failed to allocate the encoder context\n");
        exit(1);
    }

    enc_ctx->height = output_height;
    enc_ctx->width = output_width;
    enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
    enc_ctx->pix_fmt = enc->pix_fmts[0];
    enc_ctx->time_base = av_inv_q(dec_ctx->framerate);

    if (enc_ctx->time_base.num == 0 || enc_ctx->time_base.den == 0) {
        enc_ctx->time_base = av_inv_q(av_guess_frame_rate(fmt_ctx, video_stream, NULL));
    }

    // Set the bit rate and other encoder parameters if needed
    enc_ctx->bit_rate = 2 * 1000 * 1000;  // 2 Mbps
    enc_ctx->gop_size = 60;               // Keyframe interval
    enc_ctx->max_b_frames = 3;            // B-frames
    enc_ctx->keyint_min = 60;             // Maximum GOP size

    if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if ((ret = avcodec_open2(enc_ctx, enc, NULL)) < 0) {
        fprintf(stderr, "Cannot open video encoder for stream #%u\n", video_stream_index);
        exit(1);
    }

    ret = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
    if (ret < 0) {
        fprintf(
            stderr, "Failed to copy encoder parameters to output stream #%u\n", video_stream_index
        );
        exit(1);
    }

    out_stream->time_base = enc_ctx->time_base;

    // Open the output file
    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, output_filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            fprintf(stderr, "Could not open output file '%s'\n", output_filename);
            exit(1);
        }
    }

    // Initialize the filter graph
    char args[512];
    filter_graph = avfilter_graph_alloc();
    if (!filter_graph) {
        fprintf(stderr, "Unable to create filter graph.\n");
        exit(1);
    }

    // Create buffer source with additional color properties
    const AVFilter *buffersrc = avfilter_get_by_name("buffer");
    snprintf(
        args,
        sizeof(args),
        "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:frame_rate=%d/%d:"
        "pixel_aspect=%d/%d:colorspace=%d",
        dec_ctx->width,
        dec_ctx->height,
        dec_ctx->pix_fmt,
        dec_ctx->time_base.num,
        dec_ctx->time_base.den,
        dec_ctx->framerate.num,
        dec_ctx->framerate.den,
        dec_ctx->sample_aspect_ratio.num,
        dec_ctx->sample_aspect_ratio.den,
        dec_ctx->colorspace
    );

    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args, NULL, filter_graph);
    if (ret < 0) {
        fprintf(stderr, "Cannot create buffer source\n");
        exit(1);
    }

    // Create buffer sink
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    ret =
        avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", NULL, NULL, filter_graph);
    if (ret < 0) {
        fprintf(stderr, "Cannot create buffer sink\n");
        exit(1);
    }

    // Create libplacebo filter with output width and height
    const AVFilter *libplacebo_filter = avfilter_get_by_name("libplacebo");
    if (!libplacebo_filter) {
        fprintf(stderr, "Could not find the libplacebo filter\n");
        exit(1);
    }

    snprintf(
        args,
        sizeof(args),
        "w=%d:h=%d:upscaler=ewa_lanczos:custom_shader_path='%s'",
        output_width,
        output_height,
        shader_path
    );

    ret = avfilter_graph_create_filter(
        &libplacebo_ctx, libplacebo_filter, "libplacebo", args, NULL, filter_graph
    );
    if (ret < 0) {
        fprintf(stderr, "Cannot create libplacebo filter\n");
        exit(1);
    }

    // Link the filters
    ret = avfilter_link(buffersrc_ctx, 0, libplacebo_ctx, 0);
    if (ret >= 0) {
        ret = avfilter_link(libplacebo_ctx, 0, buffersink_ctx, 0);
    }
    if (ret < 0) {
        fprintf(stderr, "Error connecting filters\n");
        exit(1);
    }

    // Configure the graph
    ret = avfilter_graph_config(filter_graph, NULL);
    if (ret < 0) {
        fprintf(stderr, "Error configuring the filter graph\n");
        exit(1);
    }

    // Write the output file header
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Error occurred when opening output file\n");
        exit(1);
    }

    // Processing loop
    AVPacket packet;
    AVFrame *frame = av_frame_alloc();
    AVFrame *filt_frame = av_frame_alloc();

    while (av_read_frame(fmt_ctx, &packet) >= 0) {
        if (packet.stream_index == video_stream_index) {
            ret = avcodec_send_packet(dec_ctx, &packet);
            if (ret < 0) {
                fprintf(stderr, "Error sending packet to decoder\n");
                break;
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(dec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    fprintf(stderr, "Error decoding video frame\n");
                    goto end;
                }

                // Set the frame PTS
                if (frame->pts == AV_NOPTS_VALUE) {
                    frame->pts =
                        av_rescale_q(packet.pts, video_stream->time_base, dec_ctx->time_base);
                }

                // Feed the frame to the filter graph
                if (av_buffersrc_add_frame(buffersrc_ctx, frame) < 0) {
                    fprintf(stderr, "Error while feeding the filter graph\n");
                    break;
                }

                // Get the filtered frame
                while (1) {
                    ret = av_buffersink_get_frame(buffersink_ctx, filt_frame);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                        break;
                    }
                    if (ret < 0) {
                        goto end;
                    }

                    // Rescale PTS to encoder's time base
                    filt_frame->pts = av_rescale_q(
                        filt_frame->pts, buffersink_ctx->inputs[0]->time_base, enc_ctx->time_base
                    );

                    // Encode the filtered frame
                    ret = avcodec_send_frame(enc_ctx, filt_frame);
                    if (ret < 0) {
                        fprintf(stderr, "Error sending frame to encoder\n");
                        av_frame_unref(filt_frame);
                        goto end;
                    }

                    av_frame_unref(filt_frame);

                    while (ret >= 0) {
                        AVPacket *enc_pkt = av_packet_alloc();
                        enc_pkt->data = NULL;
                        enc_pkt->size = 0;

                        ret = avcodec_receive_packet(enc_ctx, enc_pkt);
                        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                            av_packet_unref(enc_pkt);
                            break;
                        } else if (ret < 0) {
                            fprintf(stderr, "Error during encoding\n");
                            av_packet_unref(enc_pkt);
                            goto end;
                        }

                        enc_pkt->stream_index = out_stream->index;

                        // Rescale packet timestamps to output stream time base
                        av_packet_rescale_ts(enc_pkt, enc_ctx->time_base, out_stream->time_base);

                        ret = av_interleaved_write_frame(ofmt_ctx, enc_pkt);
                        av_packet_unref(enc_pkt);
                        if (ret < 0) {
                            fprintf(stderr, "Error muxing packet\n");
                            goto end;
                        }
                    }
                }
                av_frame_unref(frame);
            }
        }
        av_packet_unref(&packet);
    }

    // Flush the decoder and filter graph
    ret = avcodec_send_packet(dec_ctx, NULL);
    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
            break;
        } else if (ret < 0) {
            fprintf(stderr, "Error during decoding\n");
            goto end;
        }

        if (frame->pts == AV_NOPTS_VALUE) {
            frame->pts = frame->best_effort_timestamp;
        }

        if (av_buffersrc_add_frame(buffersrc_ctx, frame) < 0) {
            fprintf(stderr, "Error while feeding the filter graph\n");
            break;
        }

        while (1) {
            ret = av_buffersink_get_frame(buffersink_ctx, filt_frame);
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                break;
            }
            if (ret < 0) {
                fprintf(stderr, "Error during filtering\n");
                goto end;
            }

            filt_frame->pts = av_rescale_q(
                filt_frame->pts, buffersink_ctx->inputs[0]->time_base, enc_ctx->time_base
            );

            ret = avcodec_send_frame(enc_ctx, filt_frame);
            av_frame_unref(filt_frame);
            if (ret < 0) {
                fprintf(stderr, "Error sending frame to encoder\n");
                goto end;
            }

            while (ret >= 0) {
                AVPacket *enc_pkt = av_packet_alloc();
                enc_pkt->data = NULL;
                enc_pkt->size = 0;

                ret = avcodec_receive_packet(enc_ctx, enc_pkt);
                if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                    av_packet_unref(enc_pkt);
                    break;
                } else if (ret < 0) {
                    fprintf(stderr, "Error during encoding\n");
                    av_packet_unref(enc_pkt);
                    goto end;
                }

                enc_pkt->stream_index = out_stream->index;

                // Rescale packet timestamps to output stream time base
                av_packet_rescale_ts(enc_pkt, enc_ctx->time_base, out_stream->time_base);

                ret = av_interleaved_write_frame(ofmt_ctx, enc_pkt);
                av_packet_unref(enc_pkt);
                if (ret < 0) {
                    fprintf(stderr, "Error muxing packet\n");
                    goto end;
                }
            }
        }
        av_frame_unref(frame);
    }

    // Flush the encoder
    ret = avcodec_send_frame(enc_ctx, NULL);
    while (ret >= 0) {
        AVPacket *enc_pkt = av_packet_alloc();
        enc_pkt->data = NULL;
        enc_pkt->size = 0;
        ret = avcodec_receive_packet(enc_ctx, enc_pkt);
        if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
            av_packet_unref(enc_pkt);
            break;
        } else if (ret < 0) {
            fprintf(stderr, "Error during encoding\n");
            av_packet_unref(enc_pkt);
            goto end;
        }

        enc_pkt->stream_index = out_stream->index;

        // Rescale packet timestamps to output stream time base
        av_packet_rescale_ts(enc_pkt, enc_ctx->time_base, out_stream->time_base);

        ret = av_interleaved_write_frame(ofmt_ctx, enc_pkt);
        av_packet_unref(enc_pkt);
        if (ret < 0) {
            fprintf(stderr, "Error muxing packet\n");
            goto end;
        }
    }

    av_write_trailer(ofmt_ctx);

end:
    av_frame_free(&frame);
    av_frame_free(&filt_frame);
    avfilter_graph_free(&filter_graph);
    if (dec_ctx) {
        avcodec_free_context(&dec_ctx);
    }
    if (enc_ctx) {
        avcodec_free_context(&enc_ctx);
    }
    avformat_close_input(&fmt_ctx);
    if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&ofmt_ctx->pb);
    }
    avformat_free_context(ofmt_ctx);

    if (ret < 0 && ret != AVERROR_EOF) {
        fprintf(stderr, "Error occurred: %s\n", av_err2str(ret));
        return 1;
    }

    return 0;
}
