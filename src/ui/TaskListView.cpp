#include "TaskListView.h"
#include "TaskItemWidget.h"
#include <QVBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QMap>

namespace ffmpeg_transform {

class TaskListViewPrivate {
public:
    TaskListView *q_ptr{nullptr};
    TranscodeTaskManager *manager{nullptr};

    QScrollArea *scrollArea{nullptr};
    QWidget *containerWidget{nullptr};
    QVBoxLayout *itemsLayout{nullptr};
    QLabel *emptyLabel{nullptr};

    QMap<QString, TaskItemWidget*> itemWidgets;
    QString selectedTaskId;

    void initUI() {
        auto *mainLayout = new QVBoxLayout(q_ptr);
        mainLayout->setContentsMargins(0, 0, 0, 0);

        scrollArea = new QScrollArea(q_ptr);
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);

        containerWidget = new QWidget(scrollArea);
        itemsLayout = new QVBoxLayout(containerWidget);
        itemsLayout->setContentsMargins(8, 8, 8, 8);
        itemsLayout->setSpacing(8);

        emptyLabel = new QLabel("列表为空，请拖入或添加需要转换的视频", containerWidget);
        emptyLabel->setObjectName("listEmptyLabel");
        emptyLabel->setAlignment(Qt::AlignCenter);
        itemsLayout->addWidget(emptyLabel);
        itemsLayout->addStretch();

        scrollArea->setWidget(containerWidget);
        mainLayout->addWidget(scrollArea);
    }

    void addItem(TranscodeTask *task) {
        if (!task) return;

        if (emptyLabel->isVisible()) {
            emptyLabel->setVisible(false);
        }

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
            emptyLabel->setVisible(true);
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

} // namespace ffmpeg_transform
