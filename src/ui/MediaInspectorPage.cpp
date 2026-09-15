#include "MediaInspectorPage.h"
#include "MediaInfoCard.h"
#include "manager/TranscodeTaskManager.h"
#include "manager/TranscodeTask.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileInfo>
#include <QUrl>
#include <QStyle>

namespace ffmpeg_transform {

class MediaInspectorPagePrivate {
public:
    MediaInspectorPage *q_ptr{nullptr};
    TranscodeTaskManager *manager{nullptr};

    MediaInfoCard *infoCard{nullptr};

    // 右侧垂直列表面板
    QWidget *rightPanel{nullptr};
    QLabel *rightHeaderTitle{nullptr};
    QLabel *rightBadgeLabel{nullptr};
    QListWidget *videoListWidget{nullptr};
    QLabel *emptyListHint{nullptr};

    QString selectedTaskId;

    void initUI() {
        q_ptr->setObjectName("mediaInspectorPage");
        q_ptr->setAcceptDrops(true);

        auto *rootLayout = new QHBoxLayout(q_ptr);
        rootLayout->setContentsMargins(16, 16, 16, 16);
        rootLayout->setSpacing(16);

        // 1. 左侧主体：页面标题 + 媒体信息/缩略图卡片
        auto *leftArea = new QWidget(q_ptr);
        leftArea->setObjectName("inspectorLeftArea");
        auto *leftLayout = new QVBoxLayout(leftArea);
        leftLayout->setContentsMargins(0, 0, 0, 0);
        leftLayout->setSpacing(12);

        auto *title = new QLabel("媒体信息深度解析", leftArea);
        title->setObjectName("inspectorPageTitle");
        leftLayout->addWidget(title);

        infoCard = new MediaInfoCard(leftArea);
        leftLayout->addWidget(infoCard, 1);

        rootLayout->addWidget(leftArea, 1);

        // 2. 右侧垂直列表：队列视频清单 (宽度 280px)
        rightPanel = new QWidget(q_ptr);
        rightPanel->setObjectName("inspectorRightPanel");
        rightPanel->setFixedWidth(280);

        auto *rightLayout = new QVBoxLayout(rightPanel);
        rightLayout->setContentsMargins(12, 12, 12, 12);
        rightLayout->setSpacing(10);

        // 右侧顶栏
        auto *headerLayout = new QHBoxLayout();
        headerLayout->setContentsMargins(0, 0, 0, 0);
        headerLayout->setSpacing(8);

        rightHeaderTitle = new QLabel("队列视频列表", rightPanel);
        rightHeaderTitle->setObjectName("inspectorListHeader");

        rightBadgeLabel = new QLabel("0 个文件", rightPanel);
        rightBadgeLabel->setObjectName("inspectorListBadge");

        headerLayout->addWidget(rightHeaderTitle);
        headerLayout->addStretch();
        headerLayout->addWidget(rightBadgeLabel);
        rightLayout->addLayout(headerLayout);

        // 列表控件
        videoListWidget = new QListWidget(rightPanel);
        videoListWidget->setObjectName("inspectorVideoList");
        videoListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
        videoListWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        videoListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        rightLayout->addWidget(videoListWidget, 1);

        // 空态提示
        emptyListHint = new QLabel("队列中暂无视频\n\n可直接拖放视频到此处\n或在【准备文件】中添加", rightPanel);
        emptyListHint->setObjectName("inspectorEmptyLabel");
        emptyListHint->setAlignment(Qt::AlignCenter);
        emptyListHint->setWordWrap(true);
        emptyListHint->setVisible(true);
        videoListWidget->setVisible(false);
        rightLayout->addWidget(emptyListHint, 1);

        rootLayout->addWidget(rightPanel);

        // 列表选中事件绑定
        QObject::connect(videoListWidget, &QListWidget::currentItemChanged, [this](QListWidgetItem *current, QListWidgetItem*) {
            if (!current) return;
            QString taskId = current->data(Qt::UserRole).toString();
            selectTaskInternal(taskId);
        });
    }

    void bindManager() {
        if (!manager) return;

        QObject::connect(manager, &TranscodeTaskManager::taskAdded, [this](TranscodeTask *task) {
            connectTaskSignals(task);
            refreshList();
        });

        QObject::connect(manager, &TranscodeTaskManager::taskRemoved, [this](const QString &id) {
            if (selectedTaskId == id) {
                selectedTaskId.clear();
            }
            refreshList();
        });

        for (auto *task : manager->allTasks()) {
            connectTaskSignals(task);
        }

        refreshList();
    }

