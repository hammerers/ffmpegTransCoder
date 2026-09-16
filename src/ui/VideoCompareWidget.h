#pragma once

#include "core/CommonTypes.h"
#include <QWidget>
#include <QImage>
#include <memory>

namespace ffmpeg_transform {

class VideoCompareWidgetPrivate;

enum class CompareMode {
    SideBySide,     // 左右分屏对比
    CurtainSplit,   // 鼠标可拖拽卷帘分屏
    ProcessedOnly   // 仅显示处理后画面
};

/**
 * @brief 实时音视频双分屏画面对比播放视口 (遵循 Pimpl 模式与动态样式规范)
 */
class VideoCompareWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int compareMode READ compareModeInt WRITE setCompareModeInt NOTIFY compareModeChanged)

public:
    explicit VideoCompareWidget(QWidget *parent = nullptr);
    ~VideoCompareWidget() override;

    CompareMode compareMode() const;
    void setCompareMode(CompareMode mode);

    int compareModeInt() const;
    void setCompareModeInt(int mode);

    float splitPosition() const;
    void setSplitPosition(float pos);

    void setDelogoHighlight(bool enabled, const QRect &rect);
    void setWatermarkConfig(const WatermarkConfig &cfg);
    bool hasFrames() const;

public slots:
    void setStaticPreview(const QImage &origin, const QString &taskName = QString());
    void setCompleted(bool completed, const QString &summaryText = QString());
    void updateFrames(const QImage &origin, const QImage &processed, double ptsSec);
    void resetToIdle();

signals:
    void compareModeChanged(CompareMode mode);
    void splitPositionChanged(float pos);
    void watermarkConfigChanged(const WatermarkConfig &cfg);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    std::unique_ptr<VideoCompareWidgetPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(VideoCompareWidget)
};

} // namespace ffmpeg_transform
