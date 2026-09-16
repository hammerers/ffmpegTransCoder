#include "LiveMonitorPage.h"
#include "VideoCompareWidget.h"
#include "manager/TranscodeTaskManager.h"
#include "manager/TranscodeTask.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QFrame>
#include <QFileInfo>
#include <algorithm>

namespace ffmpeg_transform {

class LiveMonitorPagePrivate {
public:
    LiveMonitorPage *q_ptr{nullptr};
    TranscodeTaskManager *manager{nullptr};
    QString currentTaskId;

    QLabel *titleLabel{nullptr};
    QLabel *taskNameLabel{nullptr};
    QLabel *hwBadgeLabel{nullptr};

    QButtonGroup *modeGroup{nullptr};
    QPushButton *sideBySideBtn{nullptr};
    QPushButton *curtainBtn{nullptr};
    QPushButton *processedBtn{nullptr};

    QPushButton *startBtn{nullptr};
    QPushButton *pauseBtn{nullptr};

    VideoCompareWidget *viewport{nullptr};

    QComboBox *taskCombo{nullptr};
    QCheckBox *delogoCheck{nullptr};
    QSpinBox *spinX{nullptr};
    QSpinBox *spinY{nullptr};
    QSpinBox *spinW{nullptr};
    QSpinBox *spinH{nullptr};

    void initUI() {
        q_ptr->setObjectName("liveMonitorPage");

        auto *mainLayout = new QVBoxLayout(q_ptr);
        mainLayout->setContentsMargins(20, 20, 20, 20);
        mainLayout->setSpacing(14);

        // 1. 顶部控制栏
        auto *headerLayout = new QHBoxLayout();
        headerLayout->setSpacing(10);

        auto *titleBox = new QVBoxLayout();
        titleBox->setSpacing(2);
        titleLabel = new QLabel("实时双分屏画面对比检视", q_ptr);
        titleLabel->setObjectName("liveMonitorTitle");

        taskNameLabel = new QLabel("当前状态: 暂无活跃转码流", q_ptr);
        taskNameLabel->setObjectName("liveMonitorSubtitle");
        titleBox->addWidget(titleLabel);
        titleBox->addWidget(taskNameLabel);
        headerLayout->addLayout(titleBox);

        headerLayout->addStretch();

        hwBadgeLabel = new QLabel("硬件加速: 自动探测就绪", q_ptr);
        hwBadgeLabel->setObjectName("liveHwBadge");
        headerLayout->addWidget(hwBadgeLabel);

        // 视口分屏模式按钮组
        modeGroup = new QButtonGroup(q_ptr);
        modeGroup->setExclusive(true);

        sideBySideBtn = new QPushButton("左右并排", q_ptr);
        sideBySideBtn->setObjectName("btnCompareModeSide");
        sideBySideBtn->setCheckable(true);
        sideBySideBtn->setChecked(true);
        modeGroup->addButton(sideBySideBtn, 0);

        curtainBtn = new QPushButton("卷帘分屏", q_ptr);
        curtainBtn->setObjectName("btnCompareModeCurtain");
        curtainBtn->setCheckable(true);
        modeGroup->addButton(curtainBtn, 1);

        processedBtn = new QPushButton("成品画面", q_ptr);
        processedBtn->setObjectName("btnCompareModeProcessed");
        processedBtn->setCheckable(true);
        modeGroup->addButton(processedBtn, 2);

        headerLayout->addWidget(sideBySideBtn);
        headerLayout->addWidget(curtainBtn);
        headerLayout->addWidget(processedBtn);

        // 转码控制按钮组
        startBtn = new QPushButton("开始转码检视", q_ptr);
        startBtn->setObjectName("btnLiveStart");
        headerLayout->addWidget(startBtn);

        pauseBtn = new QPushButton("暂停", q_ptr);
        pauseBtn->setObjectName("btnLivePause");
        pauseBtn->setEnabled(false);
        headerLayout->addWidget(pauseBtn);

        mainLayout->addLayout(headerLayout);

        // 2. 中央双分屏视口
        viewport = new VideoCompareWidget(q_ptr);
        mainLayout->addWidget(viewport, 1);

        // 3. 底部算法与滤镜参数控制条
        auto *bottomCard = new QFrame(q_ptr);
        bottomCard->setObjectName("liveFilterCard");
        auto *bottomLayout = new QHBoxLayout(bottomCard);
        bottomLayout->setContentsMargins(14, 10, 14, 10);
        bottomLayout->setSpacing(10);

        auto *taskLbl = new QLabel("检视视频:", bottomCard);
        taskLbl->setObjectName("liveSpinLabel");
        bottomLayout->addWidget(taskLbl);

        taskCombo = new QComboBox(bottomCard);
        taskCombo->setObjectName("liveTaskCombo");
        bottomLayout->addWidget(taskCombo);

        delogoCheck = new QCheckBox("启用内存级去水印与平滑", bottomCard);
        delogoCheck->setObjectName("checkLiveDelogo");
        bottomLayout->addWidget(delogoCheck);

        auto addSpin = [bottomCard, bottomLayout](const QString &label, int minV, int maxV, int initV) -> QSpinBox* {
            auto *lbl = new QLabel(label, bottomCard);
            lbl->setObjectName("liveSpinLabel");
            auto *spin = new QSpinBox(bottomCard);
            spin->setObjectName("liveSpinBox");
            spin->setRange(minV, maxV);
            spin->setValue(initV);
            bottomLayout->addWidget(lbl);
            bottomLayout->addWidget(spin);
            return spin;
        };

        spinX = addSpin("X:", 0, 3840, 30);
        spinY = addSpin("Y:", 0, 2160, 30);
        spinW = addSpin("宽:", 2, 1920, 160);
        spinH = addSpin("高:", 2, 1080, 60);

        bottomLayout->addStretch();

        auto *hintLabel = new QLabel("提示: 调参后画面红框与平滑效果即时可见 | 拖拽卷帘竖线同屏对比", bottomCard);
        hintLabel->setObjectName("liveHintLabel");
        bottomLayout->addWidget(hintLabel);

        mainLayout->addWidget(bottomCard);

        bindEvents();
    }

