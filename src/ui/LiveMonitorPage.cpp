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
#include <QListWidget>
#include <algorithm>

namespace ffmpeg_transform {

class LiveMonitorPagePrivate {
public:
    LiveMonitorPage *q_ptr{nullptr};
    TranscodeTaskManager *manager{nullptr};
    QString currentTaskId;

    QLabel *titleLabel{nullptr};
    QLabel *taskNameLabel{nullptr};

    VideoCompareWidget *viewport{nullptr};

    // 底部工具与检视任务选择
    QComboBox *taskCombo{nullptr};
    QPushButton *tabWatermarkBtn{nullptr};
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

    // 右侧垂直队列面板
    QWidget *rightPanel{nullptr};
    QLabel *rightHeaderTitle{nullptr};
    QLabel *rightBadgeLabel{nullptr};
    QListWidget *videoListWidget{nullptr};
    QLabel *emptyListHint{nullptr};

    void connectTaskSignals(TranscodeTask *task) {
        if (!task) return;
        QObject::connect(task, &TranscodeTask::mediaInfoLoaded, q_ptr, [this, task](const MediaInfo &) {
            updateTaskItem(task);
        });
        QObject::connect(task, &TranscodeTask::thumbnailLoaded, q_ptr, [this, task](const QImage &img) {
            if (currentTaskId == task->id() && !viewport->hasFrames()) {
                viewport->setStaticPreview(img, QFileInfo(task->inputFilePath()).fileName());
            }
        });
        QObject::connect(task, &TranscodeTask::progressChanged, q_ptr, [this, task](const TranscodeProgress &) {
            updateTaskItem(task);
        });
        QObject::connect(task, &TranscodeTask::stateChanged, q_ptr, [this, task](TaskState) {
            updateTaskItem(task);
            updateRunningState();
        });
    }

    void updateTaskItem(TranscodeTask *task) {
        if (!task || !videoListWidget) return;
        for (int i = 0; i < videoListWidget->count(); ++i) {
            auto *item = videoListWidget->item(i);
            if (item && item->data(Qt::UserRole).toString() == task->id()) {
                QString name = QFileInfo(task->inputFilePath()).fileName();
                QString stateStr;
                switch (task->state()) {
                case TaskState::Pending:    stateStr = "待命调参"; break;
                case TaskState::Converting: stateStr = QString("压制中 %1%").arg(static_cast<int>(task->progress().percent)); break;
                case TaskState::Paused:     stateStr = "已暂停"; break;
                case TaskState::Completed:  stateStr = "已完成"; break;
                case TaskState::Failed:     stateStr = "失败"; break;
                case TaskState::Canceled:   stateStr = "已取消"; break;
                default:                    stateStr = "就绪"; break;
                }
                QString sub;
                if (task->mediaInfo().durationSec > 0.0) {
                    sub = QString("%1 | %2 | %3")
                        .arg(task->mediaInfo().formattedDuration())
                        .arg(task->mediaInfo().resolutionString())
                        .arg(stateStr);
                } else {
                    sub = QString("元数据解析中... | %1").arg(stateStr);
                }
                item->setText(name + "\n" + sub);
                break;
            }
        }
    }

