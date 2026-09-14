#include "MainWindow.h"
#include "NavSidebar.h"
#include "HomePage.h"
#include "QueuePage.h"
#include "FilePrepPage.h"
#include "ParamConsolePage.h"
#include "MediaInspectorPage.h"
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

    QLabel *perfInfoLabel{nullptr};
    QLabel *topStatusBadge{nullptr};
    QTimer *perfTimer{nullptr};

    QString currentSelectedTaskId;
    bool hasTasks{false};

    void initUI() {
        q_ptr->setObjectName("mainWindow");
        q_ptr->setWindowTitle("FFmpeg Transcoder Pro - 高性能音视频转码工作台 (Native C API)");
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

        topStatusBadge = new QLabel("Native C API 运行中", topBar);
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

        // 创建辅助功能页面占位
        auto createPlaceholderPage = [this](const QString &title, const QString &desc) -> QWidget* {
            auto *w = new QWidget(pageStack);
            auto *l = new QVBoxLayout(w);
            l->setAlignment(Qt::AlignCenter);
            l->setSpacing(8);
            auto *t = new QLabel(title, w);
            t->setObjectName("paramSectionTitle");
            t->setAlignment(Qt::AlignCenter);
            auto *d = new QLabel(desc, w);
            d->setObjectName("paramSectionSubtitle");
            d->setAlignment(Qt::AlignCenter);
            l->addWidget(t);
            l->addWidget(d);
            return w;
        };

        QWidget *perfPage = createPlaceholderPage("性能监控面板", "实时捕获硬件编解码利用率、帧率吞吐量与内存占用");
        QWidget *toolsPage = createPlaceholderPage("集成工具箱", "包含视频无损截取、音频提取、字幕压制与色彩空间转换工具");
        QWidget *settingsPage = createPlaceholderPage("软件设置", "配置默认输出路径、线程池大小与 GPU 硬件加速首选项");
        QWidget *aboutPage = createPlaceholderPage("关于系统", "FFmpeg Transcoder Pro 6.0\n全链路基于原生 C API 构建，具备高吞吐量与专业压制调校能力");

        pageStack->addWidget(homePage);            // 0: 起始页面
        pageStack->addWidget(queuePage);           // 1: 编码队列
        pageStack->addWidget(filePrepPage);        // 2: 准备文件
        pageStack->addWidget(paramPage);           // 3: 参数面板
        pageStack->addWidget(mediaInspectorPage);  // 4: 媒体信息
        pageStack->addWidget(perfPage);            // 5: 性能监控
        pageStack->addWidget(toolsPage);           // 6: 集成工具
        pageStack->addWidget(settingsPage);        // 7: 软件设置
        pageStack->addWidget(aboutPage);           // 8: 关于系统

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
                    manager.addTask(file);
                }
            }
            q_ptr->setHasTasks(!manager.allTasks().isEmpty());
        };

        QObject::connect(filePrepPage, &FilePrepPage::enqueueFilesRequested, [this, onAddFiles](const QStringList &files) {
            onAddFiles(files);
            navSidebar->setCurrentIndex(1); // 自动切到编码队列
        });

        QObject::connect(queuePage, &QueuePage::filesDropped, onAddFiles);

        // 任务选中联动
        QObject::connect(queuePage, &QueuePage::taskSelected, [this](const QString &id) {
            currentSelectedTaskId = id;
            auto *t = manager.getTask(id);
            if (t) {
                mediaInspectorPage->setMedia(t->mediaInfo(), t->thumbnail());
                paramPage->setConfig(t->config());
            } else {
                mediaInspectorPage->clearMedia();
            }
        });

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
        QString perfStr = QString("FFmpeg Transcoder Pro  |  RAM %1 MB  |  任务队列: %2  |  Native C Engine Ready")
            .arg(ramMB).arg(taskCount);
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
