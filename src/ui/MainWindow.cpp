#include "MainWindow.h"
#include "DropAreaWidget.h"
#include "MediaInfoCard.h"
#include "PresetPanel.h"
#include "TaskListView.h"
#include "manager/TranscodeTaskManager.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QStyle>
#include <QStatusBar>
#include <QScrollArea>
#include <QMessageBox>

namespace ffmpeg_transform {

class MainWindowPrivate {
public:
    MainWindow *q_ptr{nullptr};
    TranscodeTaskManager manager;

    DropAreaWidget *dropArea{nullptr};
    TaskListView *taskListView{nullptr};
    MediaInfoCard *mediaInfoCard{nullptr};
    PresetPanel *presetPanel{nullptr};

    QPushButton *addFileBtn{nullptr};
    QPushButton *startAllBtn{nullptr};
    QPushButton *pauseAllBtn{nullptr};
    QPushButton *cancelAllBtn{nullptr};
    QPushButton *clearAllBtn{nullptr};
    QPushButton *applyToAllBtn{nullptr};

    QLabel *statusLabel{nullptr};
    QString currentSelectedTaskId;
    bool hasTasks{false};

    void initUI() {
        q_ptr->setObjectName("mainWindow");
        q_ptr->setWindowTitle("FFmpeg 专业视频格式转换器 - 桌面客户端 (Native C API)");
        q_ptr->resize(1180, 760);
        q_ptr->setMinimumSize(950, 600);
        q_ptr->setAcceptDrops(true);

        auto *centralWidget = new QWidget(q_ptr);
        auto *rootLayout = new QVBoxLayout(centralWidget);
        rootLayout->setContentsMargins(16, 16, 16, 16);
        rootLayout->setSpacing(12);

        // 1. 顶部一体化命令工具栏
        auto *headerWidget = new QWidget(centralWidget);
        headerWidget->setObjectName("topBarWidget");
        auto *headerLayout = new QHBoxLayout(headerWidget);
        headerLayout->setContentsMargins(8, 4, 8, 4);
        headerLayout->setSpacing(8);

        auto *logoLabel = new QLabel("🎬 FFmpeg Transcoder Pro", headerWidget);
        logoLabel->setObjectName("brandLogoLabel");

        auto *verBadge = new QLabel("Native C API", headerWidget);
        verBadge->setObjectName("brandVersionBadge");

        auto *sloganLabel = new QLabel("· 专业级音视频压制工具", headerWidget);
        sloganLabel->setObjectName("brandSloganLabel");

        addFileBtn = new QPushButton("➕ 添加媒体", headerWidget);
        addFileBtn->setObjectName("btnPrimary");
        addFileBtn->setCursor(Qt::PointingHandCursor);

        startAllBtn = new QPushButton("🚀 全部开始", headerWidget);
        startAllBtn->setObjectName("btnSuccess");
        startAllBtn->setCursor(Qt::PointingHandCursor);

        pauseAllBtn = new QPushButton("⏸️ 暂停全部", headerWidget);
        pauseAllBtn->setObjectName("btnWarning");
        pauseAllBtn->setCursor(Qt::PointingHandCursor);

        cancelAllBtn = new QPushButton("⏹️ 终止全部", headerWidget);
        cancelAllBtn->setObjectName("btnDanger");
        cancelAllBtn->setCursor(Qt::PointingHandCursor);

        clearAllBtn = new QPushButton("🗑️ 清空队列", headerWidget);
        clearAllBtn->setCursor(Qt::PointingHandCursor);

        headerLayout->addWidget(logoLabel);
        headerLayout->addWidget(verBadge);
        headerLayout->addWidget(sloganLabel);
        headerLayout->addStretch();
        headerLayout->addWidget(addFileBtn);
        headerLayout->addWidget(startAllBtn);
        headerLayout->addWidget(pauseAllBtn);
        headerLayout->addWidget(cancelAllBtn);
        headerLayout->addWidget(clearAllBtn);
        rootLayout->addWidget(headerWidget);

        // 2. 主体左右分栏
        auto *splitter = new QSplitter(Qt::Horizontal, centralWidget);
        splitter->setHandleWidth(4);

        // 左侧面板：拖拽区 + 任务队列
        auto *leftPanel = new QWidget(splitter);
        auto *leftLayout = new QVBoxLayout(leftPanel);
        leftLayout->setContentsMargins(0, 0, 8, 0);
        leftLayout->setSpacing(10);

        dropArea = new DropAreaWidget(leftPanel);
        dropArea->setFixedHeight(120);

        taskListView = new TaskListView(leftPanel);
        taskListView->setManager(&manager);

        leftLayout->addWidget(dropArea);
        leftLayout->addWidget(taskListView, 1);
        splitter->addWidget(leftPanel);

        // 右侧面板：选中任务元数据展示 + 预设调节
        auto *rightPanel = new QWidget(splitter);
        auto *rightLayout = new QVBoxLayout(rightPanel);
        rightLayout->setContentsMargins(8, 0, 0, 0);
        rightLayout->setSpacing(10);

        auto *rightScroll = new QScrollArea(rightPanel);
        rightScroll->setWidgetResizable(true);
        rightScroll->setFrameShape(QFrame::NoFrame);

        auto *rightContainer = new QWidget(rightScroll);
        auto *rcLayout = new QVBoxLayout(rightContainer);
        rcLayout->setContentsMargins(0, 0, 0, 0);
        rcLayout->setSpacing(12);

        mediaInfoCard = new MediaInfoCard(rightContainer);
        presetPanel = new PresetPanel(rightContainer);

        applyToAllBtn = new QPushButton("🔄 将此配置应用到列表中所有任务", rightContainer);
        applyToAllBtn->setObjectName("applyAllBtn");

        rcLayout->addWidget(mediaInfoCard);
        rcLayout->addWidget(presetPanel);
        rcLayout->addWidget(applyToAllBtn);
        rcLayout->addStretch();

        rightScroll->setWidget(rightContainer);
        rightLayout->addWidget(rightScroll);
        splitter->addWidget(rightPanel);

        // 分割比例: 60% : 40%
        splitter->setStretchFactor(0, 3);
        splitter->setStretchFactor(1, 2);

        rootLayout->addWidget(splitter, 1);

        // 3. 状态栏
        statusLabel = new QLabel("就绪 | 欢迎使用 FFmpeg 视频格式转换器", centralWidget);
        statusLabel->setObjectName("footerStatusLabel");
        q_ptr->statusBar()->addWidget(statusLabel, 1);

        q_ptr->setCentralWidget(centralWidget);

        bindSignals();
    }

