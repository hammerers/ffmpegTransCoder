#pragma once

#include "CommonTypes.h"
#include <QObject>
#include <memory>

namespace ffmpeg_transform {

class TranscodeEnginePrivate;

/**
 * @brief 基于全链路 FFmpeg C API 的音视频转码引擎 (遵循 Pimpl 规范)
 */
class TranscodeEngine : public QObject {
    Q_OBJECT

public:
    explicit TranscodeEngine(QObject *parent = nullptr);
    ~TranscodeEngine() override;

    /**
     * @brief 启动异步转码
     * @param config 转码配置项
     * @return 成功启动返回 true
     */
    bool start(const TranscodeConfig &config);

    /**
     * @brief 暂停转码
     */
    void pause();

    /**
     * @brief 恢复转码
     */
    void resume();

    /**
     * @brief 中止取消转码
     */
    void cancel();

    /**
     * @brief 获取当前任务状态
     */
    TaskState state() const;

    /**
     * @brief 获取当前转码进度与指标
     */
    TranscodeProgress progress() const;

    /**
     * @brief 获取当前转码配置
     */
    TranscodeConfig config() const;

signals:
    void stateChanged(TaskState newState);
    void progressUpdated(const TranscodeProgress &progress);
    void finished(bool success, const QString &message);

private:
    std::unique_ptr<TranscodeEnginePrivate> const d_ptr;
    Q_DECLARE_PRIVATE(TranscodeEngine)
};

} // namespace ffmpeg_transform
