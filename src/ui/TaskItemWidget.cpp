#include "TaskItemWidget.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QStyle>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QProcess>

namespace ffmpeg_transform {

class TaskItemWidgetPrivate {
public:
    TaskItemWidget *q_ptr{nullptr};
    TranscodeTask *task{nullptr};
    TaskState state{TaskState::Pending};

    QLabel *thumbLabel{nullptr};
    QLabel *titleLabel{nullptr};
    QLabel *statusBadge{nullptr};
    QLabel *detailLabel{nullptr};
    QProgressBar *progressBar{nullptr};
    QLabel *statsLabel{nullptr};

    QPushButton *pauseResumeBtn{nullptr};
    QPushButton *cancelBtn{nullptr};
    QPushButton *openFolderBtn{nullptr};
    QPushButton *removeBtn{nullptr};

    void initUI() {
        q_ptr->setObjectName("taskItemWidget");
        q_ptr->setCursor(Qt::PointingHandCursor);

        auto *mainLayout = new QHBoxLayout(q_ptr);
        mainLayout->setContentsMargins(10, 8, 10, 8);
        mainLayout->setSpacing(12);

        // 缩略图
        thumbLabel = new QLabel(q_ptr);
        thumbLabel->setObjectName("itemThumbLabel");
        thumbLabel->setFixedSize(80, 50);
        thumbLabel->setAlignment(Qt::AlignCenter);
        thumbLabel->setText("");

        // 中部详情
        auto *centerLayout = new QVBoxLayout();
        centerLayout->setSpacing(4);

        auto *topRow = new QHBoxLayout();
        titleLabel = new QLabel(QFileInfo(task->inputFilePath()).fileName(), q_ptr);
        titleLabel->setObjectName("itemTitleLabel");

        statusBadge = new QLabel("等待中", q_ptr);
        statusBadge->setObjectName("statusBadge");

        topRow->addWidget(titleLabel);
        topRow->addWidget(statusBadge);
        topRow->addStretch();

        detailLabel = new QLabel(task->config().outputPath, q_ptr);
        detailLabel->setObjectName("itemDetailLabel");

        progressBar = new QProgressBar(q_ptr);
        progressBar->setObjectName("itemProgressBar");
        progressBar->setRange(0, 100);
        progressBar->setValue(0);
        progressBar->setFixedHeight(6);
        progressBar->setTextVisible(false);

        statsLabel = new QLabel("0%  |  --:--  |  0.0x", q_ptr);
        statsLabel->setObjectName("itemMetricsLabel");

        auto *bottomRow = new QHBoxLayout();
        bottomRow->addWidget(progressBar, 1);
        bottomRow->addWidget(statsLabel);

        centerLayout->addLayout(topRow);
        centerLayout->addWidget(detailLabel);
        centerLayout->addLayout(bottomRow);

        // 右侧操作按钮
        auto *actionLayout = new QHBoxLayout();
        actionLayout->setSpacing(6);

        pauseResumeBtn = new QPushButton("暂停", q_ptr);
        pauseResumeBtn->setObjectName("itemActionBtn");
        pauseResumeBtn->setToolTip("暂停/继续");

        cancelBtn = new QPushButton("终止", q_ptr);
        cancelBtn->setObjectName("itemActionBtn");
        cancelBtn->setToolTip("终止任务");

        openFolderBtn = new QPushButton("打开", q_ptr);
        openFolderBtn->setObjectName("itemActionBtn");
        openFolderBtn->setToolTip("打开所在文件夹");
        openFolderBtn->setVisible(false);

        removeBtn = new QPushButton("删除", q_ptr);
        removeBtn->setObjectName("itemActionBtn");
        removeBtn->setToolTip("移除任务");

        actionLayout->addWidget(pauseResumeBtn);
        actionLayout->addWidget(cancelBtn);
        actionLayout->addWidget(openFolderBtn);
        actionLayout->addWidget(removeBtn);

        mainLayout->addWidget(thumbLabel);
        mainLayout->addLayout(centerLayout, 1);
        mainLayout->addLayout(actionLayout);

        // 绑定 Task 信号
        QObject::connect(task, &TranscodeTask::thumbnailLoaded, [this](const QImage &img) {
            if (!img.isNull()) {
                QPixmap pix = QPixmap::fromImage(img);
                thumbLabel->setPixmap(pix.scaled(thumbLabel->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
            }
        });

        QObject::connect(task, &TranscodeTask::stateChanged, [this](TaskState s) {
            q_ptr->setState(s);
        });

        QObject::connect(task, &TranscodeTask::progressChanged, [this](const TranscodeProgress &p) {
            updateProgress(p);
        });

        // 绑定按钮点击
        QObject::connect(pauseResumeBtn, &QPushButton::clicked, [this]() {
            if (state == TaskState::Converting) {
                emit q_ptr->pauseClicked(task->id());
            } else if (state == TaskState::Paused) {
                emit q_ptr->resumeClicked(task->id());
            }
        });

        QObject::connect(cancelBtn, &QPushButton::clicked, [this]() {
            emit q_ptr->cancelClicked(task->id());
        });

        QObject::connect(removeBtn, &QPushButton::clicked, [this]() {
            emit q_ptr->removeClicked(task->id());
        });

        QObject::connect(openFolderBtn, &QPushButton::clicked, [this]() {
            QString outPath = task->config().outputPath;
            #ifdef Q_OS_WIN
            QStringList args;
            args << "/select," << QDir::toNativeSeparators(outPath);
            QProcess::startDetached("explorer.exe", args);
            #else
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(outPath).absolutePath()));
            #endif
        });

        if (!task->thumbnail().isNull()) {
            QPixmap pix = QPixmap::fromImage(task->thumbnail());
            thumbLabel->setPixmap(pix.scaled(thumbLabel->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
        }

        q_ptr->setState(task->state());
    }

    void updateProgress(const TranscodeProgress &p) {
        progressBar->setValue(static_cast<int>(p.percent));
        QString statsText = QString("%1%  |  速率: %2  |  ETA: %3  |  %4 FPS")
            .arg(static_cast<int>(p.percent))
            .arg(p.formattedSpeed())
            .arg(p.formattedEta())
            .arg(static_cast<int>(p.currentFps));
        statsLabel->setText(statsText);
    }

    void updateUIForState(TaskState s) {
        state = s;
        switch (s) {
        case TaskState::Pending:
            statusBadge->setText("等待压制");
            statusBadge->setProperty("status", "pending");
            pauseResumeBtn->setEnabled(false);
            cancelBtn->setEnabled(false);
            openFolderBtn->setVisible(false);
            break;
        case TaskState::Analyzing:
            statusBadge->setText("分析中...");
            statusBadge->setProperty("status", "analyzing");
            pauseResumeBtn->setEnabled(false);
            cancelBtn->setEnabled(false);
            openFolderBtn->setVisible(false);
            break;
        case TaskState::Converting:
            statusBadge->setText("压制中");
            statusBadge->setProperty("status", "converting");
            pauseResumeBtn->setEnabled(true);
            pauseResumeBtn->setText("暂停");
            cancelBtn->setEnabled(true);
            openFolderBtn->setVisible(false);
            break;
        case TaskState::Paused:
            statusBadge->setText("已暂停");
            statusBadge->setProperty("status", "paused");
            pauseResumeBtn->setEnabled(true);
            pauseResumeBtn->setText("继续");
            cancelBtn->setEnabled(true);
            openFolderBtn->setVisible(false);
            break;
        case TaskState::Completed:
            statusBadge->setText("压制完成");
            statusBadge->setProperty("status", "completed");
            progressBar->setValue(100);
            statsLabel->setText("100%  |  压制成功");
            pauseResumeBtn->setVisible(false);
            cancelBtn->setVisible(false);
            openFolderBtn->setVisible(true);
            break;
        case TaskState::Failed:
            statusBadge->setText("失败");
            statusBadge->setProperty("status", "failed");
            statsLabel->setText("压制失败: " + task->errorMessage());
            pauseResumeBtn->setVisible(false);
            cancelBtn->setVisible(false);
            openFolderBtn->setVisible(false);
            break;
        case TaskState::Canceled:
            statusBadge->setText("已终止");
            statusBadge->setProperty("status", "canceled");
            statsLabel->setText("已终止");
            pauseResumeBtn->setVisible(false);
            cancelBtn->setVisible(false);
            openFolderBtn->setVisible(false);
            break;
        }

        statusBadge->style()->unpolish(statusBadge);
        statusBadge->style()->polish(statusBadge);
        statusBadge->update();
    }
};

TaskItemWidget::TaskItemWidget(TranscodeTask *task, QWidget *parent)
    : QWidget(parent), d_ptr(std::make_unique<TaskItemWidgetPrivate>()) {
    Q_D(TaskItemWidget);
    d->q_ptr = this;
    d->task = task;
    d->initUI();
}

TaskItemWidget::~TaskItemWidget() = default;

TranscodeTask* TaskItemWidget::task() const {
    return d_ptr->task;
}

TaskState TaskItemWidget::state() const {
    return d_ptr->state;
}

void TaskItemWidget::setState(TaskState state) {
    Q_D(TaskItemWidget);
    if (d->state == state) return;
    d->updateUIForState(state);

    style()->unpolish(this);
    style()->polish(this);
    update();

    emit stateChanged(state);
}

void TaskItemWidget::mousePressEvent(QMouseEvent *event) {
    emit itemSelected(d_ptr->task->id());
    QWidget::mousePressEvent(event);
}

} // namespace ffmpeg_transform
