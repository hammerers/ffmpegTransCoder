#include "ParamConsolePage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QComboBox>
#include <QSlider>
#include <QLineEdit>
#include <QPushButton>
#include <QFrame>
#include <QStackedWidget>
#include <QScrollArea>
#include <QGuiApplication>
#include <QClipboard>
#include <QStyle>

namespace ffmpeg_transform {

class ParamConsolePagePrivate {
public:
    ParamConsolePage *q_ptr{nullptr};

    // 二级导航控件
    QWidget *subSidebar{nullptr};
    QVector<QPushButton*> subNavBtns;
    QStackedWidget *pageStack{nullptr};

    // 视频参数控件
    QComboBox *encoderTypeCombo{nullptr};
    QComboBox *encoderCategoryCombo{nullptr};
    QComboBox *encoderSpecificCombo{nullptr};

    QComboBox *presetCombo{nullptr};
    QComboBox *profileCombo{nullptr};
    QComboBox *tuneCombo{nullptr};
    QLineEdit *gpuEdit{nullptr};
    QLineEdit *threadsEdit{nullptr};

    // 画面帧参数
    QComboBox *resCombo{nullptr};
    QComboBox *fpsCombo{nullptr};

    // 质量参数
    QSlider *crfSlider{nullptr};
    QLabel *crfValLabel{nullptr};

    // 音频与封装
    QComboBox *formatCombo{nullptr};
    QComboBox *audioCodecCombo{nullptr};
    QComboBox *audioBitrateCombo{nullptr};
    QComboBox *sampleRateCombo{nullptr};

    // 快速预设
    QComboBox *quickPresetCombo{nullptr};

    // 实时等效命令预览
    QLineEdit *cmdPreviewEdit{nullptr};
    QPushButton *copyCmdBtn{nullptr};

    void initUI() {
        q_ptr->setObjectName("paramConsolePage");

        auto *rootLayout = new QHBoxLayout(q_ptr);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(0);

        // 1. 左侧二级分类树导航 (Width ~170px)
        initSubSidebar();
        rootLayout->addWidget(subSidebar);

        // 2. 右侧参数内容区
        auto *rightArea = new QWidget(q_ptr);
        rightArea->setObjectName("paramRightArea");
        auto *rightLayout = new QVBoxLayout(rightArea);
        rightLayout->setContentsMargins(20, 16, 20, 16);
        rightLayout->setSpacing(12);

        pageStack = new QStackedWidget(rightArea);
        initPages();
        rightLayout->addWidget(pageStack, 1);

        // 3. 底部等效参数实时预览终端 (与截图 4 底部完全对应)
        initPreviewBar(rightLayout, rightArea);

        rootLayout->addWidget(rightArea, 1);

        bindEvents();
        updatePreview();
    }

    void initSubSidebar() {
        subSidebar = new QWidget(q_ptr);
        subSidebar->setObjectName("paramSubSidebar");
        subSidebar->setFixedWidth(160);

        auto *sidebarLayout = new QVBoxLayout(subSidebar);
        sidebarLayout->setContentsMargins(8, 12, 8, 12);
        sidebarLayout->setSpacing(2);

        auto addSep = [this, sidebarLayout]() {
            auto *line = new QFrame(subSidebar);
            line->setObjectName("paramSubSep");
            line->setFrameShape(QFrame::HLine);
            sidebarLayout->addSpacing(4);
            sidebarLayout->addWidget(line);
            sidebarLayout->addSpacing(4);
        };

        auto addBtn = [this, sidebarLayout](int pageIndex, const QString &text) {
            auto *btn = new QPushButton(text, subSidebar);
            btn->setObjectName("paramSubBtn");
            btn->setCheckable(true);
            btn->setCursor(Qt::PointingHandCursor);
            subNavBtns.append(btn);
            sidebarLayout->addWidget(btn);

            QObject::connect(btn, &QPushButton::clicked, [this, pageIndex, btn]() {
                pageStack->setCurrentIndex(pageIndex);
                for (auto *b : subNavBtns) {
                    bool sel = (b == btn);
                    b->setChecked(sel);
                    b->setProperty("active", sel);
                    b->style()->unpolish(b);
                    b->style()->polish(b);
                    b->update();
                }
            });
        };

        addBtn(0, "预设管理");
        addSep();
        addBtn(1, "视频编码器");
        addBtn(2, "画面与帧率");
        addBtn(3, "质量控制");
        addSep();
        addBtn(4, "音频与封装");
        addBtn(5, "输出文件设置");

        sidebarLayout->addStretch();
    }

