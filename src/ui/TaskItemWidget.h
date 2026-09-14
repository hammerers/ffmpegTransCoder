#pragma once

#include "core/CommonTypes.h"
#include "manager/TranscodeTask.h"
#include <QWidget>
#include <memory>

namespace ffmpeg_transform {

class TaskItemWidgetPrivate;

/**
 * @brief 单个转码任务卡片条目控件 (遵循 Pimpl 与动态样式规范)
 */
class TaskItemWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(TaskState state READ state WRITE setState NOTIFY stateChanged)

public:
    explicit TaskItemWidget(TranscodeTask *task, QWidget *parent = nullptr);
    ~TaskItemWidget() override;

    TranscodeTask* task() const;

    TaskState state() const;
    void setState(TaskState state);

signals:
    void stateChanged(TaskState state);
    void pauseClicked(const QString &taskId);
    void resumeClicked(const QString &taskId);
    void cancelClicked(const QString &taskId);
    void removeClicked(const QString &taskId);
    void openDirClicked(const QString &outputPath);
    void itemSelected(const QString &taskId);

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    std::unique_ptr<TaskItemWidgetPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(TaskItemWidget)
};

} // namespace ffmpeg_transform
