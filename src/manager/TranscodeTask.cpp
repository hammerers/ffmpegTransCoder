#include "TranscodeTask.h"
#include <QUuid>
#include <QFileInfo>

namespace ffmpeg_transform {

class TranscodeTaskPrivate {
public:
    TranscodeTask *q_ptr{nullptr};
    QString id;
    QString inputFilePath;
    MediaInfo mediaInfo;
    QImage thumbnail;
    TranscodeConfig config;
    TaskState state{TaskState::Pending};
    TranscodeProgress progress;
    QString errorMessage;

    void initDefaultConfig() {
        QFileInfo fi(inputFilePath);
        config.inputPath = inputFilePath;
        // 默认输出路径为输入同目录下同名 .converted.mp4
        config.outputPath = fi.absolutePath() + "/" + fi.completeBaseName() + "_converted.mp4";
        config.containerFormat = "mp4";
        config.videoCodec = VideoCodecType::H264;
        config.audioCodec = AudioCodecType::AAC;
        config.resolutionScale = ResolutionScale::Original;
        config.fpsOption = FpsOption::Original;
        config.qualityMode = QualityMode::CRF;
        config.crf = 23;
        config.preset = "medium";
    }
};

TranscodeTask::TranscodeTask(const QString &inputFilePath, QObject *parent)
    : QObject(parent), d_ptr(std::make_unique<TranscodeTaskPrivate>()) {
    Q_D(TranscodeTask);
    d->q_ptr = this;
    d->id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    d->inputFilePath = inputFilePath;
    d->initDefaultConfig();
}

TranscodeTask::~TranscodeTask() = default;

QString TranscodeTask::id() const {
    return d_ptr->id;
}

QString TranscodeTask::inputFilePath() const {
    return d_ptr->inputFilePath;
}

MediaInfo TranscodeTask::mediaInfo() const {
    return d_ptr->mediaInfo;
}

void TranscodeTask::setMediaInfo(const MediaInfo &info) {
    Q_D(TranscodeTask);
    d->mediaInfo = info;
    emit mediaInfoLoaded(info);
}

QImage TranscodeTask::thumbnail() const {
    return d_ptr->thumbnail;
}

void TranscodeTask::setThumbnail(const QImage &image) {
    Q_D(TranscodeTask);
    d->thumbnail = image;
    emit thumbnailLoaded(image);
}

TranscodeConfig TranscodeTask::config() const {
    return d_ptr->config;
}

void TranscodeTask::setConfig(const TranscodeConfig &config) {
    Q_D(TranscodeTask);
    d->config = config;
}

TaskState TranscodeTask::state() const {
    return d_ptr->state;
}

void TranscodeTask::setState(TaskState state) {
    Q_D(TranscodeTask);
    if (d->state != state) {
        d->state = state;
        emit stateChanged(state);
    }
}

TranscodeProgress TranscodeTask::progress() const {
    return d_ptr->progress;
}

void TranscodeTask::setProgress(const TranscodeProgress &progress) {
    Q_D(TranscodeTask);
    d->progress = progress;
    emit progressChanged(progress);
}

QString TranscodeTask::errorMessage() const {
    return d_ptr->errorMessage;
}

void TranscodeTask::setErrorMessage(const QString &message) {
    Q_D(TranscodeTask);
    d->errorMessage = message;
    emit errorOccurred(message);
}

} // namespace ffmpeg_transform
