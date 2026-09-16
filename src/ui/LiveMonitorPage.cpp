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
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QComboBox>
#include <QStackedWidget>
#include <QScrollArea>
#include <QFileDialog>
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

    QPushButton *startBtn{nullptr};
    QPushButton *pauseBtn{nullptr};

    VideoCompareWidget *viewport{nullptr};

    // 底部工具与检视任务选择
    QComboBox *taskCombo{nullptr};
    QPushButton *tabWatermarkBtn{nullptr};
    QPushButton *tabDelogoBtn{nullptr};
    QStackedWidget *toolStack{nullptr};

    // 自定义水印控件
    QCheckBox *watermarkCheck{nullptr};
    QComboBox *wmTypeCombo{nullptr};
    QWidget *wmTextWidget{nullptr};
    QLineEdit *wmTextEdit{nullptr};
    QSpinBox *wmFontSizeSpin{nullptr};
    QWidget *wmImageWidget{nullptr};
    QLineEdit *wmImagePathEdit{nullptr};
    QPushButton *wmBrowseBtn{nullptr};
    QComboBox *wmPosCombo{nullptr};
    QSpinBox *wmSpinX{nullptr};
    QSpinBox *wmSpinY{nullptr};
    QDoubleSpinBox *wmOpacitySpin{nullptr};
    QDoubleSpinBox *wmScaleSpin{nullptr};

    // 内存去水印控件
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

        // 转码控制按钮组
        startBtn = new QPushButton("开始转码检视", q_ptr);
        startBtn->setObjectName("btnLiveStart");
        headerLayout->addWidget(startBtn);

        pauseBtn = new QPushButton("暂停", q_ptr);
        pauseBtn->setObjectName("btnLivePause");
        pauseBtn->setEnabled(false);
        headerLayout->addWidget(pauseBtn);

        mainLayout->addLayout(headerLayout);

        // 2. 中央双分屏视口 (仅保留左右并排)
        viewport = new VideoCompareWidget(q_ptr);
        viewport->setCompareMode(CompareMode::SideBySide);
        mainLayout->addWidget(viewport, 1);

        // 3. 底部算法与滤镜参数控制条
        auto *bottomCard = new QFrame(q_ptr);
        bottomCard->setObjectName("liveFilterCard");
        auto *bottomLayout = new QVBoxLayout(bottomCard);
        bottomLayout->setContentsMargins(14, 10, 14, 10);
        bottomLayout->setSpacing(8);

        // 第 1 行：视频选择 + 算法选项卡切换 + 提示
        auto *selectorRow = new QHBoxLayout();
        selectorRow->setSpacing(10);

        auto *taskLbl = new QLabel("检视视频:", bottomCard);
        taskLbl->setObjectName("liveSpinLabel");
        selectorRow->addWidget(taskLbl);

        taskCombo = new QComboBox(bottomCard);
        taskCombo->setObjectName("liveTaskCombo");
        taskCombo->setMinimumWidth(160);
        taskCombo->setMaximumWidth(240);
        selectorRow->addWidget(taskCombo);

        auto *tabGroup = new QButtonGroup(bottomCard);
        tabGroup->setExclusive(true);

        tabWatermarkBtn = new QPushButton("自定义水印叠加", bottomCard);
        tabWatermarkBtn->setObjectName("liveTabWatermark");
        tabWatermarkBtn->setCheckable(true);
        tabWatermarkBtn->setChecked(true);
        tabGroup->addButton(tabWatermarkBtn, 0);

        tabDelogoBtn = new QPushButton("内存去水印算法", bottomCard);
        tabDelogoBtn->setObjectName("liveTabDelogo");
        tabDelogoBtn->setCheckable(true);
        tabGroup->addButton(tabDelogoBtn, 1);

        selectorRow->addWidget(tabWatermarkBtn);
        selectorRow->addWidget(tabDelogoBtn);
        selectorRow->addStretch();

        auto *hintLabel = new QLabel("提示: 调参即时所见 | 右侧画面支持鼠标直接拖拽水印", bottomCard);
        hintLabel->setObjectName("liveHintLabel");
        selectorRow->addWidget(hintLabel);

        bottomLayout->addLayout(selectorRow);

        // 第 2 行：子面板切换栈
        toolStack = new QStackedWidget(bottomCard);
        toolStack->setObjectName("liveToolStack");

        // --- 子面板 0: 自定义水印面板 ---
        auto *wmPage = new QWidget(toolStack);
        wmPage->setObjectName("liveWmPage");
        wmPage->setMinimumWidth(920);
        auto *wmLayout = new QHBoxLayout(wmPage);
        wmLayout->setContentsMargins(0, 0, 0, 0);
        wmLayout->setSpacing(6);

        watermarkCheck = new QCheckBox("启用自定义水印", wmPage);
        watermarkCheck->setObjectName("checkLiveWatermark");
        wmLayout->addWidget(watermarkCheck);

        auto *typeLbl = new QLabel("类型:", wmPage);
        typeLbl->setObjectName("liveSpinLabel");
        wmLayout->addWidget(typeLbl);

        wmTypeCombo = new QComboBox(wmPage);
        wmTypeCombo->setObjectName("liveTypeCombo");
        wmTypeCombo->addItem("文字水印", static_cast<int>(WatermarkType::Text));
        wmTypeCombo->addItem("图片水印", static_cast<int>(WatermarkType::Image));
        wmTypeCombo->setMaximumWidth(95);
        wmLayout->addWidget(wmTypeCombo);

        // 文字水印专属组件
        wmTextWidget = new QWidget(wmPage);
        auto *textLayout = new QHBoxLayout(wmTextWidget);
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(6);
        auto *textLbl = new QLabel("文本:", wmTextWidget);
        textLbl->setObjectName("liveSpinLabel");
        textLayout->addWidget(textLbl);
        wmTextEdit = new QLineEdit("HAMMERERS STUDIO", wmTextWidget);
        wmTextEdit->setObjectName("liveTextEdit");
        wmTextEdit->setPlaceholderText("水印文字");
        wmTextEdit->setMinimumWidth(110);
        wmTextEdit->setMaximumWidth(160);
        textLayout->addWidget(wmTextEdit);

        auto *sizeLbl = new QLabel("字号:", wmTextWidget);
        sizeLbl->setObjectName("liveSpinLabel");
        textLayout->addWidget(sizeLbl);
        wmFontSizeSpin = new QSpinBox(wmTextWidget);
        wmFontSizeSpin->setObjectName("liveSpinBox");
        wmFontSizeSpin->setRange(10, 120);
        wmFontSizeSpin->setValue(28);
        wmFontSizeSpin->setSuffix(" px");
        wmFontSizeSpin->setMaximumWidth(70);
        textLayout->addWidget(wmFontSizeSpin);
        wmLayout->addWidget(wmTextWidget);

        // 图片水印专属组件
        wmImageWidget = new QWidget(wmPage);
        auto *imgLayout = new QHBoxLayout(wmImageWidget);
        imgLayout->setContentsMargins(0, 0, 0, 0);
        imgLayout->setSpacing(6);
        auto *pathLbl = new QLabel("图片:", wmImageWidget);
        pathLbl->setObjectName("liveSpinLabel");
        imgLayout->addWidget(pathLbl);
        wmImagePathEdit = new QLineEdit(wmImageWidget);
        wmImagePathEdit->setObjectName("liveImagePathEdit");
        wmImagePathEdit->setPlaceholderText("选择 PNG/JPG 水印文件...");
        wmImagePathEdit->setMinimumWidth(120);
        wmImagePathEdit->setMaximumWidth(180);
        imgLayout->addWidget(wmImagePathEdit);
        wmBrowseBtn = new QPushButton("浏览...", wmImageWidget);
        wmBrowseBtn->setObjectName("btnBrowseWatermark");
        wmBrowseBtn->setMaximumWidth(65);
        imgLayout->addWidget(wmBrowseBtn);
        wmLayout->addWidget(wmImageWidget);
        wmImageWidget->setVisible(false); // 初始文字水印

        // 方位选择
        auto *posLbl = new QLabel("方位:", wmPage);
        posLbl->setObjectName("liveSpinLabel");
        wmLayout->addWidget(posLbl);
        wmPosCombo = new QComboBox(wmPage);
        wmPosCombo->setObjectName("livePosCombo");
        wmPosCombo->addItem("右上角", static_cast<int>(WatermarkPosition::TopRight));
        wmPosCombo->addItem("左上角", static_cast<int>(WatermarkPosition::TopLeft));
        wmPosCombo->addItem("右下角", static_cast<int>(WatermarkPosition::BottomRight));
        wmPosCombo->addItem("左下角", static_cast<int>(WatermarkPosition::BottomLeft));
        wmPosCombo->addItem("居中", static_cast<int>(WatermarkPosition::Center));
        wmPosCombo->addItem("自定义坐标", static_cast<int>(WatermarkPosition::Custom));
        wmPosCombo->setMaximumWidth(100);
        wmLayout->addWidget(wmPosCombo);

        auto *xLbl = new QLabel("X:", wmPage);
        xLbl->setObjectName("liveSpinLabel");
        wmLayout->addWidget(xLbl);
        wmSpinX = new QSpinBox(wmPage);
        wmSpinX->setObjectName("liveSpinBox");
        wmSpinX->setRange(0, 3840);
        wmSpinX->setValue(30);
        wmSpinX->setMaximumWidth(70);
        wmLayout->addWidget(wmSpinX);

        auto *yLbl = new QLabel("Y:", wmPage);
        yLbl->setObjectName("liveSpinLabel");
        wmLayout->addWidget(yLbl);
        wmSpinY = new QSpinBox(wmPage);
        wmSpinY->setObjectName("liveSpinBox");
        wmSpinY->setRange(0, 2160);
        wmSpinY->setValue(30);
        wmSpinY->setMaximumWidth(70);
        wmLayout->addWidget(wmSpinY);

        auto *opLbl = new QLabel("透明度:", wmPage);
        opLbl->setObjectName("liveSpinLabel");
        wmLayout->addWidget(opLbl);
        wmOpacitySpin = new QDoubleSpinBox(wmPage);
        wmOpacitySpin->setObjectName("liveDoubleSpinBox");
        wmOpacitySpin->setRange(0.05, 1.00);
        wmOpacitySpin->setSingleStep(0.05);
        wmOpacitySpin->setValue(0.85);
        wmOpacitySpin->setMaximumWidth(70);
        wmLayout->addWidget(wmOpacitySpin);

        auto *scLbl = new QLabel("缩放:", wmPage);
        scLbl->setObjectName("liveSpinLabel");
        wmLayout->addWidget(scLbl);
        wmScaleSpin = new QDoubleSpinBox(wmPage);
        wmScaleSpin->setObjectName("liveDoubleSpinBox");
        wmScaleSpin->setRange(0.1, 3.0);
        wmScaleSpin->setSingleStep(0.1);
        wmScaleSpin->setValue(1.0);
        wmScaleSpin->setSuffix("x");
        wmScaleSpin->setMaximumWidth(70);
        wmLayout->addWidget(wmScaleSpin);

        wmLayout->addStretch();
        toolStack->addWidget(wmPage);

        // --- 子面板 1: 去水印面板 ---
        auto *delogoPage = new QWidget(toolStack);
        delogoPage->setObjectName("liveDelogoPage");
        delogoPage->setMinimumWidth(800);
        auto *delogoLayout = new QHBoxLayout(delogoPage);
        delogoLayout->setContentsMargins(0, 0, 0, 0);
        delogoLayout->setSpacing(8);

        delogoCheck = new QCheckBox("启用内存级去水印与平滑", delogoPage);
        delogoCheck->setObjectName("checkLiveDelogo");
        delogoLayout->addWidget(delogoCheck);

        auto addSpin = [delogoPage, delogoLayout](const QString &label, int minV, int maxV, int initV) -> QSpinBox* {
            auto *lbl = new QLabel(label, delogoPage);
            lbl->setObjectName("liveSpinLabel");
            auto *spin = new QSpinBox(delogoPage);
            spin->setObjectName("liveSpinBox");
            spin->setRange(minV, maxV);
            spin->setValue(initV);
            spin->setMaximumWidth(70);
            delogoLayout->addWidget(lbl);
            delogoLayout->addWidget(spin);
            return spin;
        };

        spinX = addSpin("X:", 0, 3840, 30);
        spinY = addSpin("Y:", 0, 2160, 30);
        spinW = addSpin("宽:", 2, 1920, 160);
        spinH = addSpin("高:", 2, 1080, 60);

        auto *algoLbl = new QLabel("算法: 双三次加权插值平滑消除 (Zero-Artifact Memory Blur)", delogoPage);
        algoLbl->setObjectName("liveAlgoLabel");
        delogoLayout->addWidget(algoLbl);

        delogoLayout->addStretch();
        toolStack->addWidget(delogoPage);

        // 使用 QScrollArea 包裹底部工具栈，确保窗口缩小时选项绝不被截断
        auto *scrollArea = new QScrollArea(bottomCard);
        scrollArea->setObjectName("liveToolScrollArea");
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollArea->setFixedHeight(46);
        scrollArea->setWidget(toolStack);
        bottomLayout->addWidget(scrollArea);

        mainLayout->addWidget(bottomCard);

        bindEvents();
    }

    void bindEvents() {
        // 视频画面拖拽水印联动
        QObject::connect(viewport, &VideoCompareWidget::watermarkConfigChanged, [this](const WatermarkConfig &cfg) {
            wmSpinX->blockSignals(true);
            wmSpinY->blockSignals(true);
            wmPosCombo->blockSignals(true);

            wmSpinX->setValue(cfg.x);
            wmSpinY->setValue(cfg.y);
            int cIdx = wmPosCombo->findData(static_cast<int>(WatermarkPosition::Custom));
            if (cIdx >= 0) {
                wmPosCombo->setCurrentIndex(cIdx);
            }

            wmSpinX->blockSignals(false);
            wmSpinY->blockSignals(false);
            wmPosCombo->blockSignals(false);

            if (manager) {
                auto *task = manager->getTask(currentTaskId);
                if (task) {
                    TranscodeConfig c = task->config();
                    c.watermark = cfg;
                    task->setConfig(c);
                }
            }
            emit q_ptr->watermarkConfigChanged(cfg);
        });

        // 底部算法选项卡切换联动
        QObject::connect(tabWatermarkBtn, &QPushButton::clicked, [this]() {
            toolStack->setCurrentIndex(0);
        });
        QObject::connect(tabDelogoBtn, &QPushButton::clicked, [this]() {
            toolStack->setCurrentIndex(1);
        });

        // 水印类型切换 (文字 / 图片)
        QObject::connect(wmTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int idx) {
            bool isText = (idx == 0);
            wmTextWidget->setVisible(isText);
            wmImageWidget->setVisible(!isText);
        });

        // 图片选择器
        QObject::connect(wmBrowseBtn, &QPushButton::clicked, [this]() {
            QString file = QFileDialog::getOpenFileName(q_ptr, "选择水印图片", QString(), "图像文件 (*.png *.jpg *.jpeg *.bmp *.webp)");
            if (!file.isEmpty()) {
                wmImagePathEdit->setText(file);
            }
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

        // 水印参数联动
        auto emitWatermark = [this]() {
            WatermarkConfig cfg;
            cfg.enabled = watermarkCheck->isChecked();
            cfg.type = static_cast<WatermarkType>(wmTypeCombo->currentData().toInt());
            cfg.text = wmTextEdit->text().trimmed();
            cfg.imagePath = wmImagePathEdit->text().trimmed();
            cfg.fontSize = wmFontSizeSpin->value();
            cfg.fontColor = "#ffffff";
            cfg.position = static_cast<WatermarkPosition>(wmPosCombo->currentData().toInt());
            cfg.x = wmSpinX->value();
            cfg.y = wmSpinY->value();
            cfg.opacity = static_cast<float>(wmOpacitySpin->value());
            cfg.scale = static_cast<float>(wmScaleSpin->value());

            viewport->setWatermarkConfig(cfg);

            if (manager) {
                auto *task = manager->getTask(currentTaskId);
                if (task) {
                    TranscodeConfig c = task->config();
                    c.watermark = cfg;
                    task->setConfig(c);
                }
            }
            emit q_ptr->watermarkConfigChanged(cfg);
        };

        QObject::connect(watermarkCheck, &QCheckBox::toggled, emitWatermark);
        QObject::connect(wmTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), emitWatermark);
        QObject::connect(wmTextEdit, &QLineEdit::textChanged, emitWatermark);
        QObject::connect(wmImagePathEdit, &QLineEdit::textChanged, emitWatermark);
        QObject::connect(wmFontSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), emitWatermark);
        QObject::connect(wmPosCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), emitWatermark);
        QObject::connect(wmSpinX, QOverload<int>::of(&QSpinBox::valueChanged), emitWatermark);
        QObject::connect(wmSpinY, QOverload<int>::of(&QSpinBox::valueChanged), emitWatermark);
        QObject::connect(wmOpacitySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), emitWatermark);
        QObject::connect(wmScaleSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), emitWatermark);

        // 去水印参数联动
        auto emitDelogo = [this]() {
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

        QObject::connect(delogoCheck, &QCheckBox::toggled, emitDelogo);
        QObject::connect(spinX, QOverload<int>::of(&QSpinBox::valueChanged), emitDelogo);
        QObject::connect(spinY, QOverload<int>::of(&QSpinBox::valueChanged), emitDelogo);
        QObject::connect(spinW, QOverload<int>::of(&QSpinBox::valueChanged), emitDelogo);
        QObject::connect(spinH, QOverload<int>::of(&QSpinBox::valueChanged), emitDelogo);

        // 切换检视任务联动
        QObject::connect(taskCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int idx) {
            if (idx < 0) return;
            currentTaskId = taskCombo->currentData().toString();
            if (manager) {
                auto *task = manager->getTask(currentTaskId);
                if (task) {
                    TranscodeConfig c = task->config();
                    // 同步去水印
                    delogoCheck->setChecked(c.delogo.enabled);
                    spinX->setValue(c.delogo.x);
                    spinY->setValue(c.delogo.y);
                    spinW->setValue(c.delogo.width);
                    spinH->setValue(c.delogo.height);

                    // 同步水印
                    watermarkCheck->setChecked(c.watermark.enabled);
                    int tIdx = wmTypeCombo->findData(static_cast<int>(c.watermark.type));
                    if (tIdx >= 0) wmTypeCombo->setCurrentIndex(tIdx);
                    if (!c.watermark.text.isEmpty()) wmTextEdit->setText(c.watermark.text);
                    wmImagePathEdit->setText(c.watermark.imagePath);
                    if (c.watermark.fontSize > 0) wmFontSizeSpin->setValue(c.watermark.fontSize);
                    int pIdx = wmPosCombo->findData(static_cast<int>(c.watermark.position));
                    if (pIdx >= 0) wmPosCombo->setCurrentIndex(pIdx);
                    wmSpinX->setValue(c.watermark.x);
                    wmSpinY->setValue(c.watermark.y);
                    if (c.watermark.opacity > 0.0f) wmOpacitySpin->setValue(c.watermark.opacity);
                    if (c.watermark.scale > 0.0f) wmScaleSpin->setValue(c.watermark.scale);

                    viewport->setWatermarkConfig(c.watermark);
                    viewport->setDelogoHighlight(c.delogo.enabled, QRect(c.delogo.x, c.delogo.y, c.delogo.width, c.delogo.height));

                    QImage thumb = task->thumbnail();
                    if (!thumb.isNull()) {
                        viewport->setStaticPreview(thumb, QFileInfo(task->inputFilePath()).fileName());
                    }
                }
            }
        });
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
        d->taskNameLabel->setText(QString("当前状态: 待命调参 (已加载: %1 - 可在下方调参预览水印与去水印效果)").arg(fileName));

        TranscodeConfig c = task->config();
        d->viewport->setWatermarkConfig(c.watermark);
        d->viewport->setDelogoHighlight(c.delogo.enabled, QRect(c.delogo.x, c.delogo.y, c.delogo.width, c.delogo.height));

        QImage thumb = task->thumbnail();
        if (!thumb.isNull()) {
            d->viewport->setStaticPreview(thumb, fileName);
        } else {
            QObject::connect(task, &TranscodeTask::thumbnailLoaded, this, [d, task](const QImage &img) {
                if (d->currentTaskId == task->id() && !d->viewport->hasFrames()) {
                    d->viewport->setStaticPreview(img, QFileInfo(task->inputFilePath()).fileName());
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

WatermarkConfig LiveMonitorPage::watermarkConfig() const {
    Q_D(const LiveMonitorPage);
    WatermarkConfig cfg;
    cfg.enabled = d->watermarkCheck->isChecked();
    cfg.type = static_cast<WatermarkType>(d->wmTypeCombo->currentData().toInt());
    cfg.text = d->wmTextEdit->text().trimmed();
    cfg.imagePath = d->wmImagePathEdit->text().trimmed();
    cfg.fontSize = d->wmFontSizeSpin->value();
    cfg.fontColor = "#ffffff";
    cfg.position = static_cast<WatermarkPosition>(d->wmPosCombo->currentData().toInt());
    cfg.x = d->wmSpinX->value();
    cfg.y = d->wmSpinY->value();
    cfg.opacity = static_cast<float>(d->wmOpacitySpin->value());
    cfg.scale = static_cast<float>(d->wmScaleSpin->value());
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

void LiveMonitorPage::setWatermarkConfig(const WatermarkConfig &cfg) {
    Q_D(LiveMonitorPage);
    d->watermarkCheck->blockSignals(true);
    d->wmTypeCombo->blockSignals(true);
    d->wmTextEdit->blockSignals(true);
    d->wmImagePathEdit->blockSignals(true);
    d->wmFontSizeSpin->blockSignals(true);
    d->wmPosCombo->blockSignals(true);
    d->wmSpinX->blockSignals(true);
    d->wmSpinY->blockSignals(true);
    d->wmOpacitySpin->blockSignals(true);
    d->wmScaleSpin->blockSignals(true);

    d->watermarkCheck->setChecked(cfg.enabled);
    int tIdx = d->wmTypeCombo->findData(static_cast<int>(cfg.type));
    if (tIdx >= 0) {
        d->wmTypeCombo->setCurrentIndex(tIdx);
        d->wmTextWidget->setVisible(cfg.type == WatermarkType::Text);
        d->wmImageWidget->setVisible(cfg.type == WatermarkType::Image);
    }
    if (!cfg.text.isEmpty()) d->wmTextEdit->setText(cfg.text);
    d->wmImagePathEdit->setText(cfg.imagePath);
    if (cfg.fontSize > 0) d->wmFontSizeSpin->setValue(cfg.fontSize);
    int pIdx = d->wmPosCombo->findData(static_cast<int>(cfg.position));
    if (pIdx >= 0) d->wmPosCombo->setCurrentIndex(pIdx);
    d->wmSpinX->setValue(cfg.x);
    d->wmSpinY->setValue(cfg.y);
    if (cfg.opacity > 0.0f) d->wmOpacitySpin->setValue(cfg.opacity);
    if (cfg.scale > 0.0f) d->wmScaleSpin->setValue(cfg.scale);

    d->watermarkCheck->blockSignals(false);
    d->wmTypeCombo->blockSignals(false);
    d->wmTextEdit->blockSignals(false);
    d->wmImagePathEdit->blockSignals(false);
    d->wmFontSizeSpin->blockSignals(false);
    d->wmPosCombo->blockSignals(false);
    d->wmSpinX->blockSignals(false);
    d->wmSpinY->blockSignals(false);
    d->wmOpacitySpin->blockSignals(false);
    d->wmScaleSpin->blockSignals(false);

    d->viewport->setWatermarkConfig(cfg);
}

} // namespace ffmpeg_transform
