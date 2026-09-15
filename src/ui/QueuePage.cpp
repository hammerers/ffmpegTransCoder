#include "QueuePage.h"
#include "TaskListView.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QProcess>
#include <QDir>
#include <QTimer>

namespace ffmpeg_transform {

class QueuePagePrivate {
public:
    QueuePage *q_ptr{nullptr};
    TranscodeTaskManager *manager{nullptr};

    TaskListView *taskListView{nullptr};

    // 操作按钮
    QPushButton *startBtn{nullptr};
    QPushButton *pauseBtn{nullptr};
    QPushButton *resumeBtn{nullptr};
    QPushButton *stopBtn{nullptr};
    QPushButton *removeBtn{nullptr};
    QPushButton *resetBtn{nullptr};
    QPushButton *applyParamsBtn{nullptr};
    QPushButton *locateBtn{nullptr};

    // 统计标签
    QLabel *statTotalLabel{nullptr};
    QLabel *statRunningLabel{nullptr};
    QLabel *statErrorLabel{nullptr};

    void initUI() {
        q_ptr->setObjectName("queuePage");

        auto *mainLayout = new QVBoxLayout(q_ptr);
        mainLayout->setContentsMargins(16, 12, 16, 12);
        mainLayout->setSpacing(10);

        // 1. 顶部任务管理菜单栏
        auto *menuBar = new QWidget(q_ptr);
        menuBar->setObjectName("queueMenuBar");
        auto *menuLayout = new QHBoxLayout(menuBar);
        menuLayout->setContentsMargins(0, 4, 0, 8);
        menuLayout->setSpacing(14);

        auto *titleLabel = new QLabel("任务管理菜单", menuBar);
        titleLabel->setObjectName("queueMenuTitle");
        menuLayout->addWidget(titleLabel);
        menuLayout->addSpacing(8);

        auto createActionBtn = [menuBar](const QString &text, const QString &objName) -> QPushButton* {
            auto *btn = new QPushButton(text, menuBar);
            btn->setObjectName(objName);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setFlat(true);
            return btn;
        };

        startBtn = createActionBtn("开始", "btnActionStart");
        pauseBtn = createActionBtn("暂停", "btnActionPause");
        resumeBtn = createActionBtn("恢复", "btnActionResume");
        stopBtn = createActionBtn("停止", "btnActionStop");
        removeBtn = createActionBtn("移除", "btnActionRemove");
        resetBtn = createActionBtn("重置", "btnActionReset");
        applyParamsBtn = createActionBtn("应用参数", "btnActionApplyAll");
        locateBtn = createActionBtn("定位", "btnActionLocate");

        menuLayout->addWidget(startBtn);
        menuLayout->addWidget(pauseBtn);
        menuLayout->addWidget(resumeBtn);
        menuLayout->addWidget(stopBtn);
        menuLayout->addWidget(removeBtn);
        menuLayout->addWidget(resetBtn);
        menuLayout->addWidget(applyParamsBtn);
        menuLayout->addWidget(locateBtn);
        menuLayout->addStretch();

        // 状态计数
        statTotalLabel = new QLabel("总数 0", menuBar);
        statTotalLabel->setObjectName("statTotalLabel");

        statRunningLabel = new QLabel("运行 0", menuBar);
        statRunningLabel->setObjectName("statRunningLabel");

        statErrorLabel = new QLabel("错误 0", menuBar);
        statErrorLabel->setObjectName("statErrorLabel");

        menuLayout->addWidget(statTotalLabel);
        menuLayout->addWidget(statRunningLabel);
        menuLayout->addWidget(statErrorLabel);

        mainLayout->addWidget(menuBar);

        // 2. 队列列表表头
        auto *headerBar = new QWidget(q_ptr);
        headerBar->setObjectName("queueTableHeader");
        auto *headerLayout = new QHBoxLayout(headerBar);
        headerLayout->setContentsMargins(12, 6, 12, 6);
        headerLayout->setSpacing(10);

        auto addColHeader = [headerBar, headerLayout](const QString &text, int stretch = 0, int fixedWidth = 0) {
            auto *l = new QLabel(text, headerBar);
            l->setObjectName("queueHeaderCol");
            if (fixedWidth > 0) l->setFixedWidth(fixedWidth);
            headerLayout->addWidget(l, stretch);
        };

        addColHeader("任务名称", 1);
        addColHeader("状态", 0, 70);
        addColHeader("进度", 0, 100);
        addColHeader("效率", 0, 110);
        addColHeader("大小/预估", 0, 90);
        addColHeader("质量", 0, 70);
        addColHeader("比特率", 0, 80);
        addColHeader("操作/定位", 0, 120);

        mainLayout->addWidget(headerBar);

        // 3. 任务队列容器
        taskListView = new TaskListView(q_ptr);
        mainLayout->addWidget(taskListView, 1);

        // 信号传递
        QObject::connect(taskListView, &TaskListView::taskSelected, q_ptr, &QueuePage::taskSelected);
        QObject::connect(taskListView, &TaskListView::filesDropped, q_ptr, &QueuePage::filesDropped);

        bindActions();
    }

