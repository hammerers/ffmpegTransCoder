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
    Q_PROPERTY(bool isDragging READ isDragging WRITE setIsDragging NOTIFY draggingChanged)

public:
    explicit TaskListView(QWidget *parent = nullptr);
    ~TaskListView() override;

    void setManager(TranscodeTaskManager *manager);
    QString selectedTaskId() const;

    bool isDragging() const;
    void setIsDragging(bool dragging);

signals:
    void taskSelected(const QString &taskId);
    void filesDropped(const QStringList &files);
    void draggingChanged(bool isDragging);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    std::unique_ptr<TaskListViewPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(TaskListView)
};

} // namespace ffmpeg_transform
