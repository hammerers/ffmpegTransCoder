#include "TranscodeEngine.h"
#include "FFmpegUtils.h"
#include <QElapsedTimer>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QPainter>
#include <QFont>
#include <QFontMetrics>
#include <QPoint>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <algorithm>

namespace ffmpeg_transform {

// 内存级图像算法：YUV420P 空间去水印 / 区域平滑双线性插值
static void applyDelogoYuv(AVFrame *frame, const DelogoConfig &cfg) {
    if (!frame || !cfg.enabled || cfg.width <= 0 || cfg.height <= 0) return;

    int fw = frame->width;
    int fh = frame->height;
    int rx = std::clamp(cfg.x, 0, fw - 2);
    int ry = std::clamp(cfg.y, 0, fh - 2);
    int rw = std::clamp(cfg.width, 2, fw - rx);
    int rh = std::clamp(cfg.height, 2, fh - ry);

    // 1. 处理 Y 亮度平面
    uint8_t *yData = frame->data[0];
    int yPitch = frame->linesize[0];

    for (int y = ry; y < ry + rh; ++y) {
        float ty = static_cast<float>(y - ry) / static_cast<float>(rh);
        uint8_t topVal = yData[ry * yPitch + rx];
        uint8_t botVal = yData[(ry + rh - 1) * yPitch + rx];

        for (int x = rx; x < rx + rw; ++x) {
            float tx = static_cast<float>(x - rx) / static_cast<float>(rw);
            uint8_t leftVal = yData[y * yPitch + rx];
            uint8_t rightVal = yData[y * yPitch + (rx + rw - 1)];

            float horiz = (1.0f - tx) * leftVal + tx * rightVal;
            float vert = (1.0f - ty) * topVal + ty * botVal;
            yData[y * yPitch + x] = static_cast<uint8_t>((horiz + vert) * 0.5f);
        }
    }

    // 2. 处理 U/V 色度平面 (尺寸减半)
    int rxHalf = rx / 2;
    int ryHalf = ry / 2;
    int rwHalf = std::max(1, rw / 2);
    int rhHalf = std::max(1, rh / 2);

    for (int plane = 1; plane <= 2; ++plane) {
        uint8_t *uvData = frame->data[plane];
        int uvPitch = frame->linesize[plane];
        for (int y = ryHalf; y < ryHalf + rhHalf; ++y) {
            float ty = static_cast<float>(y - ryHalf) / static_cast<float>(rhHalf);
            for (int x = rxHalf; x < rxHalf + rwHalf; ++x) {
                float tx = static_cast<float>(x - rxHalf) / static_cast<float>(rwHalf);
                uint8_t leftVal = uvData[y * uvPitch + rxHalf];
                uint8_t rightVal = uvData[y * uvPitch + (rxHalf + rwHalf - 1)];
                uint8_t topVal = uvData[ryHalf * uvPitch + x];
                uint8_t botVal = uvData[(ryHalf + rhHalf - 1) * uvPitch + x];

                float horiz = (1.0f - tx) * leftVal + tx * rightVal;
                float vert = (1.0f - ty) * topVal + ty * botVal;
                uvData[y * uvPitch + x] = static_cast<uint8_t>((horiz + vert) * 0.5f);
            }
        }
    }
}

// 预光栅化水印 ARGB32 位图并计算目标起始位置
static QImage createWatermarkRaster(const WatermarkConfig &cfg, int frameWidth, int frameHeight, QPoint &outPos) {
    if (!cfg.enabled || frameWidth <= 0 || frameHeight <= 0) return {};

    QImage wm;
    if (cfg.type == WatermarkType::Image) {
        if (cfg.imagePath.isEmpty() || !QFile::exists(cfg.imagePath)) return {};
        wm.load(cfg.imagePath);
        if (wm.isNull()) return {};

        if (std::abs(cfg.scale - 1.0f) > 0.01f && cfg.scale > 0.05f) {
            int nw = std::clamp(static_cast<int>(wm.width() * cfg.scale), 10, frameWidth);
            int nh = std::clamp(static_cast<int>(wm.height() * cfg.scale), 10, frameHeight);
            wm = wm.scaled(nw, nh, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    } else {
        if (cfg.text.isEmpty()) return {};
        QFont font("Segoe UI", std::clamp(cfg.fontSize, 10, 120), QFont::Bold);
        QFontMetrics fm(font);
        QRect textRect = fm.boundingRect(cfg.text);
        int padH = 8;
        int padV = 4;
        int w = textRect.width() + padH * 2;
        int h = textRect.height() + padV * 2;

        wm = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
        wm.fill(Qt::transparent);

        QPainter p(&wm);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);

        p.setBrush(QColor(15, 23, 42, 160));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(0, 0, w, h, 4, 4);

        QColor textCol(cfg.fontColor);
        if (!textCol.isValid()) textCol = Qt::white;
        p.setPen(textCol);
        p.setFont(font);
        p.drawText(wm.rect(), Qt::AlignCenter, cfg.text);
    }

    if (wm.isNull() || wm.width() <= 0 || wm.height() <= 0) return {};
    wm = wm.convertToFormat(QImage::Format_ARGB32);

    int targetX = cfg.x;
    int targetY = cfg.y;

    switch (cfg.position) {
    case WatermarkPosition::TopRight:
        targetX = frameWidth - wm.width() - cfg.x;
        targetY = cfg.y;
        break;
    case WatermarkPosition::TopLeft:
        targetX = cfg.x;
        targetY = cfg.y;
        break;
    case WatermarkPosition::BottomRight:
        targetX = frameWidth - wm.width() - cfg.x;
        targetY = frameHeight - wm.height() - cfg.y;
        break;
    case WatermarkPosition::BottomLeft:
        targetX = cfg.x;
        targetY = frameHeight - wm.height() - cfg.y;
        break;
    case WatermarkPosition::Center:
        targetX = (frameWidth - wm.width()) / 2;
        targetY = (frameHeight - wm.height()) / 2;
        break;
    case WatermarkPosition::Custom:
        targetX = cfg.x;
        targetY = cfg.y;
        break;
    }

    outPos = QPoint(targetX, targetY);
    return wm;
}

// 内存级图像算法：YUV420P 空间透明度 Alpha Blending 融合水印
static void applyWatermarkYuv(AVFrame *frame, const QImage &wm, const QPoint &pos, float globalOpacity) {
    if (!frame || wm.isNull() || globalOpacity <= 0.001f) return;
    if (frame->format != AV_PIX_FMT_YUV420P) return;

    int wmWidth = wm.width();
    int wmHeight = wm.height();

    int startX = std::max(0, pos.x());
    int startY = std::max(0, pos.y());
    int endX = std::min(frame->width, pos.x() + wmWidth);
    int endY = std::min(frame->height, pos.y() + wmHeight);

    if (startX >= endX || startY >= endY) return;

    uint8_t *yData = frame->data[0];
    uint8_t *uData = frame->data[1];
    uint8_t *vData = frame->data[2];
    int yPitch = frame->linesize[0];
    int uPitch = frame->linesize[1];
    int vPitch = frame->linesize[2];

    for (int y = startY; y < endY; ++y) {
        int wy = y - pos.y();
        for (int x = startX; x < endX; ++x) {
            int wx = x - pos.x();
            QRgb pixel = wm.pixel(wx, wy);
            int a = qAlpha(pixel);
            if (a == 0) continue;

            float alpha = (static_cast<float>(a) / 255.0f) * std::clamp(globalOpacity, 0.0f, 1.0f);
            if (alpha <= 0.002f) continue;

            int r = qRed(pixel);
            int g = qGreen(pixel);
            int b = qBlue(pixel);

            // BT.601 RGB 转 YUV 快速整数换算
            uint8_t yVal = static_cast<uint8_t>(std::clamp(((66 * r + 129 * g + 25 * b + 128) >> 8) + 16, 0, 255));
            int yOffset = y * yPitch + x;
            yData[yOffset] = static_cast<uint8_t>((1.0f - alpha) * yData[yOffset] + alpha * yVal);

            if ((y % 2 == 0) && (x % 2 == 0)) {
                uint8_t uVal = static_cast<uint8_t>(std::clamp(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128, 0, 255));
                uint8_t vVal = static_cast<uint8_t>(std::clamp(((112 * r - 94 * g - 18 * b + 128) >> 8) + 128, 0, 255));

                int uOffset = (y / 2) * uPitch + (x / 2);
                int vOffset = (y / 2) * vPitch + (x / 2);
                uData[uOffset] = static_cast<uint8_t>((1.0f - alpha) * uData[uOffset] + alpha * uVal);
                vData[vOffset] = static_cast<uint8_t>((1.0f - alpha) * vData[vOffset] + alpha * vVal);
            }
        }
    }
}

// 快速提取高质量预览 QImage 用于视口实时双分屏渲染 (严格保持真实宽高比与高解析度)
static QImage avFrameToPreviewImage(AVFrame *frame, SwsContext *&cachedSws, int &cachedW, int &cachedH, int maxDim = 1280) {
    if (!frame || frame->width <= 0 || frame->height <= 0) return {};

    int targetW = frame->width;
    int targetH = frame->height;
    if (targetW > maxDim || targetH > maxDim) {
        if (targetW >= targetH) {
            targetH = static_cast<int>(maxDim * static_cast<double>(frame->height) / frame->width);
            targetW = maxDim;
        } else {
            targetW = static_cast<int>(maxDim * static_cast<double>(frame->width) / frame->height);
            targetH = maxDim;
        }
    }
    if (targetW <= 0) targetW = 2;
    if (targetH <= 0) targetH = 2;
    // 保证 16 字节对齐宽度，确保 SIMD 矢量化指令绝对安全且不溢出
    if (targetW % 16 != 0) {
        targetW = ((targetW + 15) / 16) * 16;
    }
    if (targetH % 2 != 0) targetH++;

    if (!cachedSws || cachedW != frame->width || cachedH != frame->height) {
        if (cachedSws) sws_freeContext(cachedSws);
        cachedSws = sws_getContext(
            frame->width, frame->height, static_cast<AVPixelFormat>(frame->format),
            targetW, targetH, AV_PIX_FMT_RGB24,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );
        cachedW = frame->width;
        cachedH = frame->height;
    }

    if (!cachedSws) return {};

    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, targetW, targetH, 32);
    if (numBytes <= 0) return {};

    auto *rgbBuffer = static_cast<uint8_t *>(av_malloc(numBytes + AV_INPUT_BUFFER_PADDING_SIZE));
    if (!rgbBuffer) return {};

    uint8_t *dstData[4] = { rgbBuffer, nullptr, nullptr, nullptr };
    int dstLinesize[4] = { targetW * 3, 0, 0, 0 };

    sws_scale(cachedSws, frame->data, frame->linesize, 0, frame->height, dstData, dstLinesize);

    QImage img(rgbBuffer, targetW, targetH, dstLinesize[0], QImage::Format_RGB888, [](void *ptr) {
        av_free(ptr);
    }, rgbBuffer);

    return img;
}

class TranscodeEnginePrivate {
public:
    TranscodeEngine *q_ptr{nullptr};

