#pragma once

#include "manager/TranscodeTaskManager.h"
#include <QWidget>
#include <memory>

namespace ffmpeg_transform {

class QueuePagePrivate;
class TaskListView;

/**
 * @brief 编码队列工作区 (遵循 Pimpl 与动态样式规范)
 */
class QueuePage : public QWidget {
    Q_OBJECT

public:
    explicit QueuePage(QWidget *parent = nullptr);
    ~QueuePage() override;

    void setManager(TranscodeTaskManager *manager);
    TaskListView* taskListView() const;

signals:
    void taskSelected(const QString &taskId);
    void filesDropped(const QStringList &files);
    void applyParamsToAllRequested();

private:
    std::unique_ptr<QueuePagePrivate> const d_ptr;
    Q_DECLARE_PRIVATE(QueuePage)
};

} // namespace ffmpeg_transform