    void initPages() {
        // 创建各页面 (仅保留具备完备交互控件的核心参数页)
        pageStack->addWidget(createPresetMgrPage());    // 0: 预设管理
        pageStack->addWidget(createVideoEncoderPage());  // 1: 视频参数 | 编码器
        pageStack->addWidget(createVideoFramePage());    // 2: 视频参数 | 画面帧
        pageStack->addWidget(createVideoQualityPage());  // 3: 视频参数 | 质量
        pageStack->addWidget(createAudioPage());         // 4: 音频参数
        pageStack->addWidget(createOutputPage());       // 5: 输出文件设置

        // 默认选中 "视频编码器"
        pageStack->setCurrentIndex(1);
        if (subNavBtns.size() > 1) {
            subNavBtns[1]->setChecked(true);
            subNavBtns[1]->setProperty("active", true);
        }
    }

    QWidget* createSection(const QString &title, const QString &subtitle, QWidget *content) {
        auto *sec = new QWidget(pageStack);
        sec->setObjectName("paramSectionWidget");
        auto *layout = new QVBoxLayout(sec);
        layout->setContentsMargins(0, 0, 0, 8);
        layout->setSpacing(4);

        auto *tLabel = new QLabel(title, sec);
        tLabel->setObjectName("paramSectionTitle");

        auto *sLabel = new QLabel(subtitle, sec);
        sLabel->setObjectName("paramSectionSubtitle");
        sLabel->setWordWrap(true);

        layout->addWidget(tLabel);
        layout->addWidget(sLabel);
        layout->addWidget(content);
        return sec;
    }

    QWidget* createTagRow(const QString &tag, QWidget *control) {
        auto *row = new QWidget(pageStack);
        auto *rLayout = new QHBoxLayout(row);
        rLayout->setContentsMargins(0, 2, 0, 2);
        rLayout->setSpacing(8);

        auto *tagLabel = new QLabel(tag, row);
        tagLabel->setObjectName("paramTagBadge");
        tagLabel->setFixedWidth(90);
        tagLabel->setAlignment(Qt::AlignCenter);

        rLayout->addWidget(tagLabel);
        rLayout->addWidget(control, 1);
        return row;
    }

    // 核心页面：视频参数 | 编码器 (严格对照截图 4)
    QWidget* createVideoEncoderPage() {
        auto *scroll = new QScrollArea(pageStack);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);

        auto *panel = new QWidget(scroll);
        auto *layout = new QVBoxLayout(panel);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(14);

        // 1. 视频编码器
        auto *encRow = new QWidget(panel);
        auto *encLayout = new QHBoxLayout(encRow);
        encLayout->setContentsMargins(0, 0, 0, 0);
        encLayout->setSpacing(10);

        encoderTypeCombo = new QComboBox(encRow);
        encoderTypeCombo->addItem("类型: CPU 软件编码");
        encoderTypeCombo->addItem("类型: NVIDIA 硬件编码");
        encoderTypeCombo->addItem("类型: Intel QSV 编码");

        encoderCategoryCombo = new QComboBox(encRow);
        encoderCategoryCombo->addItem("分类: H.264 / AVC");
        encoderCategoryCombo->addItem("分类: H.265 / HEVC");
        encoderCategoryCombo->addItem("分类: 流复制 (Copy)");
        encoderCategoryCombo->addItem("分类: 禁用视频 (纯音频)");

        encoderSpecificCombo = new QComboBox(encRow);
        encoderSpecificCombo->addItem("具体编码: libx264");
        encoderSpecificCombo->addItem("具体编码: libx265");
        encoderSpecificCombo->addItem("具体编码: copy");

