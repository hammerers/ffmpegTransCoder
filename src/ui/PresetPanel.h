#pragma once

#include "core/CommonTypes.h"
#include <QWidget>
#include <memory>

namespace ffmpeg_transform {

class PresetPanelPrivate;

/**
 * @brief 专业级音视频压制参数控制台 (遵循 Pimpl 与动态样式规范)
 */
class PresetPanel : public QWidget {
    Q_OBJECT
    Q_PROPERTY(bool isAudioOnly READ isAudioOnly WRITE setIsAudioOnly NOTIFY audioOnlyChanged)

public:
    explicit PresetPanel(QWidget *parent = nullptr);
    ~PresetPanel() override;

    bool isAudioOnly() const;
    void setIsAudioOnly(bool audioOnly);

    TranscodeConfig config() const;
    void setConfig(const TranscodeConfig &cfg);

    /**
     * @brief 生成当前配置等效的 FFmpeg 命令行参数字符串
     */
    QString generateEquivalentArgs() const;

signals:
    void configChanged(const TranscodeConfig &cfg);
    void audioOnlyChanged(bool isAudioOnly);

private:
    std::unique_ptr<PresetPanelPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(PresetPanel)
};

} // namespace ffmpeg_transform