    void bindEvents() {
        // 模式切换联动
        QObject::connect(modeGroup, &QButtonGroup::idClicked, [this](int id) {
            if (id == 0) viewport->setCompareMode(CompareMode::SideBySide);
            else if (id == 1) viewport->setCompareMode(CompareMode::CurtainSplit);
            else if (id == 2) viewport->setCompareMode(CompareMode::ProcessedOnly);
        });

        // 开始转码检视
        QObject::connect(startBtn, &QPushButton::clicked, [this]() {
            if (manager) {
                manager->startAll();
                updateRunningState();
            }
        });

        // 暂停 / 恢复转码
        QObject::connect(pauseBtn, &QPushButton::clicked, [this]() {
            if (!manager) return;
            bool anyRunning = false;
            for (auto *t : manager->allTasks()) {
                if (t->state() == TaskState::Converting) { anyRunning = true; break; }
            }
            if (anyRunning) {
                manager->pauseAll();
            } else {
                manager->resumeAll();
            }
            updateRunningState();
        });

        // 切换检视任务
        QObject::connect(taskCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int idx) {
            if (idx < 0) return;
            currentTaskId = taskCombo->currentData().toString();
            if (manager) {
                auto *task = manager->getTask(currentTaskId);
                if (task) {
                    TranscodeConfig c = task->config();
                    delogoCheck->setChecked(c.delogo.enabled);
                    spinX->setValue(c.delogo.x);
                    spinY->setValue(c.delogo.y);
                    spinW->setValue(c.delogo.width);
                    spinH->setValue(c.delogo.height);

                    QImage thumb = task->thumbnail();
                    if (!thumb.isNull()) {
                        viewport->setStaticPreview(thumb, QFileInfo(task->inputFilePath()).fileName());
                        viewport->setDelogoHighlight(c.delogo.enabled, QRect(c.delogo.x, c.delogo.y, c.delogo.width, c.delogo.height));
                    }
                }
            }
        });