        encLayout->addWidget(encoderTypeCombo);
        encLayout->addWidget(encoderCategoryCombo);
        encLayout->addWidget(encoderSpecificCombo);

        layout->addWidget(createSection("视频编码器", "依次选择类别，再选具体；可编辑设置文件添加自定义 lib = CPU, nvenc = NVIDIA, qsv = Intel, amf = AMD", encRow));

        // 2. 编码预设
        presetCombo = new QComboBox(panel);
        presetCombo->addItem("ultrafast (极速压缩)");
        presetCombo->addItem("veryfast (快速压缩)");
        presetCombo->addItem("fast (较快压缩)");
        presetCombo->addItem("medium (平衡推荐)");
        presetCombo->addItem("slow (高质量慢速)");
        presetCombo->addItem("slower (极高画质)");
        presetCombo->setCurrentIndex(3);
        layout->addWidget(createSection("编码预设", "如何平衡压缩度和速度，往上越慢，往下越快", createTagRow("[-preset]", presetCombo)));

        // 3. 配置文件
        profileCombo = new QComboBox(panel);
        profileCombo->addItem("auto (自动适配规格)");
        profileCombo->addItem("baseline (基础兼容)");
        profileCombo->addItem("main (主流规格)");
        profileCombo->addItem("high (高级高规格)");
        layout->addWidget(createSection("配置文件", "控制要支持怎样的技术规格和功能，一般不用指定", createTagRow("[-profile:v]", profileCombo)));

        // 4. 场景优化
        tuneCombo = new QComboBox(panel);
        tuneCombo->addItem("none (默认通用模式)");
        tuneCombo->addItem("film (电影胶片噪点)");
        tuneCombo->addItem("animation (二维动画)");
        tuneCombo->addItem("grain (微小颗粒保留)");
        tuneCombo->addItem("fastdecode (快速解码优化)");
        tuneCombo->addItem("zerolatency (极低延迟直播)");
        layout->addWidget(createSection("场景优化", "对特定需求的专项优化，例如 CPU 编码的颗粒保留或是 GPU 编码的特调模式", createTagRow("[-tune]", tuneCombo)));

        // 5. 性能选项
        auto *perfContainer = new QWidget(panel);
        auto *perfLayout = new QVBoxLayout(perfContainer);
        perfLayout->setContentsMargins(0, 0, 0, 0);
        perfLayout->setSpacing(6);

        gpuEdit = new QLineEdit("0", perfContainer);
        gpuEdit->setPlaceholderText("GPU 索引序号 (默认 0)");

        threadsEdit = new QLineEdit("0", perfContainer);
        threadsEdit->setPlaceholderText("指定 CPU 编码线程数，不一定有效，编码器有自己的逻辑 (0 为自动)");

        perfLayout->addWidget(createTagRow("[-gpu]", gpuEdit));
        perfLayout->addWidget(createTagRow("[-threads]", threadsEdit));

        layout->addWidget(createSection("性能选项", "通常不需要考虑，也不一定起作用", perfContainer));
        layout->addStretch();

        scroll->setWidget(panel);
        return scroll;
    }

    // 画面帧页面
    QWidget* createVideoFramePage() {
        auto *panel = new QWidget(pageStack);
        auto *layout = new QVBoxLayout(panel);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(14);

        resCombo = new QComboBox(panel);
        resCombo->addItem("保持原始分辨率", static_cast<int>(ResolutionScale::Original));
        resCombo->addItem("4K 超高清 (3840 x 2160)", static_cast<int>(ResolutionScale::Scale4K));
        resCombo->addItem("1080P 全高清 (1920 x 1080)", static_cast<int>(ResolutionScale::Scale1080p));
        resCombo->addItem("720P 高清 (1280 x 720)", static_cast<int>(ResolutionScale::Scale720p));
        resCombo->addItem("480P 标清 (854 x 480)", static_cast<int>(ResolutionScale::Scale480p));
        layout->addWidget(createSection("画面分辨率缩放", "使用 FFmpeg 高保真 sws_scale 双三次平滑缩放算法", createTagRow("[-s]", resCombo)));

        fpsCombo = new QComboBox(panel);
        fpsCombo->addItem("保持原始帧率", static_cast<int>(FpsOption::Original));
        fpsCombo->addItem("60 FPS (高刷新率丝滑)", static_cast<int>(FpsOption::Fps60));
        fpsCombo->addItem("30 FPS (通用网络视频)", static_cast<int>(FpsOption::Fps30));
        fpsCombo->addItem("24 FPS (院线电影感)", static_cast<int>(FpsOption::Fps24));
        layout->addWidget(createSection("画面帧率 (FPS)", "时间基与帧率重新同步", createTagRow("[-r]", fpsCombo)));

        layout->addStretch();
        return panel;
    }

