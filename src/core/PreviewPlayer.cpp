#include "PreviewPlayer.h"
#include "FFmpegUtils.h"

#include <QFileInfo>
#include <QFile>
#include <QDebug>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <algorithm>
#include <chrono>
#include <iostream>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
}

namespace ffmpeg_transform {

// 单路流轻量级解码器封装
struct SingleStreamDecoder {
    QString filePath;
    UniqueAvFormatInput fmtCtx;
    UniqueAvCodecContext codecCtx;
    int videoStreamIdx{-1};
    AVRational timeBase{1, 1000};
    double durationSec{0.0};
    double fps{25.0};

    UniqueAvPacket packet{av_packet_alloc()};
    UniqueAvFrame frame{av_frame_alloc()};

    SwsContext *swsCtx{nullptr};
    int cachedW{0};
    int cachedH{0};

    ~SingleStreamDecoder() {
        close();
    }

    bool open(const QString &path, QString &errMsg) {
        close();
        filePath = path;

        AVFormatContext *rawFmt = nullptr;
        QByteArray utf8Path = path.toUtf8();
        int ret = avformat_open_input(&rawFmt, utf8Path.constData(), nullptr, nullptr);
        if (ret < 0) {
            errMsg = QString("打开文件失败: %1 (错误: %2)").arg(path, ffmpegErrorToString(ret));
            return false;
        }
        fmtCtx.reset(rawFmt);

        ret = avformat_find_stream_info(fmtCtx.get(), nullptr);
        if (ret < 0) {
            errMsg = QString("读取媒体流信息失败: %1").arg(ffmpegErrorToString(ret));
            return false;
        }

        AVCodec *decoder = nullptr;
        videoStreamIdx = av_find_best_stream(fmtCtx.get(), AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
        if (videoStreamIdx < 0 || !decoder) {
            errMsg = "未找到可用视频流或对应解码器";
            return false;
        }

        AVStream *stream = fmtCtx->streams[videoStreamIdx];
        timeBase = stream->time_base;

        codecCtx.reset(avcodec_alloc_context3(decoder));
        if (!codecCtx) {
            errMsg = "分配解码器上下文失败";
            return false;
        }

        ret = avcodec_parameters_to_context(codecCtx.get(), stream->codecpar);
        if (ret < 0) {
            errMsg = "填充解码器参数失败";
            return false;
        }

        codecCtx->thread_count = 1; // 纯单线程即时解码，绝无异步锁竞争与缓冲延迟
        codecCtx->thread_type = 0;

        ret = avcodec_open2(codecCtx.get(), decoder, nullptr);
        if (ret < 0) {
            errMsg = QString("打开解码器失败: %1").arg(ffmpegErrorToString(ret));
            return false;
        }

        // 计算时长与帧率
        if (stream->duration > 0) {
            durationSec = static_cast<double>(stream->duration) * av_q2d(timeBase);
        } else if (fmtCtx->duration > 0) {
            durationSec = static_cast<double>(fmtCtx->duration) / AV_TIME_BASE;
        }

        AVRational rFps = (stream->avg_frame_rate.num > 0 && stream->avg_frame_rate.den > 0)
                          ? stream->avg_frame_rate : stream->r_frame_rate;
        if (rFps.num > 0 && rFps.den > 0) {
            fps = av_q2d(rFps);
        }
        if (fps <= 1.0 || fps > 120.0) fps = 25.0;

        return true;
    }

    void close() {
        if (swsCtx) {
            sws_freeContext(swsCtx);
            swsCtx = nullptr;
        }
        cachedW = 0;
        cachedH = 0;
        if (packet) av_packet_unref(packet.get());
        if (frame) av_frame_unref(frame.get());
        codecCtx.reset();
        fmtCtx.reset();
        videoStreamIdx = -1;
        durationSec = 0.0;
        filePath.clear();
    }

    bool decodeNextFrame(QImage &outImg, double &outPtsSec, int maxDim = 960) {
        if (!fmtCtx || !codecCtx || videoStreamIdx < 0) return false;

        while (true) {
            int ret = avcodec_receive_frame(codecCtx.get(), frame.get());
            if (ret == 0) {
                // 成功解码出一帧
                int64_t pts = frame->best_effort_timestamp;
                if (pts == AV_NOPTS_VALUE) pts = frame->pts;
                outPtsSec = (pts != AV_NOPTS_VALUE) ? (static_cast<double>(pts) * av_q2d(timeBase)) : 0.0;

                // 缩放到高清预览尺寸
                int fw = frame->width;
                int fh = frame->height;
                if (fw <= 0 || fh <= 0) return false;

                int targetW = fw;
                int targetH = fh;
                if (targetW > maxDim || targetH > maxDim) {
                    if (targetW >= targetH) {
                        targetH = static_cast<int>(maxDim * static_cast<double>(fh) / fw);
                        targetW = maxDim;
                    } else {
                        targetW = static_cast<int>(maxDim * static_cast<double>(fw) / fh);
                        targetH = maxDim;
                    }
                }
                if (targetW % 2 != 0) targetW++;
                if (targetH % 2 != 0) targetH++;

                if (!swsCtx || cachedW != fw || cachedH != fh) {
                    if (swsCtx) sws_freeContext(swsCtx);
                    swsCtx = sws_getContext(
                        fw, fh, static_cast<AVPixelFormat>(frame->format),
                        targetW, targetH, AV_PIX_FMT_RGB24,
                        SWS_BILINEAR, nullptr, nullptr, nullptr
                    );
                    cachedW = fw;
                    cachedH = fh;
                }

                if (!swsCtx) return false;

                int dstLinesize[4] = {0};
                uint8_t *dstData[4] = {nullptr};
                int allocRet = av_image_alloc(dstData, dstLinesize, targetW, targetH, AV_PIX_FMT_RGB24, 32);
                if (allocRet < 0) return false;

                sws_scale(swsCtx, frame->data, frame->linesize, 0, fh, dstData, dstLinesize);

                QImage tmp(dstData[0], targetW, targetH, dstLinesize[0], QImage::Format_RGB888);
                outImg = tmp.copy();

                av_freep(&dstData[0]);
                return true;
            }

            // 读取新 packet
            ret = av_read_frame(fmtCtx.get(), packet.get());
            if (ret < 0) {
                // EOF 或读取结束
                return false;
            }

            if (packet->stream_index == videoStreamIdx) {
                avcodec_send_packet(codecCtx.get(), packet.get());
            }
            av_packet_unref(packet.get());
        }
    }

    bool seek(double targetSec) {
        if (!fmtCtx || !codecCtx || videoStreamIdx < 0) return false;
        av_packet_unref(packet.get());
        int64_t targetTs = av_rescale_q(static_cast<int64_t>(targetSec * AV_TIME_BASE), {1, AV_TIME_BASE}, timeBase);
        int ret = av_seek_frame(fmtCtx.get(), videoStreamIdx, targetTs, AVSEEK_FLAG_BACKWARD);
        if (ret < 0) {
            av_seek_frame(fmtCtx.get(), videoStreamIdx, targetTs, 0);
        }
        avcodec_flush_buffers(codecCtx.get());
        return true;
    }
};

class PreviewPlayerPrivate {
public:
    PreviewPlayer *q_ptr{nullptr};

