#pragma once

#include <QObject>
#include <QImage>
#include <memory>

namespace ffmpeg_transform {

class ThumbnailExtractorPrivate;

/**
 * @brief 视频缩略图智能提取器 (遵循 Pimpl 规范)
 */
class ThumbnailExtractor : public QObject {
    Q_OBJECT

public:
    explicit ThumbnailExtractor(QObject *parent = nullptr);
    ~ThumbnailExtractor() override;

    /**
     * @brief 提取视频指定时间位置或比例处的缩略图
     * @param filePath 视频文件绝对路径
     * @param outImage 输出的缩略图 QImage
     * @param errorMsg 失败错误信息
     * @param seekRatio 提取时间位置比例 (默认 0.08，避开片头黑屏)
     * @param targetWidth 缩略图目标宽度 (<=0 则提取原画完整原生物理分辨率，高度按比例自适应)
     * @return 成功返回 true，失败返回 false
     */
    bool extractThumbnail(const QString &filePath,
                          QImage &outImage,
                          QString &errorMsg,
                          double seekRatio = 0.08,
                          int targetWidth = 0);

private:
    std::unique_ptr<ThumbnailExtractorPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(ThumbnailExtractor)
};

} // namespace ffmpeg_transform
