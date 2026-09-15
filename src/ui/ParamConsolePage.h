#pragma once

#include "core/CommonTypes.h"
#include <QWidget>
#include <memory>

namespace ffmpeg_transform {

class ParamConsolePagePrivate;

/**
 * @brief 专业参数面板工作区 (二级导航树 + 深度FFmpeg参数配置 + 实时等效命令预览)
 * 遵循 Pimpl 与动态样式规范
 */
class ParamConsolePage : public QWidget {
    Q_OBJECT

public:
    explicit ParamConsolePage(QWidget *parent = nullptr);
    ~ParamConsolePage() override;

    TranscodeConfig config() const;
    void setConfig(const TranscodeConfig &cfg);

    QString generateEquivalentArgs() const;

signals:
    void configChanged(const TranscodeConfig &cfg);
    void applyToAllRequested(const TranscodeConfig &cfg);

private:
    std::unique_ptr<ParamConsolePagePrivate> const d_ptr;
    Q_DECLARE_PRIVATE(ParamConsolePage)
};

} // namespace ffmpeg_transform
