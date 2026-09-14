#include "ThumbnailExtractor.h"
#include "FFmpegUtils.h"
#include <QDebug>

namespace ffmpeg_transform {

class ThumbnailExtractorPrivate {
public:
    ThumbnailExtractor *q_ptr{nullptr};

    bool doExtract(const QString &filePath,
                   QImage &outImage,
                   QString &errorMsg,
                   double seekRatio,
                   int targetWidth) {
        AVFormatContext *fmtCtxRaw = nullptr;
        QByteArray pathUtf8 = filePath.toUtf8();

        int ret = avformat_open_input(&fmtCtxRaw, pathUtf8.constData(), nullptr, nullptr);
        if (ret < 0) {
            errorMsg = QString("打开视频失败: %1 (错误: %2)").arg(filePath, ffmpegErrorToString(ret));
            return false;
        }
        UniqueAvFormatInput fmtCtx(fmtCtxRaw);

        ret = avformat_find_stream_info(fmtCtx.get(), nullptr);
        if (ret < 0) {
            errorMsg = QString("获取媒体流失败: %1 (错误: %2)").arg(filePath, ffmpegErrorToString(ret));
            return false;
        }

        AVCodec *decoder = nullptr;
        int videoStreamIdx = av_find_best_stream(fmtCtx.get(), AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
        if (videoStreamIdx < 0 || !decoder) {
            errorMsg = "该媒体文件不包含可解码的视频流";
            return false;
        }

        AVStream *videoStream = fmtCtx->streams[videoStreamIdx];
        UniqueAvCodecContext codecCtx(avcodec_alloc_context3(decoder));
        if (!codecCtx) {
            errorMsg = "分配解码器上下文失败";
            return false;
        }

        ret = avcodec_parameters_to_context(codecCtx.get(), videoStream->codecpar);
        if (ret < 0) {
            errorMsg = QString("初始化解码器参数失败: %1").arg(ffmpegErrorToString(ret));
            return false;
        }

        // 多线程解码优化
        codecCtx->thread_count = 0; // 自动多线程

        ret = avcodec_open2(codecCtx.get(), decoder, nullptr);
        if (ret < 0) {
            errorMsg = QString("打开解码器失败: %1").arg(ffmpegErrorToString(ret));
            return false;
        }

        // 定位到指定比例的时间戳 (避开黑屏)
        if (fmtCtx->duration > 0 && seekRatio > 0.0 && seekRatio < 1.0) {
            int64_t seekTargetTime = static_cast<int64_t>(fmtCtx->duration * seekRatio);
            AVRational timeBaseQ{1, AV_TIME_BASE};
            int64_t seekPts = av_rescale_q(seekTargetTime, timeBaseQ, videoStream->time_base);
            av_seek_frame(fmtCtx.get(), videoStreamIdx, seekPts, AVSEEK_FLAG_BACKWARD);
        }

        UniqueAvPacket packet(av_packet_alloc());
        UniqueAvFrame frame(av_frame_alloc());

        bool frameDecoded = false;
        int maxAttempts = 200; // 最多尝试读取 200 个 packet

        while (av_read_frame(fmtCtx.get(), packet.get()) >= 0 && maxAttempts-- > 0) {
            if (packet->stream_index == videoStreamIdx) {
                ret = avcodec_send_packet(codecCtx.get(), packet.get());
                if (ret < 0) {
                    av_packet_unref(packet.get());
                    continue;
                }

                ret = avcodec_receive_frame(codecCtx.get(), frame.get());
                if (ret == 0) {
                    frameDecoded = true;
                    av_packet_unref(packet.get());
                    break;
                }
            }
            av_packet_unref(packet.get());
        }

        // 若向后跳转读取失败，尝试回退到开头重新读取
        if (!frameDecoded) {
            av_seek_frame(fmtCtx.get(), videoStreamIdx, 0, AVSEEK_FLAG_BACKWARD);
            avcodec_flush_buffers(codecCtx.get());
            maxAttempts = 200;
            while (av_read_frame(fmtCtx.get(), packet.get()) >= 0 && maxAttempts-- > 0) {
                if (packet->stream_index == videoStreamIdx) {
                    avcodec_send_packet(codecCtx.get(), packet.get());
                    if (avcodec_receive_frame(codecCtx.get(), frame.get()) == 0) {
                        frameDecoded = true;
                        av_packet_unref(packet.get());
                        break;
                    }
                }
                av_packet_unref(packet.get());
            }
        }

        if (!frameDecoded || frame->width <= 0 || frame->height <= 0) {
            errorMsg = "未能解码出有效视频帧以生成缩略图";
            return false;
        }

        // 计算目标宽高 (保持原始宽高比)
        int dstWidth = targetWidth;
        int dstHeight = static_cast<int>(dstWidth * static_cast<double>(frame->height) / frame->width);
        if (dstHeight <= 0) dstHeight = 1;
        // 保证偶数尺寸
        if (dstWidth % 2 != 0) dstWidth++;
        if (dstHeight % 2 != 0) dstHeight++;

        UniqueSwsContext swsCtx(sws_getContext(
            frame->width, frame->height, static_cast<AVPixelFormat>(frame->format),
            dstWidth, dstHeight, AV_PIX_FMT_RGB24,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        ));

        if (!swsCtx) {
            errorMsg = "创建图像缩放转换上下文 (SwsContext) 失败";
            return false;
        }

        int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, dstWidth, dstHeight, 1);
        auto *rgbBuffer = static_cast<uint8_t *>(av_malloc(numBytes * sizeof(uint8_t)));
        if (!rgbBuffer) {
            errorMsg = "分配 RGB 图像内存失败";
            return false;
        }

        uint8_t *dstData[4] = {rgbBuffer, nullptr, nullptr, nullptr};
        int dstLinesize[4] = {dstWidth * 3, 0, 0, 0};

        sws_scale(swsCtx.get(), frame->data, frame->linesize, 0, frame->height, dstData, dstLinesize);

        QImage tmpImage(rgbBuffer, dstWidth, dstHeight, dstLinesize[0], QImage::Format_RGB888);
        outImage = tmpImage.copy(); // 独立持有内存
        av_free(rgbBuffer);

        return true;
    }
};

ThumbnailExtractor::ThumbnailExtractor(QObject *parent)
    : QObject(parent), d_ptr(std::make_unique<ThumbnailExtractorPrivate>()) {
    Q_D(ThumbnailExtractor);
    d->q_ptr = this;
}

ThumbnailExtractor::~ThumbnailExtractor() = default;

bool ThumbnailExtractor::extractThumbnail(const QString &filePath,
                                         QImage &outImage,
                                         QString &errorMsg,
                                         double seekRatio,
                                         int targetWidth) {
    Q_D(ThumbnailExtractor);
    return d->doExtract(filePath, outImage, errorMsg, seekRatio, targetWidth);
}

} // namespace ffmpeg_transform
