#pragma once

#include "core/CommonTypes.h"
#include <QWidget>
#include <memory>

namespace ffmpeg_transform {

class PresetPanelPrivate;

/**
 * @brief 格式预设与专家级转码参数配置面板 (遵循 Pimpl 与动态样式规范)
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

signals:
    void configChanged(const TranscodeConfig &cfg);
    void audioOnlyChanged(bool isAudioOnly);

private:
    std::unique_ptr<PresetPanelPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(PresetPanel)
};

} // namespace ffmpeg_transform
