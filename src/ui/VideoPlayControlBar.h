#pragma once

#include <QWidget>
#include <memory>

namespace ffmpeg_transform {

class VideoPlayControlBarPrivate;

/**
 * @brief 实时视频播放控制条组件 (遵循 Pimpl 规范与动态 QSS 样式解耦)
 * 支持播放/暂停、停止复位、精准时间轴拖拽与点击 Seek、倍速切换、双模式状态指示
 */
class VideoPlayControlBar : public QWidget {
    Q_OBJECT
    Q_PROPERTY(bool isPlaying READ isPlaying WRITE setPlaying NOTIFY playingChanged)
    Q_PROPERTY(QString playbackMode READ playbackMode WRITE setPlaybackMode NOTIFY playbackModeChanged)

public:
    explicit VideoPlayControlBar(QWidget *parent = nullptr);
    ~VideoPlayControlBar() override;

    bool isPlaying() const;
    void setPlaying(bool playing);

    QString playbackMode() const;
    void setPlaybackMode(const QString &mode); // "idle", "single", "dual"
    void setModeDescription(const QString &text);

    void setDuration(double totalSec);
    void setPosition(double currentSec, double totalSec);
    void setControlEnabled(bool enabled);
    void reset();

signals:
    void playClicked();
    void pauseClicked();
    void stopClicked();
    void seekRequested(double ptsSec);
    void speedChanged(double speed);
    void playingChanged(bool playing);
    void playbackModeChanged(const QString &mode);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    std::unique_ptr<VideoPlayControlBarPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(VideoPlayControlBar)
};

} // namespace ffmpeg_transform