    void refreshVideoList() {
        if (!manager || !videoListWidget) return;
        const auto &tasks = manager->allTasks();
        rightBadgeLabel->setText(QString("%1 个视频").arg(tasks.size()));

        if (tasks.isEmpty()) {
            videoListWidget->blockSignals(true);
            videoListWidget->clear();
            videoListWidget->blockSignals(false);
            videoListWidget->setVisible(false);
            emptyListHint->setVisible(true);
            return;
        }

        videoListWidget->setVisible(true);
        emptyListHint->setVisible(false);

        videoListWidget->blockSignals(true);
        videoListWidget->clear();

        QListWidgetItem *toSelect = nullptr;
        for (auto *task : tasks) {
            auto *item = new QListWidgetItem();
            QString name = QFileInfo(task->inputFilePath()).fileName();
            QString stateStr;
            switch (task->state()) {
            case TaskState::Pending:    stateStr = "待命调参"; break;
            case TaskState::Converting: stateStr = QString("压制中 %1%").arg(static_cast<int>(task->progress().percent)); break;
            case TaskState::Paused:     stateStr = "已暂停"; break;
            case TaskState::Completed:  stateStr = "已完成"; break;
            case TaskState::Failed:     stateStr = "失败"; break;
            case TaskState::Canceled:   stateStr = "已取消"; break;
            default:                    stateStr = "就绪"; break;
            }
            QString sub;
            if (task->mediaInfo().durationSec > 0.0) {
                sub = QString("%1 | %2 | %3")
                    .arg(task->mediaInfo().formattedDuration())
                    .arg(task->mediaInfo().resolutionString())
                    .arg(stateStr);
            } else {
                sub = QString("元数据解析中... | %1").arg(stateStr);
            }
            item->setText(name + "\n" + sub);
            item->setData(Qt::UserRole, task->id());
            item->setToolTip(task->inputFilePath());
            videoListWidget->addItem(item);

            if (task->id() == currentTaskId) {
                toSelect = item;
            }
        }
        videoListWidget->blockSignals(false);

        if (!toSelect && videoListWidget->count() > 0) {
            toSelect = videoListWidget->item(0);
        }
        if (toSelect && videoListWidget->currentItem() != toSelect) {
            videoListWidget->blockSignals(true);
            videoListWidget->setCurrentItem(toSelect);
            videoListWidget->blockSignals(false);
        }
    }

    void refreshTaskCombo() {
        if (!manager || !taskCombo) return;
        QString prevId = currentTaskId;
        taskCombo->blockSignals(true);
        taskCombo->clear();
        int restoreIdx = -1;
        int i = 0;
        for (auto *t : manager->allTasks()) {
            QString name = QFileInfo(t->inputFilePath()).fileName();
            QString stateStr;
            if (t->state() == TaskState::Converting) stateStr = "转码中";
            else if (t->state() == TaskState::Completed) stateStr = "已完成";
            else if (t->state() == TaskState::Paused) stateStr = "已暂停";
            else stateStr = "待命中";
            taskCombo->addItem(QString("%1 (%2)").arg(name).arg(stateStr), t->id());
            if (t->id() == prevId) {
                restoreIdx = i;
            }
            i++;
        }
        if (restoreIdx >= 0) {
            taskCombo->setCurrentIndex(restoreIdx);
        } else if (taskCombo->count() > 0) {
            taskCombo->setCurrentIndex(0);
        }
        taskCombo->blockSignals(false);
    }

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

        mainLayout->addLayout(headerLayout);

        // 2. 中央主体工作区 (水平分割：左侧视口+调参栏，右侧队列列表)
        auto *centerLayout = new QHBoxLayout();
        centerLayout->setSpacing(14);

        // 左侧工作区 (视口 + 底部调参卡片)
        auto *leftArea = new QWidget(q_ptr);
        auto *leftLayout = new QVBoxLayout(leftArea);
        leftLayout->setContentsMargins(0, 0, 0, 0);
        leftLayout->setSpacing(12);

        viewport = new VideoCompareWidget(leftArea);
        viewport->setCompareMode(CompareMode::SideBySide);
        leftLayout->addWidget(viewport, 1);

        // 3. 底部算法与滤镜参数控制条
        auto *bottomCard = new QFrame(leftArea);
        bottomCard->setObjectName("liveFilterCard");
        auto *bottomLayout = new QVBoxLayout(bottomCard);
        bottomLayout->setContentsMargins(14, 10, 14, 10);
        bottomLayout->setSpacing(8);

        // 第 1 行：视频选择 + 当前配置分区标识
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

        tabWatermarkBtn = new QPushButton("自定义水印叠加", bottomCard);
        tabWatermarkBtn->setObjectName("liveTabWatermark");
        tabWatermarkBtn->setCheckable(true);
        tabWatermarkBtn->setChecked(true);
        selectorRow->addWidget(tabWatermarkBtn);

        selectorRow->addStretch();

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