    // 质量控制页面
    QWidget* createVideoQualityPage() {
        auto *panel = new QWidget(pageStack);
        auto *layout = new QVBoxLayout(panel);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(14);

        auto *sliderBox = new QWidget(panel);
        auto *sLayout = new QHBoxLayout(sliderBox);
        sLayout->setContentsMargins(0, 0, 0, 0);
        sLayout->setSpacing(10);

        crfSlider = new QSlider(Qt::Horizontal, sliderBox);
        crfSlider->setRange(18, 35);
        crfSlider->setValue(23);

        crfValLabel = new QLabel("23 (推荐高质量)", sliderBox);
        crfValLabel->setFixedWidth(130);

        sLayout->addWidget(crfSlider);
        sLayout->addWidget(crfValLabel);

        layout->addWidget(createSection("质量因子 (CRF 恒定质量模式)", "数值越小画质越高、体积越大；18-20 为视觉无损，21-24 为高质量推荐，25-28 为适中体积", createTagRow("[-crf]", sliderBox)));
        layout->addStretch();
        return panel;
    }

    // 音频与封装页面
    QWidget* createAudioPage() {
        auto *panel = new QWidget(pageStack);
        auto *layout = new QVBoxLayout(panel);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(14);

        audioCodecCombo = new QComboBox(panel);
        audioCodecCombo->addItem("AAC (高保真通用编码)", static_cast<int>(AudioCodecType::AAC));
        audioCodecCombo->addItem("MP3 (经典兼容格式)", static_cast<int>(AudioCodecType::MP3));
        audioCodecCombo->addItem("流复制 (Stream Copy)", static_cast<int>(AudioCodecType::Copy));
        audioCodecCombo->addItem("静音 (去除音频轨道)", static_cast<int>(AudioCodecType::None));
        layout->addWidget(createSection("音频编码格式", "选择目标音频流编码器", createTagRow("[-c:a]", audioCodecCombo)));

        audioBitrateCombo = new QComboBox(panel);
        audioBitrateCombo->addItem("320 kbps (最高保真母带级)", 320000);
        audioBitrateCombo->addItem("256 kbps (超高音质)", 256000);
        audioBitrateCombo->addItem("192 kbps (标准音质 - 推荐)", 192000);
        audioBitrateCombo->addItem("128 kbps (网络流媒体适中)", 128000);
        audioBitrateCombo->setCurrentIndex(2);
        layout->addWidget(createSection("音频比特率", "音频目标压缩速率", createTagRow("[-b:a]", audioBitrateCombo)));

        sampleRateCombo = new QComboBox(panel);
        sampleRateCombo->addItem("48000 Hz (影视蓝光工业标准)", 48000);
        sampleRateCombo->addItem("44100 Hz (CD 无损音乐标准)", 44100);
        layout->addWidget(createSection("音频重采样频率", "由 FFmpeg swr_convert 执行高精度音频重采样", createTagRow("[-ar]", sampleRateCombo)));

        layout->addStretch();
        return panel;
    }

