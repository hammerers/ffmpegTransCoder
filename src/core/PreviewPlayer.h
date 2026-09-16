#pragma once

#include <QObject>
#include <QImage>
#include <memory>

namespace ffmpeg_transform {

class PreviewPlayerPrivate;

enum class PlaybackMode {
    Idle,           // 未加载媒体
    SingleSource,   // 单视频调参播放模式 (原画解码，右屏动态模拟滤镜)
    DualSource      // 双视频同步对比播放模式 (原画 vs 导出成品)
};

/**
 * @brief 高性能轻量级视频预览播放引擎 (遵循 Pimpl 规范)
 * 采用原生 FFmpeg C API 解码，支持单源动态调参播放与双源同帧率无漂移同步播放
 */
class PreviewPlayer : public QObject {
    Q_OBJECT

public:
    explicit PreviewPlayer(QObject *parent = nullptr);
    ~PreviewPlayer() override;

    bool open(const QString &sourcePath, const QString &comparedPath = QString());
    void close();

    PlaybackMode mode() const;
    bool isPlaying() const;
    bool isOpened() const;

    double duration() const;
    double currentPosition() const;
    double playbackSpeed() const;
    void setPlaybackSpeed(double speed);

    QSize sourceVideoSize() const;
    QSize comparedVideoSize() const;

public slots:
    void play();
    void pause();
    void togglePlay();
    void seek(double ptsSec);
    void stop();

signals:
    void frameReady(const QImage &origin, const QImage &compared, double ptsSec);
    void positionChanged(double currentSec, double totalSec);
    void playbackStateChanged(bool isPlaying);
    void mediaOpened(PlaybackMode mode, double durationSec);
    void playbackEnded();

private:
    std::unique_ptr<PreviewPlayerPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(PreviewPlayer)
};

} // namespace ffmpeg_transform

Q_DECLARE_METATYPE(ffmpeg_transform::PlaybackMode)
