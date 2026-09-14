#pragma once

#include <QString>
#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/time.h>
#include <libavutil/audio_fifo.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

namespace ffmpeg_transform {

// 错误码转换为可读字符串
QString ffmpegErrorToString(int errNum);

// RAII 资源释放器
struct AvFormatInputDeleter {
    void operator()(AVFormatContext *ctx) const {
        if (ctx) {
            avformat_close_input(&ctx);
        }
    }
};
using UniqueAvFormatInput = std::unique_ptr<AVFormatContext, AvFormatInputDeleter>;

struct AvFormatOutputDeleter {
    void operator()(AVFormatContext *ctx) const {
        if (ctx) {
            if (ctx->pb && !(ctx->oformat->flags & AVFMT_NOFILE)) {
                avio_closep(&ctx->pb);
            }
            avformat_free_context(ctx);
        }
    }
};
using UniqueAvFormatOutput = std::unique_ptr<AVFormatContext, AvFormatOutputDeleter>;

struct AvCodecContextDeleter {
    void operator()(AVCodecContext *ctx) const {
        if (ctx) {
            avcodec_free_context(&ctx);
        }
    }
};
using UniqueAvCodecContext = std::unique_ptr<AVCodecContext, AvCodecContextDeleter>;

struct AvPacketDeleter {
    void operator()(AVPacket *pkt) const {
        if (pkt) {
            av_packet_free(&pkt);
        }
    }
};
using UniqueAvPacket = std::unique_ptr<AVPacket, AvPacketDeleter>;

struct AvFrameDeleter {
    void operator()(AVFrame *frame) const {
        if (frame) {
            av_frame_free(&frame);
        }
    }
};
using UniqueAvFrame = std::unique_ptr<AVFrame, AvFrameDeleter>;

struct SwsContextDeleter {
    void operator()(SwsContext *sws) const {
        if (sws) {
            sws_freeContext(sws);
        }
    }
};
using UniqueSwsContext = std::unique_ptr<SwsContext, SwsContextDeleter>;

struct SwrContextDeleter {
    void operator()(SwrContext *swr) const {
        if (swr) {
            swr_free(&swr);
        }
    }
};
using UniqueSwrContext = std::unique_ptr<SwrContext, SwrContextDeleter>;

struct AvAudioFifoDeleter {
    void operator()(AVAudioFifo *fifo) const {
        if (fifo) {
            av_audio_fifo_free(fifo);
        }
    }
};
using UniqueAvAudioFifo = std::unique_ptr<AVAudioFifo, AvAudioFifoDeleter>;

} // namespace ffmpeg_transform
