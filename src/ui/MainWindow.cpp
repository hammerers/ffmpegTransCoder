#include "MainWindow.h"
#include "NavSidebar.h"
#include "HomePage.h"
#include "QueuePage.h"
#include "FilePrepPage.h"
#include "ParamConsolePage.h"
#include "MediaInspectorPage.h"
#include "LiveMonitorPage.h"
#include "manager/TranscodeTaskManager.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QLabel>
#include <QTimer>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileInfo>
#include <QStyle>
#include <QStatusBar>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

namespace ffmpeg_transform {

class MainWindowPrivate {
public:
    MainWindow *q_ptr{nullptr};
    TranscodeTaskManager manager;

    NavSidebar *navSidebar{nullptr};
    QStackedWidget *pageStack{nullptr};

    HomePage *homePage{nullptr};
    QueuePage *queuePage{nullptr};
    FilePrepPage *filePrepPage{nullptr};
    ParamConsolePage *paramPage{nullptr};
    MediaInspectorPage *mediaInspectorPage{nullptr};
    LiveMonitorPage *liveMonitorPage{nullptr};

    QLabel *perfInfoLabel{nullptr};
    QLabel *topStatusBadge{nullptr};
    QTimer *perfTimer{nullptr};

    QString currentSelectedTaskId;
    bool hasTasks{false};

    void initUI() {
        q_ptr->setObjectName("mainWindow");
        q_ptr->setWindowTitle("FFmpeg Transcoder Pro - 高性能音视频转码工作台");
        q_ptr->resize(1200, 780);
        q_ptr->setMinimumSize(1000, 650);
        q_ptr->setAcceptDrops(true);

        auto *centralWidget = new QWidget(q_ptr);
        auto *rootLayout = new QVBoxLayout(centralWidget);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(0);

        // 1. 顶部性能监视与标识条 (对应截图顶部风格)
        auto *topBar = new QWidget(centralWidget);
        topBar->setObjectName("topInfoBar");
        auto *topLayout = new QHBoxLayout(topBar);
        topLayout->setContentsMargins(14, 6, 14, 6);
        topLayout->setSpacing(12);

        auto *appIcon = new QLabel("TRANSCODER", topBar);
        appIcon->setObjectName("topAppIcon");

        perfInfoLabel = new QLabel("FFmpeg Transcoder Pro  |  CPU 0.4%  |  RAM 86M / 240M  |  GPU 0.0% 114M + 4M", topBar);
        perfInfoLabel->setObjectName("topPerfLabel");

        topStatusBadge = new QLabel("转码引擎已就绪", topBar);
        topStatusBadge->setObjectName("topStatusBadge");

        topLayout->addWidget(appIcon);
        topLayout->addWidget(perfInfoLabel);
        topLayout->addStretch();
        topLayout->addWidget(topStatusBadge);

        rootLayout->addWidget(topBar);

        // 2. 主体：左侧导航栏 + 右侧工作区堆栈
        auto *bodyWidget = new QWidget(centralWidget);
        auto *bodyLayout = new QHBoxLayout(bodyWidget);
        bodyLayout->setContentsMargins(0, 0, 0, 0);
        bodyLayout->setSpacing(0);

        navSidebar = new NavSidebar(bodyWidget);
        pageStack = new QStackedWidget(bodyWidget);
        pageStack->setObjectName("workspaceStack");

        // 初始化工作区页面
        homePage = new HomePage(pageStack);
        queuePage = new QueuePage(pageStack);
        queuePage->setManager(&manager);
        filePrepPage = new FilePrepPage(pageStack);
        paramPage = new ParamConsolePage(pageStack);
        mediaInspectorPage = new MediaInspectorPage(pageStack);
        mediaInspectorPage->setManager(&manager);
        liveMonitorPage = new LiveMonitorPage(pageStack);
        liveMonitorPage->setManager(&manager);

        pageStack->addWidget(homePage);            // 0: 起始页面
        pageStack->addWidget(queuePage);           // 1: 编码队列
        pageStack->addWidget(filePrepPage);        // 2: 准备文件
        pageStack->addWidget(paramPage);           // 3: 参数面板
        pageStack->addWidget(mediaInspectorPage);  // 4: 媒体信息
        pageStack->addWidget(liveMonitorPage);     // 5: 实时检视

        bodyLayout->addWidget(navSidebar);
        bodyLayout->addWidget(pageStack, 1);
        rootLayout->addWidget(bodyWidget, 1);

        q_ptr->setCentralWidget(centralWidget);

        bindSignals();
        initPerfMonitor();
    }