    TranscodeConfig currentConfig;
    TranscodeProgress currentProgress;
    std::atomic<TaskState> state{TaskState::Pending};

    std::atomic<bool> isRunning{false};
    std::atomic<bool> isPaused{false};
    std::atomic<bool> isCanceled{false};

    std::mutex pauseMutex;
    std::condition_variable pauseCv;
    std::thread workerThread;

    ~TranscodeEnginePrivate() {
        stopWorker();
    }

    void stopWorker() {
        isCanceled = true;
        isPaused = false;
        pauseCv.notify_all();
        if (workerThread.joinable()) {
            workerThread.join();
        }
    }

    void updateState(TaskState newState) {
        state = newState;
        emit q_ptr->stateChanged(newState);
    }

    void runTranscode(TranscodeConfig cfg) {
        updateState(TaskState::Converting);

        QElapsedTimer timer;
        timer.start();

        // 确保目标文件夹存在
        QFileInfo outFi(cfg.outputPath);
        if (!outFi.absoluteDir().exists()) {
            outFi.absoluteDir().mkpath(".");
        }

        // 1. 打开输入
        AVFormatContext *inFmtRaw = nullptr;
        int ret = avformat_open_input(&inFmtRaw, cfg.inputPath.toUtf8().constData(), nullptr, nullptr);
        if (ret < 0) {
            updateState(TaskState::Failed);
            emit q_ptr->finished(false, QString("无法打开输入文件: %1").arg(ffmpegErrorToString(ret)));
            return;
        }
        UniqueAvFormatInput inFmtCtx(inFmtRaw);

        ret = avformat_find_stream_info(inFmtCtx.get(), nullptr);
        if (ret < 0) {
            updateState(TaskState::Failed);
            emit q_ptr->finished(false, QString("无法获取输入流信息: %1").arg(ffmpegErrorToString(ret)));
            return;
        }

        double totalDuration = inFmtCtx->duration > 0 ? (static_cast<double>(inFmtCtx->duration) / AV_TIME_BASE) : 0.0;

        // 2. 找到音视频流
        AVCodec *inVideoDecoder = nullptr;
        AVCodec *inAudioDecoder = nullptr;
        int inVideoIdx = av_find_best_stream(inFmtCtx.get(), AVMEDIA_TYPE_VIDEO, -1, -1, &inVideoDecoder, 0);
        int inAudioIdx = av_find_best_stream(inFmtCtx.get(), AVMEDIA_TYPE_AUDIO, -1, -1, &inAudioDecoder, 0);

        // 3. 打开输出上下文
        AVFormatContext *outFmtRaw = nullptr;
        ret = avformat_alloc_output_context2(&outFmtRaw, nullptr, nullptr, cfg.outputPath.toUtf8().constData());
        if (ret < 0 || !outFmtRaw) {
            updateState(TaskState::Failed);
            emit q_ptr->finished(false, QString("创建输出上下文失败: %1").arg(ffmpegErrorToString(ret)));
            return;
        }
        UniqueAvFormatOutput outFmtCtx(outFmtRaw);

        // 解码器与编码器上下文
        UniqueAvCodecContext inVideoCodecCtx;
        UniqueAvCodecContext inAudioCodecCtx;
        UniqueAvCodecContext outVideoCodecCtx;
        UniqueAvCodecContext outAudioCodecCtx;

        AVStream *outVideoStream = nullptr;
        AVStream *outAudioStream = nullptr;

        UniqueSwsContext swsCtx;
        UniqueSwrContext swrCtx;
        UniqueAvAudioFifo audioFifo;

        int64_t nextVideoPts = 0;
        int64_t nextAudioPts = 0;

        // --- 配置视频流 ---
        bool hasVideoOutput = (inVideoIdx >= 0 && cfg.videoCodec != VideoCodecType::None);
        bool transcodeVideo = (hasVideoOutput && cfg.videoCodec != VideoCodecType::Copy);

        if (hasVideoOutput) {
            AVStream *inStream = inFmtCtx->streams[inVideoIdx];
            if (!transcodeVideo) {
                // 视频流直接拷贝 (Stream Copy)
                outVideoStream = avformat_new_stream(outFmtCtx.get(), nullptr);
                avcodec_parameters_copy(outVideoStream->codecpar, inStream->codecpar);
                outVideoStream->codecpar->codec_tag = 0;
                outVideoStream->time_base = inStream->time_base;
            } else {
                // 准备解码器
                inVideoCodecCtx.reset(avcodec_alloc_context3(inVideoDecoder));
                avcodec_parameters_to_context(inVideoCodecCtx.get(), inStream->codecpar);
                inVideoCodecCtx->thread_count = 0;
                if (avcodec_open2(inVideoCodecCtx.get(), inVideoDecoder, nullptr) < 0) {
                    updateState(TaskState::Failed);
                    emit q_ptr->finished(false, "打开输入视频解码器失败");
                    return;
                }

                // 选择编码器 (支持 NVENC / QSV 硬件加速及自动回退)
                const AVCodec *outVideoEncoder = nullptr;
                QString usedEncoderName;

                if (cfg.hwAccel == HwAccelMode::Auto || cfg.hwAccel == HwAccelMode::NVENC) {
                    if (cfg.videoCodec == VideoCodecType::H264) {
                        outVideoEncoder = avcodec_find_encoder_by_name("h264_nvenc");
                    } else if (cfg.videoCodec == VideoCodecType::H265) {
                        outVideoEncoder = avcodec_find_encoder_by_name("hevc_nvenc");
                    }
                    if (outVideoEncoder) {
                        usedEncoderName = (cfg.videoCodec == VideoCodecType::H264) ? "h264_nvenc" : "hevc_nvenc";
                    }
                }

                if (!outVideoEncoder && (cfg.hwAccel == HwAccelMode::Auto || cfg.hwAccel == HwAccelMode::QSV)) {
                    if (cfg.videoCodec == VideoCodecType::H264) {
                        outVideoEncoder = avcodec_find_encoder_by_name("h264_qsv");
                    } else if (cfg.videoCodec == VideoCodecType::H265) {
                        outVideoEncoder = avcodec_find_encoder_by_name("hevc_qsv");
                    }
                    if (outVideoEncoder) {
                        usedEncoderName = (cfg.videoCodec == VideoCodecType::H264) ? "h264_qsv" : "hevc_qsv";
                    }
                }

                if (!outVideoEncoder) {
                    if (cfg.videoCodec == VideoCodecType::H264) {
                        outVideoEncoder = avcodec_find_encoder_by_name("libx264");
                        if (!outVideoEncoder) outVideoEncoder = avcodec_find_encoder(AV_CODEC_ID_H264);
                        usedEncoderName = "libx264";
                    } else if (cfg.videoCodec == VideoCodecType::H265) {
                        outVideoEncoder = avcodec_find_encoder_by_name("libx265");
                        if (!outVideoEncoder) outVideoEncoder = avcodec_find_encoder(AV_CODEC_ID_HEVC);
                        usedEncoderName = "libx265";
                    }
                }

                if (!outVideoEncoder) {
                    updateState(TaskState::Failed);
                    emit q_ptr->finished(false, "未找到目标视频编码器");
                    return;
                }

                outVideoCodecCtx.reset(avcodec_alloc_context3(outVideoEncoder));

                // 计算输出分辨率
                int outW = inVideoCodecCtx->width;
                int outH = inVideoCodecCtx->height;
                if (cfg.resolutionScale == ResolutionScale::Scale4K) { outW = 3840; outH = 2160; }
                else if (cfg.resolutionScale == ResolutionScale::Scale1080p) { outW = 1920; outH = 1080; }
                else if (cfg.resolutionScale == ResolutionScale::Scale720p) { outW = 1280; outH = 720; }
                else if (cfg.resolutionScale == ResolutionScale::Scale480p) { outW = 854; outH = 480; }
                else if (cfg.resolutionScale == ResolutionScale::Custom) { outW = cfg.customWidth; outH = cfg.customHeight; }
                // 确保宽高为偶数
                outW = (outW / 2) * 2;
                outH = (outH / 2) * 2;
                if (outW <= 0) outW = 2;
                if (outH <= 0) outH = 2;

                // 计算目标帧率与 time_base
                AVRational targetFps = (inStream->avg_frame_rate.num > 0) ? inStream->avg_frame_rate : inStream->r_frame_rate;
                if (cfg.fpsOption == FpsOption::Fps60) targetFps = {60, 1};
                else if (cfg.fpsOption == FpsOption::Fps30) targetFps = {30, 1};
                else if (cfg.fpsOption == FpsOption::Fps24) targetFps = {24, 1};
                else if (cfg.fpsOption == FpsOption::Custom) targetFps = {static_cast<int>(cfg.customFps * 1000), 1000};
                if (targetFps.num <= 0 || targetFps.den <= 0) targetFps = {25, 1};

                outVideoCodecCtx->width = outW;
                outVideoCodecCtx->height = outH;
                outVideoCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
                outVideoCodecCtx->time_base = av_inv_q(targetFps);
                outVideoCodecCtx->framerate = targetFps;
                outVideoCodecCtx->gop_size = 12;
                outVideoCodecCtx->max_b_frames = 2;

                if (cfg.qualityMode == QualityMode::CRF) {
                    av_opt_set(outVideoCodecCtx->priv_data, "crf", std::to_string(cfg.crf).c_str(), 0);
                } else {
                    outVideoCodecCtx->bit_rate = cfg.videoBitrate;
                }

                av_opt_set(outVideoCodecCtx->priv_data, "preset", cfg.preset.toUtf8().constData(), 0);
                outVideoCodecCtx->thread_count = (cfg.threads > 0) ? cfg.threads : 0;

                if (outFmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
                    outVideoCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
                }

                ret = avcodec_open2(outVideoCodecCtx.get(), outVideoEncoder, nullptr);
                if (ret < 0 && (usedEncoderName.contains("nvenc") || usedEncoderName.contains("qsv"))) {
                    // 硬件编码器不可用时，无缝回退至 CPU 软件编码器
                    const AVCodec *fallbackEncoder = (cfg.videoCodec == VideoCodecType::H264)
                        ? (avcodec_find_encoder_by_name("libx264") ? avcodec_find_encoder_by_name("libx264") : avcodec_find_encoder(AV_CODEC_ID_H264))
                        : (avcodec_find_encoder_by_name("libx265") ? avcodec_find_encoder_by_name("libx265") : avcodec_find_encoder(AV_CODEC_ID_HEVC));
                    if (fallbackEncoder) {
                        outVideoCodecCtx.reset(avcodec_alloc_context3(fallbackEncoder));
                        outVideoCodecCtx->width = outW;
                        outVideoCodecCtx->height = outH;
                        outVideoCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
                        outVideoCodecCtx->time_base = av_inv_q(targetFps);
                        outVideoCodecCtx->framerate = targetFps;
                        outVideoCodecCtx->gop_size = 12;
                        outVideoCodecCtx->max_b_frames = 2;
                        if (cfg.qualityMode == QualityMode::CRF) {
                            av_opt_set(outVideoCodecCtx->priv_data, "crf", std::to_string(cfg.crf).c_str(), 0);
                        } else {
                            outVideoCodecCtx->bit_rate = cfg.videoBitrate;
                        }
                        av_opt_set(outVideoCodecCtx->priv_data, "preset", cfg.preset.toUtf8().constData(), 0);
                        outVideoCodecCtx->thread_count = (cfg.threads > 0) ? cfg.threads : 0;
                        if (outFmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
                            outVideoCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
                        }
                        ret = avcodec_open2(outVideoCodecCtx.get(), fallbackEncoder, nullptr);
                    }
                }
                if (ret < 0) {
                    updateState(TaskState::Failed);
                    emit q_ptr->finished(false, QString("打开输出视频编码器失败: %1").arg(ffmpegErrorToString(ret)));
                    return;
                }

                outVideoStream = avformat_new_stream(outFmtCtx.get(), nullptr);
                avcodec_parameters_from_context(outVideoStream->codecpar, outVideoCodecCtx.get());
                outVideoStream->time_base = outVideoCodecCtx->time_base;

                swsCtx.reset(sws_getContext(
                    inVideoCodecCtx->width, inVideoCodecCtx->height, inVideoCodecCtx->pix_fmt,
                    outW, outH, AV_PIX_FMT_YUV420P,
                    SWS_BILINEAR, nullptr, nullptr, nullptr
                ));
            }
        }

        // --- 配置音频流 ---
        bool hasAudioOutput = (inAudioIdx >= 0 && cfg.audioCodec != AudioCodecType::None);
        bool transcodeAudio = (hasAudioOutput && cfg.audioCodec != AudioCodecType::Copy);

        if (hasAudioOutput) {
            AVStream *inStream = inFmtCtx->streams[inAudioIdx];
            if (!transcodeAudio) {
                // 音频直接拷贝 (Stream Copy)
                outAudioStream = avformat_new_stream(outFmtCtx.get(), nullptr);
                avcodec_parameters_copy(outAudioStream->codecpar, inStream->codecpar);
                outAudioStream->codecpar->codec_tag = 0;
                outAudioStream->time_base = inStream->time_base;
            } else {
                inAudioCodecCtx.reset(avcodec_alloc_context3(inAudioDecoder));
                avcodec_parameters_to_context(inAudioCodecCtx.get(), inStream->codecpar);
                if (avcodec_open2(inAudioCodecCtx.get(), inAudioDecoder, nullptr) < 0) {
                    updateState(TaskState::Failed);
                    emit q_ptr->finished(false, "打开输入音频解码器失败");
                    return;
                }

                const AVCodec *outAudioEncoder = nullptr;
                if (cfg.audioCodec == AudioCodecType::AAC) {
                    outAudioEncoder = avcodec_find_encoder(AV_CODEC_ID_AAC);
                } else if (cfg.audioCodec == AudioCodecType::MP3) {
                    outAudioEncoder = avcodec_find_encoder(AV_CODEC_ID_MP3);
                }

                if (!outAudioEncoder) {
                    updateState(TaskState::Failed);
                    emit q_ptr->finished(false, "未找到目标音频编码器");
                    return;
                }

                outAudioCodecCtx.reset(avcodec_alloc_context3(outAudioEncoder));
                outAudioCodecCtx->sample_rate = cfg.audioSampleRate > 0 ? cfg.audioSampleRate : 44100;
                outAudioCodecCtx->channels = 2;
                outAudioCodecCtx->channel_layout = AV_CH_LAYOUT_STEREO;

                // 选择支持的采样格式
                AVSampleFormat targetSampleFmt = AV_SAMPLE_FMT_FLTP;
                if (outAudioEncoder->sample_fmts) {
                    targetSampleFmt = outAudioEncoder->sample_fmts[0];
                }
                outAudioCodecCtx->sample_fmt = targetSampleFmt;
                outAudioCodecCtx->bit_rate = cfg.audioBitrate > 0 ? cfg.audioBitrate : 192000;
                outAudioCodecCtx->time_base = {1, outAudioCodecCtx->sample_rate};

                if (outFmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
                    outAudioCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
                }

                ret = avcodec_open2(outAudioCodecCtx.get(), outAudioEncoder, nullptr);
                if (ret < 0) {
                    updateState(TaskState::Failed);
                    emit q_ptr->finished(false, QString("打开输出音频编码器失败: %1").arg(ffmpegErrorToString(ret)));
                    return;
                }

                outAudioStream = avformat_new_stream(outFmtCtx.get(), nullptr);
                avcodec_parameters_from_context(outAudioStream->codecpar, outAudioCodecCtx.get());
                outAudioStream->time_base = outAudioCodecCtx->time_base;

                // 初始化 SwrContext 重采样
                int64_t inLayout = inAudioCodecCtx->channel_layout;
                if (inLayout == 0) inLayout = av_get_default_channel_layout(inAudioCodecCtx->channels);

                swrCtx.reset(swr_alloc_set_opts(
                    nullptr,
                    outAudioCodecCtx->channel_layout, outAudioCodecCtx->sample_fmt, outAudioCodecCtx->sample_rate,
                    inLayout, inAudioCodecCtx->sample_fmt, inAudioCodecCtx->sample_rate,
                    0, nullptr
                ));
                swr_init(swrCtx.get());

                int fifoInitialSize = outAudioCodecCtx->frame_size > 0 ? outAudioCodecCtx->frame_size * 4 : 4096;
                audioFifo.reset(av_audio_fifo_alloc(outAudioCodecCtx->sample_fmt, outAudioCodecCtx->channels, fifoInitialSize));
            }
        }

        // 4. 打开输出文件写入 Header
        if (!(outFmtCtx->oformat->flags & AVFMT_NOFILE)) {
            ret = avio_open(&outFmtCtx->pb, cfg.outputPath.toUtf8().constData(), AVIO_FLAG_WRITE);
            if (ret < 0) {
                updateState(TaskState::Failed);
                emit q_ptr->finished(false, QString("无法创建输出文件: %1").arg(ffmpegErrorToString(ret)));
                return;
            }
        }

        ret = avformat_write_header(outFmtCtx.get(), nullptr);
        if (ret < 0) {
            updateState(TaskState::Failed);
            outFmtCtx.reset();
            inFmtCtx.reset();
            if (QFile::exists(cfg.outputPath)) {
                QFile::remove(cfg.outputPath);
            }
            emit q_ptr->finished(false, QString("写入媒体文件头失败: %1").arg(ffmpegErrorToString(ret)));
            return;
        }

        // 准备转码帧与包
        UniqueAvPacket inPacket(av_packet_alloc());
        UniqueAvPacket outPacket(av_packet_alloc());
        UniqueAvFrame decodedFrame(av_frame_alloc());

        UniqueAvFrame scaledVideoFrame(av_frame_alloc());
        if (transcodeVideo) {
            scaledVideoFrame->width = outVideoCodecCtx->width;
            scaledVideoFrame->height = outVideoCodecCtx->height;
            scaledVideoFrame->format = outVideoCodecCtx->pix_fmt;
            av_frame_get_buffer(scaledVideoFrame.get(), 32);
        }

        int64_t encodedVideoFrames = 0;
        int64_t lastReportMs = 0;
        int64_t lastVideoPts = -1;
        int64_t fallbackVideoPts = 0;

        qint64 lastPreviewEmitMs = 0;
        SwsContext *previewSwsOrigin = nullptr;
        SwsContext *previewSwsProcessed = nullptr;
        int originCacheW = 0, originCacheH = 0;
        int procCacheW = 0, procCacheH = 0;

        // 预光栅化自定义水印位图 (一次性生成，微秒级内存融合)
        QPoint wmPos;
        QImage wmRaster;
        if (transcodeVideo && cfg.watermark.enabled) {
            wmRaster = createWatermarkRaster(cfg.watermark, outVideoCodecCtx->width, outVideoCodecCtx->height, wmPos);
        }

        // 5. 核心转码处理循环
        while (isRunning && !isCanceled) {
            // 处理暂停
            if (isPaused) {
                std::unique_lock<std::mutex> lock(pauseMutex);
                pauseCv.wait(lock, [this]() {
                    return !isPaused || isCanceled || !isRunning;
                });
            }

            if (isCanceled || !isRunning) break;

            ret = av_read_frame(inFmtCtx.get(), inPacket.get());
            if (ret < 0) {
                // EOF 或读取错误
                break;
            }

            int streamIdx = inPacket->stream_index;

            // --- 处理视频 ---
            if (streamIdx == inVideoIdx && hasVideoOutput) {
                if (!transcodeVideo) {
                    // Stream Copy: 重算 pts/dts 并写入
                    av_packet_rescale_ts(inPacket.get(), inFmtCtx->streams[inVideoIdx]->time_base, outVideoStream->time_base);
                    inPacket->stream_index = outVideoStream->index;
                    av_interleaved_write_frame(outFmtCtx.get(), inPacket.get());
                } else {
                    // 解码视频
                    ret = avcodec_send_packet(inVideoCodecCtx.get(), inPacket.get());
                    if (ret >= 0) {
                        while (avcodec_receive_frame(inVideoCodecCtx.get(), decodedFrame.get()) == 0) {
                            if (isCanceled) break;

                            // 尺寸与颜色空间转换
                            sws_scale(swsCtx.get(), decodedFrame->data, decodedFrame->linesize, 0,
                                      inVideoCodecCtx->height, scaledVideoFrame->data, scaledVideoFrame->linesize);

                            // 内存级图像算法：动态实时去水印与区域平滑
                            if (cfg.delogo.enabled) {
                                applyDelogoYuv(scaledVideoFrame.get(), cfg.delogo);
                            }

                            // 内存级图像算法：自定义水印透明度 Alpha Blending 融合
                            if (cfg.watermark.enabled && !wmRaster.isNull()) {
                                applyWatermarkYuv(scaledVideoFrame.get(), wmRaster, wmPos, cfg.watermark.opacity);
                            }

                            // 动态精准时间戳计算 (弃用伪造的自增计数，按真实时间戳重映射对齐)
                            int64_t inPts = decodedFrame->best_effort_timestamp;
                            if (inPts == AV_NOPTS_VALUE) inPts = decodedFrame->pts;
                            if (inPts == AV_NOPTS_VALUE) inPts = fallbackVideoPts;
                            fallbackVideoPts = inPts + 1;

                            int64_t targetPts = av_rescale_q_rnd(
                                inPts,
                                inFmtCtx->streams[inVideoIdx]->time_base,
                                outVideoCodecCtx->time_base,
                                static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX)
                            );
                            if (targetPts <= lastVideoPts) {
                                targetPts = lastVideoPts + 1;
                            }
                            lastVideoPts = targetPts;
                            scaledVideoFrame->pts = targetPts;

                            // 实时双分屏预览抽样抛送 (节流约 25 FPS，避免跨线程信号风暴)
                            qint64 nowMs = timer.elapsed();
                            if (nowMs - lastPreviewEmitMs >= 40) {
                                lastPreviewEmitMs = nowMs;
                                QImage origImg = avFrameToPreviewImage(decodedFrame.get(), previewSwsOrigin, originCacheW, originCacheH, 1280);
                                QImage procImg = avFrameToPreviewImage(scaledVideoFrame.get(), previewSwsProcessed, procCacheW, procCacheH, 1280);
                                double currentPtsSec = static_cast<double>(targetPts) * av_q2d(outVideoCodecCtx->time_base);
                                emit q_ptr->frameRendered(origImg, procImg, currentPtsSec);
                            }

                            // 编码视频
                            if (avcodec_send_frame(outVideoCodecCtx.get(), scaledVideoFrame.get()) >= 0) {
                                while (avcodec_receive_packet(outVideoCodecCtx.get(), outPacket.get()) == 0) {
                                    av_packet_rescale_ts(outPacket.get(), outVideoCodecCtx->time_base, outVideoStream->time_base);
                                    outPacket->stream_index = outVideoStream->index;
                                    av_interleaved_write_frame(outFmtCtx.get(), outPacket.get());
                                    av_packet_unref(outPacket.get());
                                    encodedVideoFrames++;
                                }
                            }
                        }
                    }
                }
            }
            // --- 处理音频 ---
            else if (streamIdx == inAudioIdx && hasAudioOutput) {
                if (!transcodeAudio) {
                    av_packet_rescale_ts(inPacket.get(), inFmtCtx->streams[inAudioIdx]->time_base, outAudioStream->time_base);
                    inPacket->stream_index = outAudioStream->index;
                    av_interleaved_write_frame(outFmtCtx.get(), inPacket.get());
                } else {
                    ret = avcodec_send_packet(inAudioCodecCtx.get(), inPacket.get());
                    if (ret >= 0) {
                        while (avcodec_receive_frame(inAudioCodecCtx.get(), decodedFrame.get()) == 0) {
                            if (isCanceled) break;

                            // 重采样音频帧
                            int maxOutSamples = av_rescale_rnd(
                                swr_get_delay(swrCtx.get(), inAudioCodecCtx->sample_rate) + decodedFrame->nb_samples,
                                outAudioCodecCtx->sample_rate, inAudioCodecCtx->sample_rate, AV_ROUND_UP
                            );

                            uint8_t **convertedData = nullptr;
                            int linesize = 0;
                            av_samples_alloc_array_and_samples(&convertedData, &linesize, outAudioCodecCtx->channels,
                                                               maxOutSamples, outAudioCodecCtx->sample_fmt, 0);

                            int outSamples = swr_convert(swrCtx.get(), convertedData, maxOutSamples,
                                                         const_cast<const uint8_t**>(decodedFrame->data), decodedFrame->nb_samples);

                            if (outSamples > 0) {
                                av_audio_fifo_write(audioFifo.get(), reinterpret_cast<void**>(convertedData), outSamples);
                            }

                            if (convertedData) {
                                av_freep(&convertedData[0]);
                                av_freep(&convertedData);
                            }

                            // 当 FIFO 中的样本数达到编码器需求大小时编码
                            int frameSize = outAudioCodecCtx->frame_size > 0 ? outAudioCodecCtx->frame_size : 1024;
                            while (av_audio_fifo_size(audioFifo.get()) >= frameSize) {
                                UniqueAvFrame encAudioFrame(av_frame_alloc());
                                encAudioFrame->nb_samples = frameSize;
                                encAudioFrame->channel_layout = outAudioCodecCtx->channel_layout;
                                encAudioFrame->format = outAudioCodecCtx->sample_fmt;
                                encAudioFrame->sample_rate = outAudioCodecCtx->sample_rate;
                                av_frame_get_buffer(encAudioFrame.get(), 0);

                                av_audio_fifo_read(audioFifo.get(), reinterpret_cast<void**>(encAudioFrame->data), frameSize);

                                encAudioFrame->pts = nextAudioPts;
                                nextAudioPts += frameSize;

                                if (avcodec_send_frame(outAudioCodecCtx.get(), encAudioFrame.get()) >= 0) {
                                    while (avcodec_receive_packet(outAudioCodecCtx.get(), outPacket.get()) == 0) {
                                        av_packet_rescale_ts(outPacket.get(), outAudioCodecCtx->time_base, outAudioStream->time_base);
                                        outPacket->stream_index = outAudioStream->index;
                                        av_interleaved_write_frame(outFmtCtx.get(), outPacket.get());
                                        av_packet_unref(outPacket.get());
                                    }
                                }
                            }
                        }
                    }
                }
            }

            av_packet_unref(inPacket.get());

            // 进度计算与汇报 (每 150ms 汇报一次)
            qint64 currentElapsedMs = timer.elapsed();
            if (currentElapsedMs - lastReportMs > 150) {
                lastReportMs = currentElapsedMs;

                double currentPtsSec = 0.0;
                if (hasVideoOutput && outVideoStream) {
                    currentPtsSec = static_cast<double>(nextVideoPts) * av_q2d(outVideoCodecCtx ? outVideoCodecCtx->time_base : outVideoStream->time_base);
                } else if (hasAudioOutput && outAudioStream) {
                    currentPtsSec = static_cast<double>(nextAudioPts) * av_q2d(outAudioCodecCtx ? outAudioCodecCtx->time_base : outAudioStream->time_base);
                }

                double percent = 0.0;
                if (totalDuration > 0.0) {
                    percent = std::clamp((currentPtsSec / totalDuration) * 100.0, 0.0, 99.0);
                }

                double elapsedSec = currentElapsedMs / 1000.0;
                double speed = elapsedSec > 0.0 ? (currentPtsSec / elapsedSec) : 0.0;
                double fps = elapsedSec > 0.0 ? (static_cast<double>(encodedVideoFrames) / elapsedSec) : 0.0;
                int eta = 0;
                if (speed > 0.05 && totalDuration > currentPtsSec) {
                    eta = static_cast<int>((totalDuration - currentPtsSec) / speed);
                }

                currentProgress.percent = percent;
                currentProgress.currentPtsSec = currentPtsSec;
                currentProgress.totalDurationSec = totalDuration;
                currentProgress.currentFps = fps;
                currentProgress.speedMultiplier = speed;
                currentProgress.elapsedMs = currentElapsedMs;
                currentProgress.etaSec = eta;

                emit q_ptr->progressUpdated(currentProgress);
            }
        }