        // 滤镜参数联动
        auto emitConfig = [this]() {
            DelogoConfig cfg;
            cfg.enabled = delogoCheck->isChecked();
            cfg.x = spinX->value();
            cfg.y = spinY->value();
            cfg.width = spinW->value();
            cfg.height = spinH->value();
            viewport->setDelogoHighlight(cfg.enabled, QRect(cfg.x, cfg.y, cfg.width, cfg.height));

            if (manager) {
                auto *task = manager->getTask(currentTaskId);
                if (task) {
                    TranscodeConfig c = task->config();
                    c.delogo = cfg;
                    task->setConfig(c);
                }
            }
            emit q_ptr->delogoConfigChanged(cfg);
        };

        QObject::connect(delogoCheck, &QCheckBox::toggled, emitConfig);
        QObject::connect(spinX, QOverload<int>::of(&QSpinBox::valueChanged), emitConfig);
        QObject::connect(spinY, QOverload<int>::of(&QSpinBox::valueChanged), emitConfig);
        QObject::connect(spinW, QOverload<int>::of(&QSpinBox::valueChanged), emitConfig);
        QObject::connect(spinH, QOverload<int>::of(&QSpinBox::valueChanged), emitConfig);
    }

    void updateRunningState() {
        if (!manager) return;
        bool anyRunning = false;
        bool anyPaused = false;
        bool anyPending = false;
        for (auto *t : manager->allTasks()) {
            if (t->state() == TaskState::Converting) anyRunning = true;
            else if (t->state() == TaskState::Paused) anyPaused = true;
            else if (t->state() == TaskState::Pending) anyPending = true;
        }

        if (anyRunning) {
            startBtn->setText("正在转码中...");
            startBtn->setEnabled(false);
            pauseBtn->setText("暂停");
            pauseBtn->setEnabled(true);
        } else if (anyPaused) {
            startBtn->setText("恢复转码");
            startBtn->setEnabled(true);
            pauseBtn->setText("恢复");
            pauseBtn->setEnabled(true);
        } else if (anyPending) {
            startBtn->setText("开始转码检视");
            startBtn->setEnabled(true);
            pauseBtn->setEnabled(false);
        } else {
            startBtn->setText(manager->allTasks().isEmpty() ? "队列无任务" : "重新转码");
            startBtn->setEnabled(!manager->allTasks().isEmpty());
            pauseBtn->setEnabled(false);
        }
    }
};

LiveMonitorPage::LiveMonitorPage(QWidget *parent)
    : QWidget(parent), d_ptr(std::make_unique<LiveMonitorPagePrivate>()) {
    Q_D(LiveMonitorPage);
    d->q_ptr = this;
    d->initUI();
}

LiveMonitorPage::~LiveMonitorPage() = default;

void LiveMonitorPage::setManager(TranscodeTaskManager *manager) {
    Q_D(LiveMonitorPage);
    d->manager = manager;
    if (!manager) return;

    auto refreshTaskCombo = [this, d]() {
        d->taskCombo->blockSignals(true);
        d->taskCombo->clear();
        for (auto *t : d->manager->allTasks()) {
            QString name = QFileInfo(t->inputFilePath()).fileName();
            QString stateStr;
            if (t->state() == TaskState::Converting) stateStr = "转码中";
            else if (t->state() == TaskState::Completed) stateStr = "已完成";
            else if (t->state() == TaskState::Paused) stateStr = "已暂停";
            else stateStr = "待命中";
            d->taskCombo->addItem(QString("%1 (%2)").arg(name).arg(stateStr), t->id());
        }
        d->taskCombo->blockSignals(false);
        checkAndLoadPreview();
    };

    QObject::connect(manager, &TranscodeTaskManager::taskAdded, this, refreshTaskCombo);
    QObject::connect(manager, &TranscodeTaskManager::taskRemoved, this, refreshTaskCombo);
    QObject::connect(manager, &TranscodeTaskManager::taskStateChanged, this, [d](TranscodeTask *, TaskState) {
        d->updateRunningState();
    });

    refreshTaskCombo();
}