    // 格式输出页面
    QWidget* createOutputPage() {
        auto *panel = new QWidget(pageStack);
        auto *layout = new QVBoxLayout(panel);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(14);

        formatCombo = new QComboBox(panel);
        formatCombo->addItem("MP4 格式 (.mp4) - 极致多端跨平台兼容", "mp4");
        formatCombo->addItem("MKV 格式 (.mkv) - 全功能无损媒体封装", "mkv");
        formatCombo->addItem("MOV 格式 (.mov) - Apple 影视专业剪辑", "mov");
        formatCombo->addItem("AVI 格式 (.avi) - 传统通用封装", "avi");
        formatCombo->addItem("MP3 音频 (.mp3) - 纯音频通用文件", "mp3");
        formatCombo->addItem("AAC 音频 (.aac) - 高保真音频流", "aac");
        layout->addWidget(createSection("输出封装格式", "选择目标容器封装格式", createTagRow("格式", formatCombo)));

        layout->addStretch();
        return panel;
    }

    // 预设管理页面
    QWidget* createPresetMgrPage() {
        auto *panel = new QWidget(pageStack);
        auto *layout = new QVBoxLayout(panel);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(14);

        quickPresetCombo = new QComboBox(panel);
        quickPresetCombo->addItem("通用 MP4 (H.264 + AAC) - 全网兼容推荐");
        quickPresetCombo->addItem("高效 MKV (H.265/HEVC) - 节省 50% 磁盘空间");
        quickPresetCombo->addItem("纯音频剥离 (MP3 - 192kbps 标准音质)");
        quickPresetCombo->addItem("纯音频剥离 (AAC - 320kbps 母带级)");
        quickPresetCombo->addItem("Apple 影视 (MOV / H.264 原画)");
        layout->addWidget(createSection("快速预设加载", "一键应用工业级成熟调优参数方案", createTagRow("快速预设", quickPresetCombo)));

        layout->addStretch();
        return panel;
    }

    void initPreviewBar(QVBoxLayout *rightLayout, QWidget *rightArea) {
        auto *previewCard = new QWidget(rightArea);
        previewCard->setObjectName("commandPreviewCard");
        auto *previewLayout = new QHBoxLayout(previewCard);
        previewLayout->setContentsMargins(12, 6, 12, 6);
        previewLayout->setSpacing(10);

        cmdPreviewEdit = new QLineEdit(previewCard);
        cmdPreviewEdit->setObjectName("commandPreviewText");
        cmdPreviewEdit->setReadOnly(true);

        copyCmdBtn = new QPushButton("复制参数", previewCard);
        copyCmdBtn->setObjectName("btnCopyCmd");
        copyCmdBtn->setCursor(Qt::PointingHandCursor);

        previewLayout->addWidget(cmdPreviewEdit, 1);
        previewLayout->addWidget(copyCmdBtn);
        rightLayout->addWidget(previewCard);
    }

    void bindEvents() {
        auto onAnyChange = [this](int) {
            updatePreview();
            emit q_ptr->configChanged(q_ptr->config());
        };

        QObject::connect(encoderCategoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int idx) {
            if (idx == 0) {
                encoderSpecificCombo->clear();
                encoderSpecificCombo->addItem("具体编码: libx264");
            } else if (idx == 1) {
                encoderSpecificCombo->clear();
                encoderSpecificCombo->addItem("具体编码: libx265");
            } else if (idx == 2) {
                encoderSpecificCombo->clear();
                encoderSpecificCombo->addItem("具体编码: copy");
            } else if (idx == 3) {
                encoderSpecificCombo->clear();
                encoderSpecificCombo->addItem("具体编码: none");
            }
            updatePreview();
            emit q_ptr->configChanged(q_ptr->config());
        });