    void bindSignals() {
        // 导入文件
        auto onAddFiles = [this](const QStringList &files) {
            for (const auto &file : files) {
                if (QFileInfo::exists(file)) {
                    manager.addTask(file);
                }
            }
            q_ptr->setHasTasks(!manager.allTasks().isEmpty());
            updateStatusText();
        };

        QObject::connect(dropArea, &DropAreaWidget::filesDropped, onAddFiles);
        QObject::connect(dropArea, &DropAreaWidget::fileSelected, [this, onAddFiles]() {
            QStringList files = QFileDialog::getOpenFileNames(
                q_ptr, "选择待转换的视频文件", "",
                "视频文件 (*.mp4 *.mkv *.mov *.avi *.flv *.ts *.webm *.wmv *.m4v *.mp3 *.aac);;所有文件 (*.*)"
            );
            if (!files.isEmpty()) {
                onAddFiles(files);
            }
        });

        QObject::connect(addFileBtn, &QPushButton::clicked, [this, onAddFiles]() {
            QStringList files = QFileDialog::getOpenFileNames(
                q_ptr, "选择待转换的视频文件", "",
                "视频文件 (*.mp4 *.mkv *.mov *.avi *.flv *.ts *.webm *.wmv *.m4v *.mp3 *.aac);;所有文件 (*.*)"
            );
            if (!files.isEmpty()) {
                onAddFiles(files);
            }
        });

        // 任务批量控制
        QObject::connect(startAllBtn, &QPushButton::clicked, [this]() {
            manager.startAll();
            updateStatusText();
        });

        QObject::connect(pauseAllBtn, &QPushButton::clicked, [this]() {
            manager.pauseAll();
            updateStatusText();
        });

        QObject::connect(cancelAllBtn, &QPushButton::clicked, [this]() {
            manager.cancelAll();
            updateStatusText();
        });

        QObject::connect(clearAllBtn, &QPushButton::clicked, [this]() {
            manager.clearAllTasks();
            q_ptr->setHasTasks(false);
            mediaInfoCard->clearMedia();
            updateStatusText();
        });

        // 选中任务联动
        QObject::connect(taskListView, &TaskListView::taskSelected, [this](const QString &id) {
            currentSelectedTaskId = id;
            auto *t = manager.getTask(id);
            if (t) {
                mediaInfoCard->setMedia(t->mediaInfo(), t->thumbnail());
                presetPanel->setConfig(t->config());
            } else {
                mediaInfoCard->clearMedia();
            }
        });

        // 预设修改更新到当前任务
        QObject::connect(presetPanel, &PresetPanel::configChanged, [this](const TranscodeConfig &cfg) {
            auto *t = manager.getTask(currentSelectedTaskId);
            if (t) {
                TranscodeConfig newCfg = cfg;
                newCfg.inputPath = t->inputFilePath();

                // 更新输出文件后缀
                QFileInfo fi(t->inputFilePath());
                QString ext = cfg.containerFormat;
                newCfg.outputPath = fi.absolutePath() + "/" + fi.completeBaseName() + "_converted." + ext;
                t->setConfig(newCfg);
            }
        });

        // 应用到全部任务
        QObject::connect(applyToAllBtn, &QPushButton::clicked, [this]() {
            auto templateCfg = presetPanel->config();
            int count = 0;
            for (auto *t : manager.allTasks()) {
                if (t->state() == TaskState::Pending) {
                    TranscodeConfig newCfg = templateCfg;
                    newCfg.inputPath = t->inputFilePath();
                    QFileInfo fi(t->inputFilePath());
                    newCfg.outputPath = fi.absolutePath() + "/" + fi.completeBaseName() + "_converted." + templateCfg.containerFormat;
                    t->setConfig(newCfg);
                    count++;
                }
            }
            statusLabel->setText(QString("已将当前配置批量应用到 %1 个待处理任务").arg(count));
        });

        // 状态变更与全部完成监听
        QObject::connect(&manager, &TranscodeTaskManager::taskStateChanged, [this](TranscodeTask*, TaskState) {
            updateStatusText();
        });

        QObject::connect(&manager, &TranscodeTaskManager::allTasksCompleted, [this]() {
            statusLabel->setText("🎉 恭喜！列表中所有转码任务均已完成！");
        });
    }

    void updateStatusText() {
        auto tasks = manager.allTasks();
        int pending = 0, running = 0, completed = 0, failed = 0;
        for (auto *t : tasks) {
            switch (t->state()) {
            case TaskState::Pending:
            case TaskState::Analyzing: pending++; break;
            case TaskState::Converting: running++; break;
            case TaskState::Completed: completed++; break;
            case TaskState::Failed: failed++; break;
            default: break;
            }
        }
        statusLabel->setText(QString("任务总数: %1 | 转换中: %2 | 等待中: %3 | 已完成: %4 | 失败: %5")
            .arg(tasks.size()).arg(running).arg(pending).arg(completed).arg(failed));
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
    for (const auto &url : urls) {
        if (url.isLocalFile()) {
            d_ptr->manager.addTask(url.toLocalFile());
        }
    }
    setHasTasks(!d_ptr->manager.allTasks().isEmpty());
    event->acceptProposedAction();
}

} // namespace ffmpeg_transform