    PlaybackMode mode{PlaybackMode::Idle};
    std::atomic<bool> isPlaying{false};
    std::atomic<bool> isOpened{false};
    std::atomic<bool> isRunning{false};

    double durationSec{0.0};
    std::atomic<double> currentPts{0.0};
    std::atomic<double> speed{1.0};

    SingleStreamDecoder sourceDecoder;
    SingleStreamDecoder comparedDecoder;

    std::thread workerThread;
    std::mutex mtx;
    std::condition_variable cv;
    std::mutex decoderMtx;

    std::atomic<bool> seekPending{false};
    std::atomic<double> seekTargetPts{0.0};

    void workerLoop() {
        while (isRunning) {
            // 等待播放唤醒或 Seek
            {
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait(lock, [this]() {
                    return !isRunning || isPlaying || seekPending;
                });
            }

            if (!isRunning) break;

            // 处理精准 Seek
            if (seekPending.exchange(false)) {
                double target = seekTargetPts.load();
                QImage img1, img2;
                double pts1 = target, pts2 = target;
                {
                    std::lock_guard<std::mutex> decLock(decoderMtx);
                    if (!isOpened) continue;
                    sourceDecoder.seek(target);
                    if (mode == PlaybackMode::DualSource) {
                        comparedDecoder.seek(target);
                    }
                    if (sourceDecoder.decodeNextFrame(img1, pts1)) {
                        currentPts = pts1;
                        if (mode == PlaybackMode::DualSource) {
                            comparedDecoder.decodeNextFrame(img2, pts2);
                        }
                    }
                }
                if (!img1.isNull()) {
                    emit q_ptr->frameReady(img1, img2, pts1);
                    emit q_ptr->positionChanged(pts1, durationSec);
                }
                continue;
            }

            if (!isPlaying) continue;

            auto frameStart = std::chrono::steady_clock::now();

            QImage originImg;
            double originPts = 0.0;
            QImage compImg;
            double compPts = 0.0;
            bool ok1 = false;
            bool ok2 = true;

            {
                std::lock_guard<std::mutex> decLock(decoderMtx);
                if (!isOpened || !isPlaying) continue;
                ok1 = sourceDecoder.decodeNextFrame(originImg, originPts);
                if (mode == PlaybackMode::DualSource) {
                    ok2 = comparedDecoder.decodeNextFrame(compImg, compPts);
                }
            }

            if (!ok1) {
                {
                    std::lock_guard<std::mutex> decLock(decoderMtx);
                    sourceDecoder.seek(0.0);
                    if (mode == PlaybackMode::DualSource) comparedDecoder.seek(0.0);
                }
                currentPts = 0.0;
                emit q_ptr->positionChanged(0.0, durationSec);
                emit q_ptr->playbackEnded();
                isPlaying = false;
                emit q_ptr->playbackStateChanged(false);
                continue;
            }

            currentPts = originPts;
            emit q_ptr->frameReady(originImg, compImg, originPts);
            emit q_ptr->positionChanged(originPts, durationSec);

            // 帧率与倍速精确延时对齐
            double currentFps = sourceDecoder.fps;
            if (currentFps <= 1.0) currentFps = 25.0;
            double currentSpeed = speed.load();
            if (currentSpeed <= 0.1) currentSpeed = 1.0;

            double frameIntervalSec = (1.0 / currentFps) / currentSpeed;
            auto targetDuration = std::chrono::duration<double>(frameIntervalSec);
            auto elapsed = std::chrono::steady_clock::now() - frameStart;
            if (targetDuration > elapsed) {
                std::this_thread::sleep_for(targetDuration - elapsed);
            }
        }
    }
};

PreviewPlayer::PreviewPlayer(QObject *parent)
    : QObject(parent), d_ptr(std::make_unique<PreviewPlayerPrivate>()) {
    qRegisterMetaType<PlaybackMode>("PlaybackMode");
    Q_D(PreviewPlayer);
    d->q_ptr = this;
    d->isRunning = true;
    d->workerThread = std::thread(&PreviewPlayerPrivate::workerLoop, d);
}

PreviewPlayer::~PreviewPlayer() {
    Q_D(PreviewPlayer);
    d->isRunning = false;
    d->isPlaying = false;
    d->cv.notify_all();
    if (d->workerThread.joinable()) {
        d->workerThread.join();
    }
}

bool PreviewPlayer::open(const QString &sourcePath, const QString &comparedPath) {
    Q_D(PreviewPlayer);
    pause();

    if (sourcePath.isEmpty() || !QFile::exists(sourcePath)) {
        return false;
    }

    std::lock_guard<std::mutex> decLock(d->decoderMtx);

    QString err;
    if (!d->sourceDecoder.open(sourcePath, err)) {
        qWarning() << "[PreviewPlayer] 打开主视频流失败:" << err;
        d->mode = PlaybackMode::Idle;
        d->isOpened = false;
        return false;
    }

    d->durationSec = d->sourceDecoder.durationSec;
    d->currentPts = 0.0;

    if (!comparedPath.isEmpty() && QFile::exists(comparedPath)) {
        QString err2;
        if (d->comparedDecoder.open(comparedPath, err2)) {
            d->mode = PlaybackMode::DualSource;
            qDebug() << "[PreviewPlayer] 已加载双视频同步对比模式:" << sourcePath << "vs" << comparedPath;
        } else {
            d->mode = PlaybackMode::SingleSource;
            qDebug() << "[PreviewPlayer] 副视频加载失败，降级为单视频调参预览模式";
        }
    } else {
        d->mode = PlaybackMode::SingleSource;
        d->comparedDecoder.close();
        qDebug() << "[PreviewPlayer] 已加载单视频调参播放模式:" << sourcePath;
    }

    d->isOpened = true;

    // 同步解码出第一帧用于就绪画面呈现
    QImage img1, img2;
    double pts1 = 0.0, pts2 = 0.0;
    if (d->sourceDecoder.decodeNextFrame(img1, pts1)) {
        d->currentPts = pts1;
        if (d->mode == PlaybackMode::DualSource) {
            d->comparedDecoder.decodeNextFrame(img2, pts2);
        }
    }

    emit mediaOpened(d->mode, d->durationSec);
    if (!img1.isNull()) {
        emit frameReady(img1, img2, pts1);
        emit positionChanged(pts1, d->durationSec);
    }
    return true;
}

void PreviewPlayer::close() {
    Q_D(PreviewPlayer);
    pause();
    std::lock_guard<std::mutex> decLock(d->decoderMtx);
    d->sourceDecoder.close();
    d->comparedDecoder.close();
    d->mode = PlaybackMode::Idle;
    d->isOpened = false;
    d->durationSec = 0.0;
    d->currentPts = 0.0;
}

PlaybackMode PreviewPlayer::mode() const {
    return d_ptr->mode;
}

bool PreviewPlayer::isPlaying() const {
    return d_ptr->isPlaying;
}

bool PreviewPlayer::isOpened() const {
    return d_ptr->isOpened;
}

double PreviewPlayer::duration() const {
    return d_ptr->durationSec;
}

double PreviewPlayer::currentPosition() const {
    return d_ptr->currentPts;
}

double PreviewPlayer::playbackSpeed() const {
    return d_ptr->speed;
}

void PreviewPlayer::setPlaybackSpeed(double spd) {
    Q_D(PreviewPlayer);
    d->speed = std::clamp(spd, 0.25, 4.0);
}

void PreviewPlayer::play() {
    Q_D(PreviewPlayer);
    if (!d->isOpened || d->isPlaying) return;
    d->isPlaying = true;
    d->cv.notify_all();
    emit playbackStateChanged(true);
}

void PreviewPlayer::pause() {
    Q_D(PreviewPlayer);
    if (!d->isPlaying) return;
    d->isPlaying = false;
    emit playbackStateChanged(false);
}

void PreviewPlayer::togglePlay() {
    if (isPlaying()) {
        pause();
    } else {
        play();
    }
}

void PreviewPlayer::seek(double ptsSec) {
    Q_D(PreviewPlayer);
    if (!d->isOpened) return;
    d->seekTargetPts = std::clamp(ptsSec, 0.0, d->durationSec);
    d->seekPending = true;
    d->cv.notify_all();
}

void PreviewPlayer::stop() {
    Q_D(PreviewPlayer);
    d->isPlaying = false;
    emit playbackStateChanged(false);
    seek(0.0);
}

} // namespace ffmpeg_transform
