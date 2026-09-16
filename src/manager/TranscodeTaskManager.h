#pragma once

#include "TranscodeTask.h"
#include <QObject>
#include <memory>
#include <QList>

namespace ffmpeg_transform {

class TranscodeTaskManagerPrivate;

/**
 * @brief 批量转码任务调度与队列管理器 (遵循 Pimpl 规范)
 */
class TranscodeTaskManager : public QObject {
    Q_OBJECT

public:
    explicit TranscodeTaskManager(QObject *parent = nullptr);
    ~TranscodeTaskManager() override;

    /**
     * @brief 添加视频文件至任务列表 (异步分析元数据与缩略图)
     */
    TranscodeTask* addTask(const QString &inputFilePath);

    /**
     * @brief 移除指定任务
     */
    void removeTask(const QString &taskId);

    /**
     * @brief 清空所有任务
     */
    void clearAllTasks();

    /**
     * @brief 获取所有任务列表
     */
    QList<TranscodeTask*> allTasks() const;

    /**
     * @brief 根据 ID 查找任务
     */
    TranscodeTask* getTask(const QString &taskId) const;

    // 批量控制接口
    void startAll();
    void pauseAll();
    void resumeAll();
    void cancelAll();

    // 单任务控制接口
    void startTask(const QString &taskId);
    void pauseTask(const QString &taskId);
    void resumeTask(const QString &taskId);
    void cancelTask(const QString &taskId);

    /**
     * @brief 设置最大并发转码数 (默认 1，保证系统稳定与流畅)
     */
    void setMaxConcurrentTasks(int count);
    int maxConcurrentTasks() const;

signals:
    void taskAdded(TranscodeTask *task);
    void taskRemoved(const QString &taskId);
    void taskStateChanged(TranscodeTask *task, TaskState state);
    void taskFrameRendered(TranscodeTask *task, const QImage &originFrame, const QImage &processedFrame, double ptsSec);
    void allTasksCompleted();

private:
    std::unique_ptr<TranscodeTaskManagerPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(TranscodeTaskManager)
};

} // namespace ffmpeg_transform
