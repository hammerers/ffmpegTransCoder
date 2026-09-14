#include "TranscodeEngine.h"
#include "FFmpegUtils.h"
#include <QElapsedTimer>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <algorithm>

namespace ffmpeg_transform {

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

                // 选择编码器
                const AVCodec *outVideoEncoder = nullptr;
                if (cfg.videoCodec == VideoCodecType::H264) {
                    outVideoEncoder = avcodec_find_encoder_by_name("libx264");
                    if (!outVideoEncoder) outVideoEncoder = avcodec_find_encoder(AV_CODEC_ID_H264);
                } else if (cfg.videoCodec == VideoCodecType::H265) {
                    outVideoEncoder = avcodec_find_encoder_by_name("libx265");
                    if (!outVideoEncoder) outVideoEncoder = avcodec_find_encoder(AV_CODEC_ID_HEVC);
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

                            scaledVideoFrame->pts = nextVideoPts++;

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
                // 将 FIFO 中剩余样本全部编码
                int remainingSamples = av_audio_fifo_size(audioFifo.get());
                if (remainingSamples > 0) {
                    UniqueAvFrame encAudioFrame(av_frame_alloc());
                    encAudioFrame->nb_samples = remainingSamples;
                    encAudioFrame->channel_layout = outAudioCodecCtx->channel_layout;
                    encAudioFrame->format = outAudioCodecCtx->sample_fmt;
                    encAudioFrame->sample_rate = outAudioCodecCtx->sample_rate;
                    av_frame_get_buffer(encAudioFrame.get(), 0);

                    av_audio_fifo_read(audioFifo.get(), reinterpret_cast<void**>(encAudioFrame->data), remainingSamples);
                    encAudioFrame->pts = nextAudioPts;

                    if (avcodec_send_frame(outAudioCodecCtx.get(), encAudioFrame.get()) >= 0) {
                        while (avcodec_receive_packet(outAudioCodecCtx.get(), outPacket.get()) == 0) {
                            av_packet_rescale_ts(outPacket.get(), outAudioCodecCtx->time_base, outAudioStream->time_base);
                            outPacket->stream_index = outAudioStream->index;
                            av_interleaved_write_frame(outFmtCtx.get(), outPacket.get());
                            av_packet_unref(outPacket.get());
                        }
                    }
                }

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

        // 7. 完成状态判定
        if (isCanceled) {
            updateState(TaskState::Canceled);
            // 删除未完成的文件
            QFile::remove(cfg.outputPath);
            emit q_ptr->finished(false, "用户已中止转码");
        } else {
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
