#pragma once

#include "CommonTypes.h"
#include <QObject>
#include <memory>

namespace ffmpeg_transform {

class MetadataExtractorPrivate;

/**
 * @brief 视频元数据提取器 (遵循 Pimpl 规范)
 */
class MetadataExtractor : public QObject {
    Q_OBJECT

public:
    explicit MetadataExtractor(QObject *parent = nullptr);
    ~MetadataExtractor() override;

    /**
     * @brief 解析指定媒体文件的元数据
     * @param filePath 视频文件绝对路径
     * @param outInfo 输出的元数据结构体
     * @param errorMsg 失败时的错误信息
     * @return 成功返回 true，失败返回 false
     */
    bool extract(const QString &filePath, MediaInfo &outInfo, QString &errorMsg);

private:
    std::unique_ptr<MetadataExtractorPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(MetadataExtractor)
};

} // namespace ffmpeg_transform
