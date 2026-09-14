#pragma once

#include "core/CommonTypes.h"
#include <QWidget>
#include <memory>

namespace ffmpeg_transform {

class MediaInfoCardPrivate;

/**
 * @brief 视频元数据与缩略图卡片展示组件 (遵循 Pimpl 与动态样式规范)
 */
class MediaInfoCard : public QWidget {
    Q_OBJECT
    Q_PROPERTY(bool hasMedia READ hasMedia WRITE setHasMedia NOTIFY mediaStateChanged)

public:
    explicit MediaInfoCard(QWidget *parent = nullptr);
    ~MediaInfoCard() override;

    bool hasMedia() const;
    void setHasMedia(bool hasMedia);

    /**
     * @brief 加载视频元数据与缩略图
     */
    void setMedia(const MediaInfo &info, const QImage &thumbnail);

    /**
     * @brief 清空当前展示
     */
    void clearMedia();

signals:
    void mediaStateChanged(bool hasMedia);

private:
    std::unique_ptr<MediaInfoCardPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(MediaInfoCard)
};

} // namespace ffmpeg_transform
