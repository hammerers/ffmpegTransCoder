#include "LiveMonitorPage.h"
#include "VideoCompareWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QSpinBox>
#include <QFrame>

namespace ffmpeg_transform {

class LiveMonitorPagePrivate {
public:
    LiveMonitorPage *q_ptr{nullptr};

    QLabel *titleLabel{nullptr};
    QLabel *taskNameLabel{nullptr};
    QLabel *hwBadgeLabel{nullptr};

    QButtonGroup *modeGroup{nullptr};
    QPushButton *sideBySideBtn{nullptr};
    QPushButton *curtainBtn{nullptr};
    QPushButton *processedBtn{nullptr};

    VideoCompareWidget *viewport{nullptr};

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
        headerLayout->setSpacing(12);

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

        mainLayout->addLayout(headerLayout);

        // 2. 中央双分屏视口
        viewport = new VideoCompareWidget(q_ptr);
        mainLayout->addWidget(viewport, 1);

        // 3. 底部算法与滤镜参数控制条
        auto *bottomCard = new QFrame(q_ptr);
        bottomCard->setObjectName("liveFilterCard");
        auto *bottomLayout = new QHBoxLayout(bottomCard);
        bottomLayout->setContentsMargins(14, 10, 14, 10);
        bottomLayout->setSpacing(12);

        delogoCheck = new QCheckBox("启用内存级去水印与区域平滑", bottomCard);
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

        auto *hintLabel = new QLabel("提示: 拖拽卷帘竖线可同屏无缝比对原画与去水印效果", bottomCard);
        hintLabel->setObjectName("liveHintLabel");
        bottomLayout->addWidget(hintLabel);

        mainLayout->addWidget(bottomCard);

        // 模式切换联动
        QObject::connect(modeGroup, &QButtonGroup::idClicked, [this](int id) {
            if (id == 0) viewport->setCompareMode(CompareMode::SideBySide);
            else if (id == 1) viewport->setCompareMode(CompareMode::CurtainSplit);
            else if (id == 2) viewport->setCompareMode(CompareMode::ProcessedOnly);
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
            emit q_ptr->delogoConfigChanged(cfg);
        };

        QObject::connect(delogoCheck, &QCheckBox::toggled, emitConfig);
        QObject::connect(spinX, QOverload<int>::of(&QSpinBox::valueChanged), emitConfig);
        QObject::connect(spinY, QOverload<int>::of(&QSpinBox::valueChanged), emitConfig);
        QObject::connect(spinW, QOverload<int>::of(&QSpinBox::valueChanged), emitConfig);
        QObject::connect(spinH, QOverload<int>::of(&QSpinBox::valueChanged), emitConfig);
    }
};

LiveMonitorPage::LiveMonitorPage(QWidget *parent)
    : QWidget(parent), d_ptr(std::make_unique<LiveMonitorPagePrivate>()) {
    Q_D(LiveMonitorPage);
    d->q_ptr = this;
    d->initUI();
}

LiveMonitorPage::~LiveMonitorPage() = default;

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
}

void LiveMonitorPage::setHwAccelStatus(const QString &statusText) {
    Q_D(LiveMonitorPage);
    d->hwBadgeLabel->setText(statusText);
}

void LiveMonitorPage::resetToIdle() {
    Q_D(LiveMonitorPage);
    d->taskNameLabel->setText("当前状态: 暂无活跃转码流");
    d->viewport->resetToIdle();
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