        // 6. 冲洗 (Flush) 编码器缓冲区
        if (!isCanceled && isRunning) {
            if (transcodeVideo && outVideoCodecCtx) {
                avcodec_send_frame(outVideoCodecCtx.get(), nullptr);
                while (avcodec_receive_packet(outVideoCodecCtx.get(), outPacket.get()) == 0) {
                    av_packet_rescale_ts(outPacket.get(), outVideoCodecCtx->time_base, outVideoStream->time_base);
                    outPacket->stream_index = outVideoStream->index;
                    av_interleaved_write_frame(outFmtCtx.get(), outPacket.get());
                    av_packet_unref(outPacket.get());
                }
            }

            if (transcodeAudio && outAudioCodecCtx) {
                // 1. 循环排空 swrCtx 内部残留的重采样滤波延迟
                int delay = swr_get_delay(swrCtx.get(), outAudioCodecCtx->sample_rate);
                while (delay > 0) {
                    uint8_t **flushedData = nullptr;
                    int linesize = 0;
                    int allocSamples = delay + 256;
                    av_samples_alloc_array_and_samples(&flushedData, &linesize, outAudioCodecCtx->channels,
                                                       allocSamples, outAudioCodecCtx->sample_fmt, 0);
                    int outSamples = swr_convert(swrCtx.get(), flushedData, allocSamples, nullptr, 0);
                    if (outSamples > 0) {
                        av_audio_fifo_write(audioFifo.get(), reinterpret_cast<void**>(flushedData), outSamples);
                    }
                    if (flushedData) {
                        av_freep(&flushedData[0]);
                        av_freep(&flushedData);
                    }
                    delay = swr_get_delay(swrCtx.get(), outAudioCodecCtx->sample_rate);
                    if (outSamples <= 0) break;
                }

                // 2. 将 FIFO 中剩余样本全部编码，并在片尾执行静音填充 (Pad Silence) 对齐 1024 样本
                int frameSize = outAudioCodecCtx->frame_size > 0 ? outAudioCodecCtx->frame_size : 1024;
                while (av_audio_fifo_size(audioFifo.get()) > 0) {
                    int remainingSamples = av_audio_fifo_size(audioFifo.get());
                    int take = std::min(remainingSamples, frameSize);

                    UniqueAvFrame encAudioFrame(av_frame_alloc());
                    encAudioFrame->nb_samples = frameSize;
                    encAudioFrame->channel_layout = outAudioCodecCtx->channel_layout;
                    encAudioFrame->format = outAudioCodecCtx->sample_fmt;
                    encAudioFrame->sample_rate = outAudioCodecCtx->sample_rate;
                    av_frame_get_buffer(encAudioFrame.get(), 0);

                    // 预先全部静音填充（对齐 AAC 要求）
                    for (int ch = 0; ch < outAudioCodecCtx->channels; ++ch) {
                        if (encAudioFrame->extended_data[ch]) {
                            std::memset(encAudioFrame->extended_data[ch], 0, frameSize * av_get_bytes_per_sample(outAudioCodecCtx->sample_fmt));
                        }
                    }

                    av_audio_fifo_read(audioFifo.get(), reinterpret_cast<void**>(encAudioFrame->data), take);
                    encAudioFrame->pts = nextAudioPts;
                    nextAudioPts += frameSize;

                    if (avcodec_send_frame(outAudioCodecCtx.get(), encAudioFrame.get()) >= 0) {
                        while (avcodec_receive_packet(outAudioCodecCtx.get(), outPacket.get()) == 0) {
                            av_packet_rescale_ts(outPacket.get(), outAudioCodecCtx->time_base, outAudioStream->time_base);
                            outPacket->stream_index = outAudioStream->index;
                            av_interleaved_write_frame(outFmtCtx.get(), outPacket.get());
                            av_packet_unref(outPacket.get());
                        }
                    }
                }

                // 3. 刷洗编码器残留包
                avcodec_send_frame(outAudioCodecCtx.get(), nullptr);
                while (avcodec_receive_packet(outAudioCodecCtx.get(), outPacket.get()) == 0) {
                    av_packet_rescale_ts(outPacket.get(), outAudioCodecCtx->time_base, outAudioStream->time_base);
                    outPacket->stream_index = outAudioStream->index;
                    av_interleaved_write_frame(outFmtCtx.get(), outPacket.get());
                    av_packet_unref(outPacket.get());
                }
            }

            av_write_trailer(outFmtCtx.get());
        }