    void bindSignals() {
        // 侧边栏切换页面联动
        QObject::connect(navSidebar, &NavSidebar::currentChanged, [this](int index) {
            pageStack->setCurrentIndex(index);
            if (index == 5) {
                liveMonitorPage->checkAndLoadPreview();
            }
        });

        // 实时双分屏画面渲染联动
        QObject::connect(&manager, &TranscodeTaskManager::taskFrameRendered,
                         [this](TranscodeTask *task, const QImage &origin, const QImage &processed, double pts) {
            QString name = task ? (task->mediaInfo().fileName.isEmpty() ? QFileInfo(task->inputFilePath()).fileName() : task->mediaInfo().fileName) : "视频流";
            liveMonitorPage->updateLiveFrame(name, origin, processed, pts);
        });

        // 任务结束保留画面并展示完成态
        QObject::connect(&manager, &TranscodeTaskManager::allTasksCompleted, [this]() {
            liveMonitorPage->showCompletedState();
        });

        // 实时去水印参数双向同步
        QObject::connect(liveMonitorPage, &LiveMonitorPage::delogoConfigChanged, [this](const DelogoConfig &cfg) {
            TranscodeConfig c = paramPage->config();
            c.delogo = cfg;
            paramPage->setConfig(c);
        });

        // 起始页快捷跳转
        QObject::connect(homePage, &HomePage::navigateToQueue, [this]() {
            navSidebar->setCurrentIndex(1);
        });
        QObject::connect(homePage, &HomePage::navigateToFilePrep, [this]() {
            navSidebar->setCurrentIndex(2);
        });
        QObject::connect(homePage, &HomePage::navigateToParams, [this]() {
            navSidebar->setCurrentIndex(3);
        });

        // 批量添加文件联动
        auto onAddFiles = [this](const QStringList &files) {
            for (const auto &file : files) {
                if (QFileInfo::exists(file)) {
                    auto *t = manager.addTask(file);
                    // 默认采用参数面板当前配置
                    if (t && paramPage) {
                        TranscodeConfig newCfg = paramPage->config();
                        newCfg.inputPath = t->inputFilePath();
                        QFileInfo fi(t->inputFilePath());
                        QString ext = newCfg.containerFormat;
                        newCfg.outputPath = fi.absolutePath() + "/" + fi.completeBaseName() + "_converted." + ext;
                        t->setConfig(newCfg);
                    }
                }
            }
            q_ptr->setHasTasks(!manager.allTasks().isEmpty());
        };

        QObject::connect(filePrepPage, &FilePrepPage::enqueueFilesRequested, [this, onAddFiles](const QStringList &files) {
            onAddFiles(files);
            navSidebar->setCurrentIndex(1); // 自动切到编码队列
        });

        QObject::connect(queuePage, &QueuePage::filesDropped, onAddFiles);

        // 任务选中联动 (队列页面选中)
        QObject::connect(queuePage, &QueuePage::taskSelected, [this](const QString &id) {
            currentSelectedTaskId = id;
            mediaInspectorPage->selectTask(id);
            auto *t = manager.getTask(id);
            if (t) {
                paramPage->setConfig(t->config());
            }
        });

        // 媒体信息页选中联动
        QObject::connect(mediaInspectorPage, &MediaInspectorPage::taskSelected, [this](const QString &id) {
            currentSelectedTaskId = id;
            auto *t = manager.getTask(id);
            if (t) {
                paramPage->setConfig(t->config());
            }
        });

        // 媒体信息页拖拽加入文件联动
        QObject::connect(mediaInspectorPage, &MediaInspectorPage::filesDropped, onAddFiles);

        // 预设修改更新到当前选中的任务
        QObject::connect(paramPage, &ParamConsolePage::configChanged, [this](const TranscodeConfig &cfg) {
            auto *t = manager.getTask(currentSelectedTaskId);
            if (t) {
                TranscodeConfig newCfg = cfg;
                newCfg.inputPath = t->inputFilePath();

                QFileInfo fi(t->inputFilePath());
                QString ext = cfg.containerFormat;
                newCfg.outputPath = fi.absolutePath() + "/" + fi.completeBaseName() + "_converted." + ext;
                t->setConfig(newCfg);
            }
        });

        // 应用参数到队列所有视频统一联动
        auto applyConfigToAll = [this](const TranscodeConfig &cfg) {
            for (auto *t : manager.allTasks()) {
                if (t->state() != TaskState::Converting && t->state() != TaskState::Completed) {
                    TranscodeConfig newCfg = cfg;
                    newCfg.inputPath = t->inputFilePath();
                    QFileInfo fi(t->inputFilePath());
                    QString ext = cfg.containerFormat;
                    newCfg.outputPath = fi.absolutePath() + "/" + fi.completeBaseName() + "_converted." + ext;
                    t->setConfig(newCfg);
                }
            }
        };

        QObject::connect(paramPage, &ParamConsolePage::applyToAllRequested, applyConfigToAll);
        QObject::connect(queuePage, &QueuePage::applyParamsToAllRequested, [this, applyConfigToAll]() {
            if (paramPage) {
                applyConfigToAll(paramPage->config());
            }
        });

        // 任务队列整体状态
        QObject::connect(&manager, &TranscodeTaskManager::taskStateChanged, [this](TranscodeTask*, TaskState) {
            q_ptr->setHasTasks(!manager.allTasks().isEmpty());
        });
    }