        QObject::connect(presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), onAnyChange);
        QObject::connect(profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), onAnyChange);
        QObject::connect(tuneCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), onAnyChange);
        QObject::connect(resCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), onAnyChange);
        QObject::connect(fpsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), onAnyChange);
        QObject::connect(formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), onAnyChange);
        QObject::connect(audioCodecCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), onAnyChange);
        QObject::connect(audioBitrateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), onAnyChange);
        QObject::connect(sampleRateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), onAnyChange);

        QObject::connect(crfSlider, &QSlider::valueChanged, [this](int val) {
            QString desc = (val <= 20) ? "视觉无损" : (val <= 24) ? "高质量推荐" : "较小体积";
            crfValLabel->setText(QString("%1 (%2)").arg(val).arg(desc));
            updatePreview();
            emit q_ptr->configChanged(q_ptr->config());
        });

        // 快速预设联动
        QObject::connect(quickPresetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int idx) {
            switch (idx) {
            case 0: // 通用 MP4
                formatCombo->setCurrentIndex(0);
                encoderCategoryCombo->setCurrentIndex(0);
                audioCodecCombo->setCurrentIndex(0);
                crfSlider->setValue(23);
                break;
            case 1: // 高效 MKV
                formatCombo->setCurrentIndex(1);
                encoderCategoryCombo->setCurrentIndex(1);
                audioCodecCombo->setCurrentIndex(0);
                crfSlider->setValue(28);
                break;
            case 2: // 纯音频 MP3
                formatCombo->setCurrentIndex(4);
                encoderCategoryCombo->setCurrentIndex(3);
                audioCodecCombo->setCurrentIndex(1);
                audioBitrateCombo->setCurrentIndex(2);
                break;
            case 3: // 纯音频 AAC
                formatCombo->setCurrentIndex(5);
                encoderCategoryCombo->setCurrentIndex(3);
                audioCodecCombo->setCurrentIndex(0);
                audioBitrateCombo->setCurrentIndex(0);
                break;
            case 4: // MOV
                formatCombo->setCurrentIndex(2);
                encoderCategoryCombo->setCurrentIndex(0);
                audioCodecCombo->setCurrentIndex(0);
                break;
            }
            updatePreview();
            emit q_ptr->configChanged(q_ptr->config());
        });

        QObject::connect(copyCmdBtn, &QPushButton::clicked, [this]() {
            QGuiApplication::clipboard()->setText(cmdPreviewEdit->text());
            copyCmdBtn->setText("已复制");
        });
    }

    void updatePreview() {
        if (copyCmdBtn) copyCmdBtn->setText("复制参数");
        if (cmdPreviewEdit) {
            cmdPreviewEdit->setText(q_ptr->generateEquivalentArgs());
        }
    }
};

ParamConsolePage::ParamConsolePage(QWidget *parent)
    : QWidget(parent), d_ptr(std::make_unique<ParamConsolePagePrivate>()) {
    Q_D(ParamConsolePage);
    d->q_ptr = this;
    d->initUI();
}

ParamConsolePage::~ParamConsolePage() = default;

TranscodeConfig ParamConsolePage::config() const {
    Q_D(const ParamConsolePage);
    TranscodeConfig cfg;

    cfg.containerFormat = d->formatCombo->currentData().toString();
    int catIdx = d->encoderCategoryCombo->currentIndex();
    if (catIdx == 0) cfg.videoCodec = VideoCodecType::H264;
    else if (catIdx == 1) cfg.videoCodec = VideoCodecType::H265;
    else if (catIdx == 2) cfg.videoCodec = VideoCodecType::Copy;
    else if (catIdx == 3) cfg.videoCodec = VideoCodecType::None;

    cfg.resolutionScale = static_cast<ResolutionScale>(d->resCombo->currentData().toInt());
    cfg.fpsOption = static_cast<FpsOption>(d->fpsCombo->currentData().toInt());
    cfg.crf = d->crfSlider->value();

    QString presText = d->presetCombo->currentText().split(" ").first();
    cfg.preset = presText;

    cfg.audioCodec = static_cast<AudioCodecType>(d->audioCodecCombo->currentData().toInt());
    cfg.audioBitrate = d->audioBitrateCombo->currentData().toInt();
    cfg.audioSampleRate = d->sampleRateCombo->currentData().toInt();

    return cfg;
}