        if (previewSwsOrigin) sws_freeContext(previewSwsOrigin);
        if (previewSwsProcessed) sws_freeContext(previewSwsProcessed);

        // 7. 完成状态判定
        if (isCanceled) {
            updateState(TaskState::Canceled);
            // 必须在删除文件前释放并关闭输出格式上下文，解开 Windows 文件写锁
            outFmtCtx.reset();
            inFmtCtx.reset();
            if (QFile::exists(cfg.outputPath)) {
                QFile::remove(cfg.outputPath);
            }
            emit q_ptr->finished(false, "用户已中止转码");
        } else {
            outFmtCtx.reset();
            inFmtCtx.reset();

            currentProgress.percent = 100.0;
            currentProgress.etaSec = 0;
            emit q_ptr->progressUpdated(currentProgress);

            updateState(TaskState::Completed);
            emit q_ptr->finished(true, "转码已成功完成");
        }

        isRunning = false;
    }
};

TranscodeEngine::TranscodeEngine(QObject *parent)
    : QObject(parent), d_ptr(std::make_unique<TranscodeEnginePrivate>()) {
    Q_D(TranscodeEngine);
    d->q_ptr = this;
}

TranscodeEngine::~TranscodeEngine() = default;

bool TranscodeEngine::start(const TranscodeConfig &config) {
    Q_D(TranscodeEngine);
    if (d->isRunning) {
        return false;
    }

    d->stopWorker();
    d->currentConfig = config;
    d->currentProgress = TranscodeProgress{};
    d->isRunning = true;
    d->isPaused = false;
    d->isCanceled = false;

    d->workerThread = std::thread([d, config]() {
        d->runTranscode(config);
    });

    return true;
}

void TranscodeEngine::pause() {
    Q_D(TranscodeEngine);
    if (d->isRunning && !d->isPaused) {
        d->isPaused = true;
        d->updateState(TaskState::Paused);
    }
}

void TranscodeEngine::resume() {
    Q_D(TranscodeEngine);
    if (d->isRunning && d->isPaused) {
        d->isPaused = false;
        d->pauseCv.notify_all();
        d->updateState(TaskState::Converting);
    }
}

void TranscodeEngine::cancel() {
    Q_D(TranscodeEngine);
    if (d->isRunning) {
        d->isCanceled = true;
        d->isPaused = false;
        d->pauseCv.notify_all();
    }
}

TaskState TranscodeEngine::state() const {
    return d_ptr->state.load();
}

TranscodeProgress TranscodeEngine::progress() const {
    return d_ptr->currentProgress;
}

TranscodeConfig TranscodeEngine::config() const {
    return d_ptr->currentConfig;
}

} // namespace ffmpeg_transform