        leftLayout->addWidget(bottomCard);
        centerLayout->addWidget(leftArea, 1);

        // 4. 右侧垂直列表：队列视频清单 (宽度 260px)
        rightPanel = new QWidget(q_ptr);
        rightPanel->setObjectName("liveRightPanel");
        rightPanel->setFixedWidth(260);

        auto *rightLayout = new QVBoxLayout(rightPanel);
        rightLayout->setContentsMargins(12, 12, 12, 12);
        rightLayout->setSpacing(10);

        auto *headerRow = new QHBoxLayout();
        headerRow->setContentsMargins(0, 0, 0, 0);
        headerRow->setSpacing(8);

        rightHeaderTitle = new QLabel("编码队列列表", rightPanel);
        rightHeaderTitle->setObjectName("liveListHeader");

        rightBadgeLabel = new QLabel("0 个视频", rightPanel);
        rightBadgeLabel->setObjectName("liveListBadge");

        headerRow->addWidget(rightHeaderTitle);
        headerRow->addStretch();
        headerRow->addWidget(rightBadgeLabel);
        rightLayout->addLayout(headerRow);

        videoListWidget = new QListWidget(rightPanel);
        videoListWidget->setObjectName("liveVideoList");
        videoListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
        videoListWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        videoListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        rightLayout->addWidget(videoListWidget, 1);

        emptyListHint = new QLabel("队列中暂无视频任务\n\n可在【准备文件】页面中\n添加待转码视频", rightPanel);
        emptyListHint->setObjectName("liveEmptyLabel");
        emptyListHint->setAlignment(Qt::AlignCenter);
        emptyListHint->setWordWrap(true);
        emptyListHint->setVisible(true);
        videoListWidget->setVisible(false);
        rightLayout->addWidget(emptyListHint, 1);

        centerLayout->addWidget(rightPanel);

        mainLayout->addLayout(centerLayout, 1);

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

            if (manager && !currentTaskId.isEmpty()) {
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

            if (manager && !currentTaskId.isEmpty()) {
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

        // 切换检视任务联动 (触发 selectTask 并向上通知 taskSelected)
        QObject::connect(taskCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int idx) {
            if (idx < 0) return;
            QString taskId = taskCombo->currentData().toString();
            if (!taskId.isEmpty()) {
                q_ptr->selectTask(taskId);
                emit q_ptr->taskSelected(taskId);
            }
        });

        // 列表选中切换检视任务
        QObject::connect(videoListWidget, &QListWidget::currentItemChanged, [this](QListWidgetItem *current, QListWidgetItem*) {
            if (!current) return;
            QString taskId = current->data(Qt::UserRole).toString();
            if (!taskId.isEmpty() && taskId != currentTaskId) {
                q_ptr->selectTask(taskId);
                emit q_ptr->taskSelected(taskId);
            }
        });
    }

