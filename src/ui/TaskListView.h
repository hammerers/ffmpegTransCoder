#pragma once

#include "manager/TranscodeTaskManager.h"
#include <QWidget>
#include <memory>

namespace ffmpeg_transform {

class TaskListViewPrivate;
class TaskItemWidget;

/**
 * @brief 转码任务卡片列表容器视图 (遵循 Pimpl 与动态样式规范)
 */
class TaskListView : public QWidget {
    Q_OBJECT

public:
    explicit TaskListView(QWidget *parent = nullptr);
    ~TaskListView() override;

    void setManager(TranscodeTaskManager *manager);
    QString selectedTaskId() const;

signals:
    void taskSelected(const QString &taskId);

private:
    std::unique_ptr<TaskListViewPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(TaskListView)
};

} // namespace ffmpeg_transform
