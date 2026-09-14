#pragma once

#include "core/CommonTypes.h"
#include <QWidget>
#include <memory>

namespace ffmpeg_transform {

class MediaInspectorPagePrivate;
class MediaInfoCard;

/**
 * @brief 媒体信息检查工作区 (遵循 Pimpl 与动态样式规范)
 */
class MediaInspectorPage : public QWidget {
    Q_OBJECT

public:
    explicit MediaInspectorPage(QWidget *parent = nullptr);
    ~MediaInspectorPage() override;

    void setMedia(const MediaInfo &info, const QImage &thumbnail);
    void clearMedia();

private:
    std::unique_ptr<MediaInspectorPagePrivate> const d_ptr;
    Q_DECLARE_PRIVATE(MediaInspectorPage)
};

} // namespace ffmpeg_transform