void LiveMonitorPage::checkAndLoadPreview() {
    Q_D(LiveMonitorPage);
    if (!d->manager) return;

    for (auto *t : d->manager->allTasks()) {
        if (t->state() == TaskState::Converting) {
            d->updateRunningState();
            return;
        }
    }

    QString targetId = d->taskCombo->currentData().toString();
    TranscodeTask *task = d->manager->getTask(targetId);
    if (!task && !d->manager->allTasks().isEmpty()) {
        task = d->manager->allTasks().first();
    }

    if (task) {
        d->currentTaskId = task->id();
        QString fileName = QFileInfo(task->inputFilePath()).fileName();
        d->taskNameLabel->setText(QString("当前状态: 待命调参 (已加载: %1 - 可在下方调参预览去水印选区)").arg(fileName));

        QImage thumb = task->thumbnail();
        if (!thumb.isNull()) {
            d->viewport->setStaticPreview(thumb, fileName);
            d->viewport->setDelogoHighlight(d->delogoCheck->isChecked(), QRect(d->spinX->value(), d->spinY->value(), d->spinW->value(), d->spinH->value()));
        } else {
            QObject::connect(task, &TranscodeTask::thumbnailLoaded, this, [d, task](const QImage &img) {
                if (d->currentTaskId == task->id() && !d->viewport->hasFrames()) {
                    d->viewport->setStaticPreview(img, QFileInfo(task->inputFilePath()).fileName());
                    d->viewport->setDelogoHighlight(d->delogoCheck->isChecked(), QRect(d->spinX->value(), d->spinY->value(), d->spinW->value(), d->spinH->value()));
                }
            });
        }
    } else {
        d->taskNameLabel->setText("当前状态: 暂无活跃转码流");
        d->viewport->resetToIdle();
    }
    d->updateRunningState();
}

DelogoConfig LiveMonitorPage::delogoConfig() const {
    Q_D(const LiveMonitorPage);
    DelogoConfig cfg;
    cfg.enabled = d->delogoCheck->isChecked();
    cfg.x = d->spinX->value();
    cfg.y = d->spinY->value();
    cfg.width = d->spinW->value();
    cfg.height = d->spinH->value();
    return cfg;
}

void LiveMonitorPage::updateLiveFrame(const QString &taskName, const QImage &origin, const QImage &processed, double ptsSec) {
    Q_D(LiveMonitorPage);
    d->taskNameLabel->setText(QString("正在实时压制渲染: %1").arg(taskName));
    d->viewport->updateFrames(origin, processed, ptsSec);
    d->updateRunningState();
}

void LiveMonitorPage::setHwAccelStatus(const QString &statusText) {
    Q_D(LiveMonitorPage);
    d->hwBadgeLabel->setText(statusText);
}

void LiveMonitorPage::resetToIdle() {
    Q_D(LiveMonitorPage);
    d->taskNameLabel->setText("当前状态: 暂无活跃转码流");
    d->viewport->resetToIdle();
    d->updateRunningState();
}

void LiveMonitorPage::showCompletedState() {
    Q_D(LiveMonitorPage);
    d->taskNameLabel->setText("当前状态: 转码已完成 (保留最终成品画质对比，可拖拽卷帘检视)");
    d->viewport->setCompleted(true, "转码已完成");
    d->updateRunningState();
}

void LiveMonitorPage::setDelogoConfig(const DelogoConfig &cfg) {
    Q_D(LiveMonitorPage);
    d->delogoCheck->setChecked(cfg.enabled);
    d->spinX->setValue(cfg.x);
    d->spinY->setValue(cfg.y);
    d->spinW->setValue(cfg.width);
    d->spinH->setValue(cfg.height);
    d->viewport->setDelogoHighlight(cfg.enabled, QRect(cfg.x, cfg.y, cfg.width, cfg.height));
}

} // namespace ffmpeg_transform
