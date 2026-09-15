#pragma once

#include "core/CommonTypes.h"
#include <QObject>
#include <memory>

namespace ffmpeg_transform {

class TranscodeTaskPrivate;

/**
 * @brief 单个转码任务实体 (遵循 Pimpl 规范)
 */
class TranscodeTask : public QObject {
    Q_OBJECT

public:
    explicit TranscodeTask(const QString &inputFilePath, QObject *parent = nullptr);
    ~TranscodeTask() override;

    QString id() const;
    QString inputFilePath() const;

    MediaInfo mediaInfo() const;
    void setMediaInfo(const MediaInfo &info);

    QImage thumbnail() const;
    void setThumbnail(const QImage &image);

    TranscodeConfig config() const;
    void setConfig(const TranscodeConfig &config);

    TaskState state() const;
    void setState(TaskState state);

    TranscodeProgress progress() const;
    void setProgress(const TranscodeProgress &progress);

    QString errorMessage() const;
    void setErrorMessage(const QString &message);

signals:
    void stateChanged(TaskState state);
    void progressChanged(const TranscodeProgress &progress);
    void configChanged(const TranscodeConfig &config);
    void thumbnailLoaded(const QImage &image);
    void mediaInfoLoaded(const MediaInfo &info);
    void errorOccurred(const QString &message);

private:
    std::unique_ptr<TranscodeTaskPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(TranscodeTask)
};

} // namespace ffmpeg_transform