    void bindActions() {
        QObject::connect(startBtn, &QPushButton::clicked, [this]() {
            if (manager) manager->startAll();
        });

        QObject::connect(pauseBtn, &QPushButton::clicked, [this]() {
            if (manager) manager->pauseAll();
        });

        QObject::connect(resumeBtn, &QPushButton::clicked, [this]() {
            if (manager) manager->resumeAll();
        });

        QObject::connect(stopBtn, &QPushButton::clicked, [this]() {
            if (manager) manager->cancelAll();
        });

        QObject::connect(removeBtn, &QPushButton::clicked, [this]() {
            if (manager) manager->clearAllTasks();
        });

        QObject::connect(resetBtn, &QPushButton::clicked, [this]() {
            if (manager) {
                // 重置所有任务为待处理
                for (auto *t : manager->allTasks()) {
                    if (t->state() == TaskState::Completed || t->state() == TaskState::Failed || t->state() == TaskState::Canceled) {
                        t->setState(TaskState::Pending);
                    }
                }
                updateStats();
            }
        });

        QObject::connect(applyParamsBtn, &QPushButton::clicked, [this]() {
            emit q_ptr->applyParamsToAllRequested();
            applyParamsBtn->setText("已应用");
            QTimer::singleShot(1800, [this]() {
                if (applyParamsBtn) applyParamsBtn->setText("应用参数");
            });
        });

        QObject::connect(locateBtn, &QPushButton::clicked, [this]() {
            if (!manager) return;
            QString selId = taskListView->selectedTaskId();
            auto *t = manager->getTask(selId);
            QString targetPath = t ? t->config().outputPath : "";
            if (targetPath.isEmpty() && !manager->allTasks().isEmpty()) {
                targetPath = manager->allTasks().first()->config().outputPath;
            }

            if (!targetPath.isEmpty() && QFileInfo::exists(targetPath)) {
#ifdef Q_OS_WIN
                QStringList args;
                args << "/select," << QDir::toNativeSeparators(targetPath);
                QProcess::startDetached("explorer.exe", args);
#else
                QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(targetPath).absolutePath()));
#endif
            } else if (!targetPath.isEmpty()) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(targetPath).absolutePath()));
            }
        });
    }

    void updateStats() {
        if (!manager) return;
        auto tasks = manager->allTasks();
        int running = 0, error = 0;
        for (auto *t : tasks) {
            if (t->state() == TaskState::Converting) {
                running++;
            } else if (t->state() == TaskState::Failed) {
                error++;
            }
        }
        statTotalLabel->setText(QString("总数 %1").arg(tasks.size()));
        statRunningLabel->setText(QString("运行 %1").arg(running));
        statErrorLabel->setText(QString("错误 %1").arg(error));
    }
};

QueuePage::QueuePage(QWidget *parent)
    : QWidget(parent), d_ptr(std::make_unique<QueuePagePrivate>()) {
    Q_D(QueuePage);
    d->q_ptr = this;
    d->initUI();
}

QueuePage::~QueuePage() = default;

void QueuePage::setManager(TranscodeTaskManager *manager) {
    Q_D(QueuePage);
    d->manager = manager;
    d->taskListView->setManager(manager);

    QObject::connect(manager, &TranscodeTaskManager::taskAdded, [this](TranscodeTask*) {
        d_ptr->updateStats();
    });
    QObject::connect(manager, &TranscodeTaskManager::taskRemoved, [this](const QString&) {
        d_ptr->updateStats();
    });
    QObject::connect(manager, &TranscodeTaskManager::taskStateChanged, [this](TranscodeTask*, TaskState) {
        d_ptr->updateStats();
    });
}

TaskListView* QueuePage::taskListView() const {
    return d_ptr->taskListView;
}

} // namespace ffmpeg_transform