    void updateRunningState() {
        // 顶部控制按钮已按需移除，保留函数以兼容外部状态刷新调用
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

    auto refreshAll = [this, d]() {
        d->refreshTaskCombo();
        d->refreshVideoList();

        QString currentSel = d->currentTaskId;
        if ((currentSel.isEmpty() || !d->manager->getTask(currentSel)) && d->videoListWidget && d->videoListWidget->count() > 0) {
            currentSel = d->videoListWidget->item(0)->data(Qt::UserRole).toString();
        }
        if (!currentSel.isEmpty() && d->manager->getTask(currentSel)) {
            selectTask(currentSel);
        } else {
            checkAndLoadPreview();
        }
    };

    QObject::connect(manager, &TranscodeTaskManager::taskAdded, this, [this, d, refreshAll](TranscodeTask *t) {
        d->connectTaskSignals(t);
        refreshAll();
    });
    QObject::connect(manager, &TranscodeTaskManager::taskRemoved, this, [this, d, refreshAll](const QString &id) {
        if (d->currentTaskId == id) {
            d->currentTaskId.clear();
        }
        refreshAll();
    });
    QObject::connect(manager, &TranscodeTaskManager::taskStateChanged, this, [d](TranscodeTask *t, TaskState) {
        d->updateTaskItem(t);
        d->updateRunningState();
    });

    for (auto *t : manager->allTasks()) {
        d->connectTaskSignals(t);
    }

    refreshAll();
}

void LiveMonitorPage::selectTask(const QString &taskId) {
    Q_D(LiveMonitorPage);
    if (taskId.isEmpty() || !d->manager) return;
    d->currentTaskId = taskId;

    // 1. 同步 taskCombo 下拉框当前索引
    int idx = d->taskCombo->findData(taskId);
    if (idx >= 0 && d->taskCombo->currentIndex() != idx) {
        d->taskCombo->blockSignals(true);
        d->taskCombo->setCurrentIndex(idx);
        d->taskCombo->blockSignals(false);
    }

    // 2. 同步 videoListWidget 列表高亮项
    if (d->videoListWidget) {
        for (int i = 0; i < d->videoListWidget->count(); ++i) {
            auto *item = d->videoListWidget->item(i);
            if (item && item->data(Qt::UserRole).toString() == taskId) {
                if (d->videoListWidget->currentItem() != item) {
                    d->videoListWidget->blockSignals(true);
                    d->videoListWidget->setCurrentItem(item);
                    d->videoListWidget->blockSignals(false);
                }
                break;
            }
        }
    }

    auto *task = d->manager->getTask(taskId);
    if (!task) return;

    QString fileName = QFileInfo(task->inputFilePath()).fileName();
    d->taskNameLabel->setText(QString("当前状态: 待命调参 (已加载: %1)").arg(fileName));

    // 回显该任务专属的水印与去水印配置 (内部带完整的 blockSignals，绝不会反向产生误覆盖)
    setWatermarkConfig(task->config().watermark);
    setDelogoConfig(task->config().delogo);

    // 视口更新专属水印与去水印效果
    d->viewport->setWatermarkConfig(task->config().watermark);
    d->viewport->setDelogoHighlight(task->config().delogo.enabled,
        QRect(task->config().delogo.x, task->config().delogo.y, task->config().delogo.width, task->config().delogo.height));

    QImage thumb = task->thumbnail();
    if (!thumb.isNull()) {
        d->viewport->setStaticPreview(thumb, fileName);
    } else {
        d->viewport->resetToIdle();
        QObject::connect(task, &TranscodeTask::thumbnailLoaded, this, [d, task](const QImage &img) {
            if (d->currentTaskId == task->id() && !d->viewport->hasFrames()) {
                d->viewport->setStaticPreview(img, QFileInfo(task->inputFilePath()).fileName());
            }
        });
    }

    d->updateRunningState();
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

    QString targetId = d->currentTaskId;
    if (targetId.isEmpty() || !d->manager->getTask(targetId)) {
        targetId = d->taskCombo->currentData().toString();
    }
    if ((targetId.isEmpty() || !d->manager->getTask(targetId)) && !d->manager->allTasks().isEmpty()) {
        targetId = d->manager->allTasks().first()->id();
    }

    if (!targetId.isEmpty() && d->manager->getTask(targetId)) {
        selectTask(targetId);
    } else {
        d->taskNameLabel->setText("当前状态: 暂无活跃转码流");
        d->viewport->resetToIdle();
        d->updateRunningState();
    }
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

void LiveMonitorPage::setHwAccelStatus(const QString &) {
    // 硬件加速徽标已按设计移除，保留接口契约
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
    d->delogoCheck->blockSignals(true);
    d->spinX->blockSignals(true);
    d->spinY->blockSignals(true);
    d->spinW->blockSignals(true);
    d->spinH->blockSignals(true);

    d->delogoCheck->setChecked(cfg.enabled);
    d->spinX->setValue(cfg.x);
    d->spinY->setValue(cfg.y);
    d->spinW->setValue(cfg.width);
    d->spinH->setValue(cfg.height);

    d->delogoCheck->blockSignals(false);
    d->spinX->blockSignals(false);
    d->spinY->blockSignals(false);
    d->spinW->blockSignals(false);
    d->spinH->blockSignals(false);

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