    void initPerfMonitor() {
        perfTimer = new QTimer(q_ptr);
        QObject::connect(perfTimer, &QTimer::timeout, [this]() {
            updateSystemResourceText();
        });
        perfTimer->start(2000);
        updateSystemResourceText();
    }

    void updateSystemResourceText() {
        int ramMB = 85;
#ifdef Q_OS_WIN
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
            ramMB = static_cast<int>(pmc.WorkingSetSize / (1024 * 1024));
        }
#endif
        int taskCount = manager.allTasks().size();
        int runningCount = 0;
        int pendingCount = 0;
        for (auto *t : manager.allTasks()) {
            if (t->state() == TaskState::Converting) runningCount++;
            else if (t->state() == TaskState::Pending) pendingCount++;
        }
        QString stateStr = "工作台就绪";
        if (runningCount > 0) {
            stateStr = QString("正在转码 (%1 活跃)").arg(runningCount);
        } else if (pendingCount > 0) {
            stateStr = QString("%1 个任务待命 (可点击开始转码)").arg(pendingCount);
        } else if (!manager.allTasks().isEmpty()) {
            stateStr = "转码已完成";
        }
        QString perfStr = QString("FFmpeg Transcoder Pro  |  RAM %1 MB  |  任务队列: %2  |  %3")
            .arg(ramMB).arg(taskCount).arg(stateStr);
        perfInfoLabel->setText(perfStr);
    }
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), d_ptr(std::make_unique<MainWindowPrivate>()) {
    Q_D(MainWindow);
    d->q_ptr = this;
    d->initUI();
}

MainWindow::~MainWindow() = default;

bool MainWindow::hasTasks() const {
    return d_ptr->hasTasks;
}

void MainWindow::setHasTasks(bool hasTasks) {
    Q_D(MainWindow);
    if (d->hasTasks == hasTasks) return;
    d->hasTasks = hasTasks;

    style()->unpolish(this);
    style()->polish(this);
    update();

    emit tasksStateChanged(hasTasks);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event) {
    const QList<QUrl> urls = event->mimeData()->urls();
    QStringList files;
    for (const auto &url : urls) {
        if (url.isLocalFile()) {
            files.append(url.toLocalFile());
            d_ptr->manager.addTask(url.toLocalFile());
        }
    }
    setHasTasks(!d_ptr->manager.allTasks().isEmpty());
    if (!files.isEmpty()) {
        d_ptr->navSidebar->setCurrentIndex(1); // 自动切到编码队列
    }
    event->acceptProposedAction();
}

} // namespace ffmpeg_transform