void ParamConsolePage::setConfig(const TranscodeConfig &cfg) {
    Q_D(ParamConsolePage);
    int fmtIdx = d->formatCombo->findData(cfg.containerFormat);
    if (fmtIdx >= 0) d->formatCombo->setCurrentIndex(fmtIdx);

    if (cfg.videoCodec == VideoCodecType::H264) d->encoderCategoryCombo->setCurrentIndex(0);
    else if (cfg.videoCodec == VideoCodecType::H265) d->encoderCategoryCombo->setCurrentIndex(1);
    else if (cfg.videoCodec == VideoCodecType::Copy) d->encoderCategoryCombo->setCurrentIndex(2);
    else if (cfg.videoCodec == VideoCodecType::None) d->encoderCategoryCombo->setCurrentIndex(3);

    int resIdx = d->resCombo->findData(static_cast<int>(cfg.resolutionScale));
    if (resIdx >= 0) d->resCombo->setCurrentIndex(resIdx);

    int fpsIdx = d->fpsCombo->findData(static_cast<int>(cfg.fpsOption));
    if (fpsIdx >= 0) d->fpsCombo->setCurrentIndex(fpsIdx);

    d->crfSlider->setValue(cfg.crf);

    int acIdx = d->audioCodecCombo->findData(static_cast<int>(cfg.audioCodec));
    if (acIdx >= 0) d->audioCodecCombo->setCurrentIndex(acIdx);

    int abIdx = d->audioBitrateCombo->findData(cfg.audioBitrate);
    if (abIdx >= 0) d->audioBitrateCombo->setCurrentIndex(abIdx);

    int arIdx = d->sampleRateCombo->findData(cfg.audioSampleRate);
    if (arIdx >= 0) d->sampleRateCombo->setCurrentIndex(arIdx);

    d->updatePreview();
}

QString ParamConsolePage::generateEquivalentArgs() const {
    Q_D(const ParamConsolePage);
    QStringList args;
    args << "ffmpeg" << "-i" << "<input>";

    int catIdx = d->encoderCategoryCombo->currentIndex();
    if (catIdx == 3) {
        args << "-vn";
    } else if (catIdx == 2) {
        args << "-c:v" << "copy";
    } else {
        QString vcodec = (catIdx == 0) ? "libx264" : "libx265";
        args << "-c:v" << vcodec;
        args << "-crf" << QString::number(d->crfSlider->value());
        args << "-preset" << d->presetCombo->currentText().split(" ").first();

        int profIdx = d->profileCombo->currentIndex();
        if (profIdx > 0) {
            args << "-profile:v" << d->profileCombo->currentText().split(" ").first();
        }

        int tuneIdx = d->tuneCombo->currentIndex();
        if (tuneIdx > 0) {
            args << "-tune" << d->tuneCombo->currentText().split(" ").first();
        }

        auto scale = static_cast<ResolutionScale>(d->resCombo->currentData().toInt());
        if (scale == ResolutionScale::Scale4K) args << "-s" << "3840x2160";
        else if (scale == ResolutionScale::Scale1080p) args << "-s" << "1920x1080";
        else if (scale == ResolutionScale::Scale720p) args << "-s" << "1280x720";
        else if (scale == ResolutionScale::Scale480p) args << "-s" << "854x480";

        auto fps = static_cast<FpsOption>(d->fpsCombo->currentData().toInt());
        if (fps == FpsOption::Fps60) args << "-r" << "60";
        else if (fps == FpsOption::Fps30) args << "-r" << "30";
        else if (fps == FpsOption::Fps24) args << "-r" << "24";

        args << "-pix_fmt" << "yuv420p";
    }

    auto acodec = static_cast<AudioCodecType>(d->audioCodecCombo->currentData().toInt());
    if (acodec == AudioCodecType::None) {
        args << "-an";
    } else if (acodec == AudioCodecType::Copy) {
        args << "-c:a" << "copy";
    } else {
        QString aStr = (acodec == AudioCodecType::AAC) ? "aac" : "libmp3lame";
        args << "-c:a" << aStr;
        args << "-b:a" << QString("%1k").arg(d->audioBitrateCombo->currentData().toInt() / 1000);
        args << "-ar" << QString::number(d->sampleRateCombo->currentData().toInt());
    }

    args << QString("<output>.%1").arg(d->formatCombo->currentData().toString());
    return args.join(" ");
}

} // namespace ffmpeg_transform
