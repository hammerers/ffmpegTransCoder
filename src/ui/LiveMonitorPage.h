#pragma once

#include "core/CommonTypes.h"
#include <QWidget>
#include <memory>

namespace ffmpeg_transform {

class LiveMonitorPagePrivate;
class TranscodeTaskManager;

/**
 * @brief 实时双分屏画面对比播放与滤镜检视页面 (遵循 Pimpl 模式与动态样式规范)
 */
class LiveMonitorPage : public QWidget {
    Q_OBJECT

public:
    explicit LiveMonitorPage(QWidget *parent = nullptr);
    ~LiveMonitorPage() override;

    void setManager(TranscodeTaskManager *manager);
    DelogoConfig delogoConfig() const;
    WatermarkConfig watermarkConfig() const;
    void checkAndLoadPreview();

public slots:
    void updateLiveFrame(const QString &taskName, const QImage &origin, const QImage &processed, double ptsSec);
    void setHwAccelStatus(const QString &statusText);
    void resetToIdle();
    void showCompletedState();
    void setDelogoConfig(const DelogoConfig &cfg);
    void setWatermarkConfig(const WatermarkConfig &cfg);

signals:
    void delogoConfigChanged(const DelogoConfig &cfg);
    void watermarkConfigChanged(const WatermarkConfig &cfg);

private:
    std::unique_ptr<LiveMonitorPagePrivate> const d_ptr;
    Q_DECLARE_PRIVATE(LiveMonitorPage)
};

} // namespace ffmpeg_transform
