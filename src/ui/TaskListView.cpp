#include "TaskListView.h"
#include "TaskItemWidget.h"
#include <QVBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QMap>
#include <QStackedLayout>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QStyle>

namespace ffmpeg_transform {

class TaskListViewPrivate {
public:
    TaskListView *q_ptr{nullptr};
    TranscodeTaskManager *manager{nullptr};

    QStackedLayout *stackedLayout{nullptr};
    QWidget *emptyPlaceholderWidget{nullptr};
    QLabel *emptyTitleLabel{nullptr};
    QLabel *emptySubtitleLabel{nullptr};

    QScrollArea *scrollArea{nullptr};
    QWidget *containerWidget{nullptr};
    QVBoxLayout *itemsLayout{nullptr};

    QMap<QString, TaskItemWidget*> itemWidgets;
    QString selectedTaskId;
    bool isDragging{false};

    void initUI() {
        q_ptr->setObjectName("taskListView");
        q_ptr->setAcceptDrops(true);

        stackedLayout = new QStackedLayout(q_ptr);
        stackedLayout->setContentsMargins(0, 0, 0, 0);

        // 1. 空状态灰色提示卡片 (没有任务时展示)
        emptyPlaceholderWidget = new QWidget(q_ptr);
        emptyPlaceholderWidget->setObjectName("taskEmptyPlaceholder");
        emptyPlaceholderWidget->setAcceptDrops(true);
        emptyPlaceholderWidget->installEventFilter(q_ptr);

        auto *phLayout = new QVBoxLayout(emptyPlaceholderWidget);
        phLayout->setAlignment(Qt::AlignCenter);
        phLayout->setSpacing(8);

        emptyTitleLabel = new QLabel("暂无视频处理任务", emptyPlaceholderWidget);
        emptyTitleLabel->setObjectName("taskEmptyTitle");
        emptyTitleLabel->setAlignment(Qt::AlignCenter);

        emptySubtitleLabel = new QLabel("可将视频文件拖拽至此，或点击上方“添加媒体”", emptyPlaceholderWidget);
        emptySubtitleLabel->setObjectName("taskEmptySubtitle");
        emptySubtitleLabel->setAlignment(Qt::AlignCenter);

        phLayout->addWidget(emptyTitleLabel);
        phLayout->addWidget(emptySubtitleLabel);

        // 2. 任务列表滚动容器
        scrollArea = new QScrollArea(q_ptr);
        scrollArea->setObjectName("taskListScrollArea");
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setAcceptDrops(true);
        scrollArea->viewport()->setAcceptDrops(true);
        scrollArea->viewport()->installEventFilter(q_ptr);

        containerWidget = new QWidget(scrollArea);
        containerWidget->setObjectName("taskListContainer");
        containerWidget->setAcceptDrops(true);
        containerWidget->installEventFilter(q_ptr);

        itemsLayout = new QVBoxLayout(containerWidget);
        itemsLayout->setContentsMargins(8, 8, 8, 8);
        itemsLayout->setSpacing(8);
        itemsLayout->addStretch();

        scrollArea->setWidget(containerWidget);

        stackedLayout->addWidget(emptyPlaceholderWidget);
        stackedLayout->addWidget(scrollArea);
        stackedLayout->setCurrentWidget(emptyPlaceholderWidget);
    }

    void addItem(TranscodeTask *task) {
        if (!task) return;

        stackedLayout->setCurrentWidget(scrollArea);

        auto *itemWidget = new TaskItemWidget(task, containerWidget);
        itemWidgets.insert(task->id(), itemWidget);

        // 插入到倒数第二位 (留出末尾的 stretch)
        itemsLayout->insertWidget(itemsLayout->count() - 1, itemWidget);

        QObject::connect(itemWidget, &TaskItemWidget::pauseClicked, [this](const QString &id) {
            if (manager) manager->pauseTask(id);
        });

        QObject::connect(itemWidget, &TaskItemWidget::resumeClicked, [this](const QString &id) {
            if (manager) manager->resumeTask(id);
        });

        QObject::connect(itemWidget, &TaskItemWidget::cancelClicked, [this](const QString &id) {
            if (manager) manager->cancelTask(id);
        });

        QObject::connect(itemWidget, &TaskItemWidget::removeClicked, [this](const QString &id) {
            if (manager) manager->removeTask(id);
        });

        QObject::connect(itemWidget, &TaskItemWidget::itemSelected, [this](const QString &id) {
            selectedTaskId = id;
            emit q_ptr->taskSelected(id);
        });

        // 默认选中新添加的任务
        selectedTaskId = task->id();
        emit q_ptr->taskSelected(selectedTaskId);
    }

    void removeItem(const QString &id) {
        if (itemWidgets.contains(id)) {
            auto *widget = itemWidgets.take(id);
            itemsLayout->removeWidget(widget);
            widget->deleteLater();
        }

        if (itemWidgets.isEmpty()) {
            stackedLayout->setCurrentWidget(emptyPlaceholderWidget);
            selectedTaskId.clear();
            emit q_ptr->taskSelected("");
        } else if (selectedTaskId == id) {
            selectedTaskId = itemWidgets.keys().first();
            emit q_ptr->taskSelected(selectedTaskId);
        }
    }
};

TaskListView::TaskListView(QWidget *parent)
    : QWidget(parent), d_ptr(std::make_unique<TaskListViewPrivate>()) {
    Q_D(TaskListView);
    d->q_ptr = this;
    d->initUI();
}

TaskListView::~TaskListView() = default;

void TaskListView::setManager(TranscodeTaskManager *manager) {
    Q_D(TaskListView);
    d->manager = manager;

    QObject::connect(manager, &TranscodeTaskManager::taskAdded, [this](TranscodeTask *t) {
        d_ptr->addItem(t);
    });

    QObject::connect(manager, &TranscodeTaskManager::taskRemoved, [this](const QString &id) {
        d_ptr->removeItem(id);
    });
}

QString TaskListView::selectedTaskId() const {
    return d_ptr->selectedTaskId;
}

bool TaskListView::isDragging() const {
    return d_ptr->isDragging;
}

void TaskListView::setIsDragging(bool dragging) {
    Q_D(TaskListView);
    if (d->isDragging == dragging) return;
    d->isDragging = dragging;
    style()->unpolish(this);
    style()->polish(this);
    update();
    emit draggingChanged(dragging);
}

void TaskListView::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        setIsDragging(true);
        event->acceptProposedAction();
    }
}

void TaskListView::dragMoveEvent(QDragMoveEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void TaskListView::dragLeaveEvent(QDragLeaveEvent *event) {
    setIsDragging(false);
    event->accept();
}

void TaskListView::dropEvent(QDropEvent *event) {
    setIsDragging(false);
    QStringList files;
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const auto &url : urls) {
        if (url.isLocalFile()) {
            files.append(url.toLocalFile());
        }
    }
    if (!files.isEmpty()) {
        emit filesDropped(files);
    }
    event->acceptProposedAction();
}

bool TaskListView::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::DragEnter) {
        auto *de = static_cast<QDragEnterEvent*>(event);
        dragEnterEvent(de);
        return true;
    } else if (event->type() == QEvent::DragMove) {
        auto *dm = static_cast<QDragMoveEvent*>(event);
        dragMoveEvent(dm);
        return true;
    } else if (event->type() == QEvent::DragLeave) {
        auto *dl = static_cast<QDragLeaveEvent*>(event);
        dragLeaveEvent(dl);
        return true;
    } else if (event->type() == QEvent::Drop) {
        auto *dp = static_cast<QDropEvent*>(event);
        dropEvent(dp);
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace ffmpeg_transform