    void connectTaskSignals(TranscodeTask *task) {
        if (!task) return;
        QObject::connect(task, &TranscodeTask::mediaInfoLoaded, [this, task](const MediaInfo &info) {
            updateTaskItemText(task);
            if (selectedTaskId == task->id()) {
                infoCard->setMedia(info, task->thumbnail());
            }
        });

        QObject::connect(task, &TranscodeTask::thumbnailLoaded, [this, task](const QImage &thumb) {
            if (selectedTaskId == task->id()) {
                infoCard->setMedia(task->mediaInfo(), thumb);
            }
        });
    }

    void updateTaskItemText(TranscodeTask *task) {
        for (int i = 0; i < videoListWidget->count(); ++i) {
            auto *item = videoListWidget->item(i);
            if (item && item->data(Qt::UserRole).toString() == task->id()) {
                QString name = QFileInfo(task->inputFilePath()).fileName();
                QString sub;
                if (task->mediaInfo().durationSec > 0.0) {
                    sub = QString("%1  |  %2").arg(task->mediaInfo().formattedDuration()).arg(task->mediaInfo().formattedFileSize());
                } else {
                    sub = "深度解析元数据中...";
                }
                item->setText(name + "\n" + sub);
                break;
            }
        }
    }

    void refreshList() {
        if (!manager) return;

        const auto &tasks = manager->allTasks();
        rightBadgeLabel->setText(QString("%1 个文件").arg(tasks.size()));

        if (tasks.isEmpty()) {
            videoListWidget->clear();
            videoListWidget->setVisible(false);
            emptyListHint->setVisible(true);
            infoCard->clearMedia();
            selectedTaskId.clear();
            return;
        }

        videoListWidget->setVisible(true);
        emptyListHint->setVisible(false);

        // 记录先前选中
        QString prevId = selectedTaskId;

        videoListWidget->blockSignals(true);
        videoListWidget->clear();

        QListWidgetItem *toSelect = nullptr;

        for (auto *task : tasks) {
            auto *item = new QListWidgetItem();
            QString name = QFileInfo(task->inputFilePath()).fileName();
            QString sub;
            if (task->mediaInfo().durationSec > 0.0) {
                sub = QString("%1  |  %2").arg(task->mediaInfo().formattedDuration()).arg(task->mediaInfo().formattedFileSize());
            } else {
                sub = "深度解析元数据中...";
            }
            item->setText(name + "\n" + sub);
            item->setData(Qt::UserRole, task->id());
            item->setToolTip(task->inputFilePath());

            videoListWidget->addItem(item);

            if (task->id() == prevId) {
                toSelect = item;
            }
        }

        videoListWidget->blockSignals(false);

        if (!toSelect && videoListWidget->count() > 0) {
            toSelect = videoListWidget->item(0);
        }

        if (toSelect) {
            videoListWidget->setCurrentItem(toSelect);
            selectTaskInternal(toSelect->data(Qt::UserRole).toString());
        }
    }

    void selectTaskInternal(const QString &taskId) {
        selectedTaskId = taskId;
        if (!manager) return;

        auto *t = manager->getTask(taskId);
        if (t) {
            infoCard->setMedia(t->mediaInfo(), t->thumbnail());
            emit q_ptr->taskSelected(taskId);
        } else {
            infoCard->clearMedia();
        }
    }
};

MediaInspectorPage::MediaInspectorPage(QWidget *parent)
    : QWidget(parent), d_ptr(std::make_unique<MediaInspectorPagePrivate>()) {
    Q_D(MediaInspectorPage);
    d->q_ptr = this;
    d->initUI();
}

MediaInspectorPage::~MediaInspectorPage() = default;

void MediaInspectorPage::setManager(TranscodeTaskManager *manager) {
    Q_D(MediaInspectorPage);
    d->manager = manager;
    d->bindManager();
}

void MediaInspectorPage::setMedia(const MediaInfo &info, const QImage &thumbnail) {
    d_ptr->infoCard->setMedia(info, thumbnail);
}

void MediaInspectorPage::clearMedia() {
    d_ptr->infoCard->clearMedia();
}

void MediaInspectorPage::selectTask(const QString &taskId) {
    Q_D(MediaInspectorPage);
    for (int i = 0; i < d->videoListWidget->count(); ++i) {
        auto *item = d->videoListWidget->item(i);
        if (item && item->data(Qt::UserRole).toString() == taskId) {
            d->videoListWidget->setCurrentItem(item);
            d->selectTaskInternal(taskId);
            break;
        }
    }
}

void MediaInspectorPage::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MediaInspectorPage::dropEvent(QDropEvent *event) {
    Q_D(MediaInspectorPage);
    const QList<QUrl> urls = event->mimeData()->urls();
    QStringList files;
    for (const auto &url : urls) {
        if (url.isLocalFile()) {
            files.append(url.toLocalFile());
            if (d->manager) {
                d->manager->addTask(url.toLocalFile());
            }
        }
    }
    if (!files.isEmpty()) {
        emit filesDropped(files);
    }
    event->acceptProposedAction();
}

} // namespace ffmpeg_transform
