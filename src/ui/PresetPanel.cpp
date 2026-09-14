#include "PresetPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QComboBox>
#include <QSlider>
#include <QTabWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QClipboard>
#include <QGuiApplication>
#include <QStyle>

namespace ffmpeg_transform {

class PresetPanelPrivate {
public:
    PresetPanel *q_ptr{nullptr};

    QTabWidget *tabWidget{nullptr};

    // 常用方案
    QComboBox *presetCombo{nullptr};
    QLabel *presetDescLabel{nullptr};

    // 视频参数
    QComboBox *videoCodecCombo{nullptr};
    QComboBox *resCombo{nullptr};
    QComboBox *fpsCombo{nullptr};
    QSlider *crfSlider{nullptr};
    QLabel *crfValLabel{nullptr};
    QComboBox *presetSpeedCombo{nullptr};

    // 音频与容器
    QComboBox *formatCombo{nullptr};
    QComboBox *audioCodecCombo{nullptr};
    QComboBox *audioBitrateCombo{nullptr};
    QComboBox *sampleRateCombo{nullptr};

    // 等效参数实时预览
    QLineEdit *cmdPreviewEdit{nullptr};
    QPushButton *copyCmdBtn{nullptr};

    bool isAudioOnly{false};

    void initUI() {
        auto *mainLayout = new QVBoxLayout(q_ptr);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(8);

        tabWidget = new QTabWidget(q_ptr);
        tabWidget->setObjectName("presetTabWidget");

        // ------------------ Tab 1: 🌟 常用预设方案 ------------------
        auto *presetTab = new QWidget(tabWidget);
        auto *presetLayout = new QVBoxLayout(presetTab);
        presetLayout->setContentsMargins(14, 14, 14, 14);
        presetLayout->setSpacing(10);

        auto *pHeader = new QLabel("选择适合您的压制/转换预设方案 (快速模式):", presetTab);
        pHeader->setObjectName("panelSectionTitle");

        presetCombo = new QComboBox(presetTab);
        presetCombo->addItem("🌟 通用 MP4 (H.264 + AAC) - 极致多端兼容");
        presetCombo->addItem("⚡ 高效 MKV (H.265/HEVC) - 节省 50% 体积");
        presetCombo->addItem("🎵 纯音频提取 (MP3 - 192kbps 标准音质)");
        presetCombo->addItem("🎧 纯音频提取 (AAC - 320kbps 母带级)");
        presetCombo->addItem("🍎 Apple 影视 (MOV / H.264 原画)");
        presetCombo->addItem("🛠️ 自定义专家模式 (自由设定所有参数)");

        presetDescLabel = new QLabel(presetTab);
        presetDescLabel->setWordWrap(true);
        presetDescLabel->setObjectName("metaTitleLabel");
        presetDescLabel->setText("采用 H.264 视频编码与 AAC 音频编码封装为 MP4，可无缝在电视、手机、车机及各大网页端流畅播放。");

        presetLayout->addWidget(pHeader);
        presetLayout->addWidget(presetCombo);
        presetLayout->addWidget(presetDescLabel);
        presetLayout->addStretch();
        tabWidget->addTab(presetTab, "🌟 常用预设");

        // ------------------ Tab 2: 🎬 视频流参数 ------------------
        auto *videoTab = new QWidget(tabWidget);
        auto *vLayout = new QVBoxLayout(videoTab);
        vLayout->setContentsMargins(14, 14, 14, 14);
        vLayout->setSpacing(10);

        auto *vGrid = new QGridLayout();
        vGrid->setHorizontalSpacing(10);
        vGrid->setVerticalSpacing(8);

        vGrid->addWidget(new QLabel("视频编码器:", videoTab), 0, 0);
        videoCodecCombo = new QComboBox(videoTab);
        videoCodecCombo->addItem("H.264 (libx264 - 推荐)", static_cast<int>(VideoCodecType::H264));
        videoCodecCombo->addItem("H.265 (libx265 - 高压缩)", static_cast<int>(VideoCodecType::H265));
        videoCodecCombo->addItem("流复制 (Stream Copy)", static_cast<int>(VideoCodecType::Copy));
        videoCodecCombo->addItem("禁用视频 (仅提取音频)", static_cast<int>(VideoCodecType::None));
        vGrid->addWidget(videoCodecCombo, 0, 1);

        vGrid->addWidget(new QLabel("画面分辨率:", videoTab), 1, 0);
        resCombo = new QComboBox(videoTab);
        resCombo->addItem("保持原始分辨率", static_cast<int>(ResolutionScale::Original));
        resCombo->addItem("4K 超高清 (3840 x 2160)", static_cast<int>(ResolutionScale::Scale4K));
        resCombo->addItem("1080P 全高清 (1920 x 1080)", static_cast<int>(ResolutionScale::Scale1080p));
        resCombo->addItem("720P 高清 (1280 x 720)", static_cast<int>(ResolutionScale::Scale720p));
        resCombo->addItem("480P 标清 (854 x 480)", static_cast<int>(ResolutionScale::Scale480p));
        vGrid->addWidget(resCombo, 1, 1);

        vGrid->addWidget(new QLabel("输出帧率:", videoTab), 2, 0);
        fpsCombo = new QComboBox(videoTab);
        fpsCombo->addItem("保持原始帧率", static_cast<int>(FpsOption::Original));
        fpsCombo->addItem("60 FPS (丝滑流畅)", static_cast<int>(FpsOption::Fps60));
        fpsCombo->addItem("30 FPS (通用标准)", static_cast<int>(FpsOption::Fps30));
        fpsCombo->addItem("24 FPS (院线电影)", static_cast<int>(FpsOption::Fps24));
        vGrid->addWidget(fpsCombo, 2, 1);

        vGrid->addWidget(new QLabel("画质因子 (CRF):", videoTab), 3, 0);
        auto *crfLayout = new QHBoxLayout();
        crfSlider = new QSlider(Qt::Horizontal, videoTab);
        crfSlider->setRange(18, 35);
        crfSlider->setValue(23);
        crfValLabel = new QLabel("23 (推荐)", videoTab);
        crfValLabel->setFixedWidth(80);
        crfLayout->addWidget(crfSlider);
        crfLayout->addWidget(crfValLabel);
        vGrid->addLayout(crfLayout, 3, 1);

        vGrid->addWidget(new QLabel("编码速度:", videoTab), 4, 0);
        presetSpeedCombo = new QComboBox(videoTab);
        presetSpeedCombo->addItem("极速 (ultrafast)", "ultrafast");
        presetSpeedCombo->addItem("较快 (fast)", "fast");
        presetSpeedCombo->addItem("平衡 (medium - 推荐)", "medium");
        presetSpeedCombo->addItem("慢速 (slow - 高画质)", "slow");
        presetSpeedCombo->setCurrentIndex(2);
        vGrid->addWidget(presetSpeedCombo, 4, 1);

        vLayout->addLayout(vGrid);
        vLayout->addStretch();
        tabWidget->addTab(videoTab, "🎬 视频编码");

        // ------------------ Tab 3: 🎵 音频与封装 ------------------
        auto *audioTab = new QWidget(tabWidget);
        auto *aLayout = new QVBoxLayout(audioTab);
        aLayout->setContentsMargins(14, 14, 14, 14);
        aLayout->setSpacing(10);

        auto *aGrid = new QGridLayout();
        aGrid->setHorizontalSpacing(10);
        aGrid->setVerticalSpacing(8);

        aGrid->addWidget(new QLabel("输出封装格式:", audioTab), 0, 0);
        formatCombo = new QComboBox(audioTab);
        formatCombo->addItem("MP4 格式 (.mp4)", "mp4");
        formatCombo->addItem("MKV 格式 (.mkv)", "mkv");
        formatCombo->addItem("MOV 格式 (.mov)", "mov");
        formatCombo->addItem("AVI 格式 (.avi)", "avi");
        formatCombo->addItem("MP3 音频 (.mp3)", "mp3");
        formatCombo->addItem("AAC 音频 (.aac)", "aac");
        aGrid->addWidget(formatCombo, 0, 1);

        aGrid->addWidget(new QLabel("音频编码器:", audioTab), 1, 0);
        audioCodecCombo = new QComboBox(audioTab);
        audioCodecCombo->addItem("AAC (通用高品质)", static_cast<int>(AudioCodecType::AAC));
        audioCodecCombo->addItem("MP3 (经典通用)", static_cast<int>(AudioCodecType::MP3));
        audioCodecCombo->addItem("流复制 (Stream Copy)", static_cast<int>(AudioCodecType::Copy));
        audioCodecCombo->addItem("静音 (无音频流)", static_cast<int>(AudioCodecType::None));
        aGrid->addWidget(audioCodecCombo, 1, 1);

        aGrid->addWidget(new QLabel("音频比特率:", audioTab), 2, 0);
        audioBitrateCombo = new QComboBox(audioTab);
        audioBitrateCombo->addItem("320 kbps (无损母带级)", 320000);
        audioBitrateCombo->addItem("256 kbps (超高音质)", 256000);
        audioBitrateCombo->addItem("192 kbps (标准品质 - 推荐)", 192000);
        audioBitrateCombo->addItem("128 kbps (适中省流)", 128000);
        audioBitrateCombo->setCurrentIndex(2);
        aGrid->addWidget(audioBitrateCombo, 2, 1);

        aGrid->addWidget(new QLabel("音频采样率:", audioTab), 3, 0);
        sampleRateCombo = new QComboBox(audioTab);
        sampleRateCombo->addItem("48000 Hz (影视标准)", 48000);
        sampleRateCombo->addItem("44100 Hz (CD 音乐标准)", 44100);
        aGrid->addWidget(sampleRateCombo, 3, 1);

        aLayout->addLayout(aGrid);
        aLayout->addStretch();
        tabWidget->addTab(audioTab, "🎵 音频与封装");

        mainLayout->addWidget(tabWidget);

        // ------------------ 【等效 FFmpeg 核心参数实时预览面板】 ------------------
        auto *previewCard = new QWidget(q_ptr);
        previewCard->setObjectName("commandPreviewCard");
        auto *previewLayout = new QVBoxLayout(previewCard);
        previewLayout->setContentsMargins(8, 6, 8, 6);
        previewLayout->setSpacing(4);

        auto *previewHeader = new QHBoxLayout();
        auto *previewTitle = new QLabel("⚡ 等效核心参数实时预览 (Core Args Preview):", previewCard);
        previewTitle->setObjectName("cmdPreviewTitle");

        copyCmdBtn = new QPushButton("📋 复制参数", previewCard);
        copyCmdBtn->setObjectName("dropBrowseBtn");
        copyCmdBtn->setCursor(Qt::PointingHandCursor);

        previewHeader->addWidget(previewTitle);
        previewHeader->addStretch();
        previewHeader->addWidget(copyCmdBtn);

        cmdPreviewEdit = new QLineEdit(previewCard);
        cmdPreviewEdit->setObjectName("commandPreviewText");
        cmdPreviewEdit->setReadOnly(true);

        previewLayout->addLayout(previewHeader);
        previewLayout->addWidget(cmdPreviewEdit);
        mainLayout->addWidget(previewCard);

        // 信号监听
        QObject::connect(copyCmdBtn, &QPushButton::clicked, [this]() {
            QGuiApplication::clipboard()->setText(cmdPreviewEdit->text());
            copyCmdBtn->setText("✅ 已复制");
        });

        QObject::connect(presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int idx) {
            onPresetChanged(idx);
        });

        QObject::connect(crfSlider, &QSlider::valueChanged, [this](int val) {
            QString desc = (val <= 20) ? "视觉无损" : (val <= 24) ? "高质量推荐" : "较小体积";
            crfValLabel->setText(QString("%1 (%2)").arg(val).arg(desc));
            notifyChange();
        });

        auto onComboChange = [this](int) { notifyChange(); };
        QObject::connect(formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), onComboChange);
        QObject::connect(videoCodecCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), onComboChange);
        QObject::connect(audioCodecCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), onComboChange);
        QObject::connect(resCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), onComboChange);
        QObject::connect(fpsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), onComboChange);
        QObject::connect(presetSpeedCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), onComboChange);
        QObject::connect(audioBitrateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), onComboChange);
        QObject::connect(sampleRateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), onComboChange);

        notifyChange();
    }

    void onPresetChanged(int index) {
        copyCmdBtn->setText("📋 复制参数");
        switch (index) {
        case 0: // 通用 MP4
            formatCombo->setCurrentIndex(0); // mp4
            videoCodecCombo->setCurrentIndex(0); // h264
            audioCodecCombo->setCurrentIndex(0); // aac
            resCombo->setCurrentIndex(0); // original
            fpsCombo->setCurrentIndex(0); // original
            crfSlider->setValue(23);
            presetDescLabel->setText("采用 H.264 视频编码与 AAC 音频编码封装为 MP4，多端全平台高兼容。");
            q_ptr->setIsAudioOnly(false);
            break;
        case 1: // 高效 MKV
            formatCombo->setCurrentIndex(1); // mkv
            videoCodecCombo->setCurrentIndex(1); // h265
            audioCodecCombo->setCurrentIndex(0); // aac
            resCombo->setCurrentIndex(0);
            fpsCombo->setCurrentIndex(0);
            crfSlider->setValue(28);
            presetDescLabel->setText("采用 H.265 (HEVC) 高效编码封装为 MKV，相同画质下比 H.264 节省约 40%~50% 磁盘空间。");
            q_ptr->setIsAudioOnly(false);
            break;
        case 2: // 提取音频 MP3
            formatCombo->setCurrentIndex(4); // mp3
            videoCodecCombo->setCurrentIndex(3); // none
            audioCodecCombo->setCurrentIndex(1); // mp3
            audioBitrateCombo->setCurrentIndex(2); // 192k
            presetDescLabel->setText("从视频中无损剥离并转码为标准 MP3 音频，兼容所有音乐播放器与车载设备。");
            q_ptr->setIsAudioOnly(true);
            break;
        case 3: // 提取音频 AAC
            formatCombo->setCurrentIndex(5); // aac
            videoCodecCombo->setCurrentIndex(3); // none
            audioCodecCombo->setCurrentIndex(0); // aac
            audioBitrateCombo->setCurrentIndex(0); // 320k
            presetDescLabel->setText("以 320kbps 高比特率提取并编码为 AAC 高保真音频，音质清脆透亮。");
            q_ptr->setIsAudioOnly(true);
            break;
        case 4: // Apple MOV
            formatCombo->setCurrentIndex(2); // mov
            videoCodecCombo->setCurrentIndex(0); // h264
            audioCodecCombo->setCurrentIndex(0); // aac
            presetDescLabel->setText("封装为 QuickTime MOV 格式，适配 Final Cut Pro、Mac 与 iOS 设备专业剪辑。");
            q_ptr->setIsAudioOnly(false);
            break;
        default:
            presetDescLabel->setText("专家自定义模式：您可以在上方选项卡中自由定制每一个编码参数。");
            break;
        }
        notifyChange();
    }

    void notifyChange() {
        if (copyCmdBtn) copyCmdBtn->setText("📋 复制参数");
        QString args = q_ptr->generateEquivalentArgs();
        if (cmdPreviewEdit) {
            cmdPreviewEdit->setText(args);
        }
        emit q_ptr->configChanged(q_ptr->config());
    }
};

PresetPanel::PresetPanel(QWidget *parent)
    : QWidget(parent), d_ptr(std::make_unique<PresetPanelPrivate>()) {
    Q_D(PresetPanel);
    d->q_ptr = this;
    d->initUI();
}

PresetPanel::~PresetPanel() = default;

bool PresetPanel::isAudioOnly() const {
    return d_ptr->isAudioOnly;
}

void PresetPanel::setIsAudioOnly(bool audioOnly) {
    Q_D(PresetPanel);
    if (d->isAudioOnly == audioOnly) return;
    d->isAudioOnly = audioOnly;

    d->tabWidget->setTabEnabled(1, !audioOnly); // 禁用/启用视频选项卡

    style()->unpolish(this);
    style()->polish(this);
    update();

    emit audioOnlyChanged(audioOnly);
}

TranscodeConfig PresetPanel::config() const {
    Q_D(const PresetPanel);
    TranscodeConfig cfg;
    cfg.containerFormat = d->formatCombo->currentData().toString();
    cfg.videoCodec = static_cast<VideoCodecType>(d->videoCodecCombo->currentData().toInt());
    cfg.audioCodec = static_cast<AudioCodecType>(d->audioCodecCombo->currentData().toInt());
    cfg.resolutionScale = static_cast<ResolutionScale>(d->resCombo->currentData().toInt());
    cfg.fpsOption = static_cast<FpsOption>(d->fpsCombo->currentData().toInt());
    cfg.crf = d->crfSlider->value();
    cfg.preset = d->presetSpeedCombo->currentData().toString();
    cfg.audioBitrate = d->audioBitrateCombo->currentData().toInt();
    cfg.audioSampleRate = d->sampleRateCombo->currentData().toInt();
    return cfg;
}

void PresetPanel::setConfig(const TranscodeConfig &cfg) {
    Q_D(PresetPanel);
    int idx = d->formatCombo->findData(cfg.containerFormat);
    if (idx >= 0) d->formatCombo->setCurrentIndex(idx);

    idx = d->videoCodecCombo->findData(static_cast<int>(cfg.videoCodec));
    if (idx >= 0) d->videoCodecCombo->setCurrentIndex(idx);

    idx = d->audioCodecCombo->findData(static_cast<int>(cfg.audioCodec));
    if (idx >= 0) d->audioCodecCombo->setCurrentIndex(idx);

    idx = d->resCombo->findData(static_cast<int>(cfg.resolutionScale));
    if (idx >= 0) d->resCombo->setCurrentIndex(idx);

    idx = d->fpsCombo->findData(static_cast<int>(cfg.fpsOption));
    if (idx >= 0) d->fpsCombo->setCurrentIndex(idx);

    d->crfSlider->setValue(cfg.crf);

    idx = d->presetSpeedCombo->findData(cfg.preset);
    if (idx >= 0) d->presetSpeedCombo->setCurrentIndex(idx);

    idx = d->audioBitrateCombo->findData(cfg.audioBitrate);
    if (idx >= 0) d->audioBitrateCombo->setCurrentIndex(idx);

    idx = d->sampleRateCombo->findData(cfg.audioSampleRate);
    if (idx >= 0) d->sampleRateCombo->setCurrentIndex(idx);

    setIsAudioOnly(cfg.videoCodec == VideoCodecType::None);
    d->notifyChange();
}

QString PresetPanel::generateEquivalentArgs() const {
    auto cfg = config();
    QStringList args;
    args << "ffmpeg" << "-i" << "<input>";

    // 视频参数
    if (cfg.videoCodec == VideoCodecType::None) {
        args << "-vn";
    } else if (cfg.videoCodec == VideoCodecType::Copy) {
        args << "-c:v" << "copy";
    } else {
        if (cfg.videoCodec == VideoCodecType::H264) args << "-c:v" << "libx264";
        else if (cfg.videoCodec == VideoCodecType::H265) args << "-c:v" << "libx265";

        args << "-crf" << QString::number(cfg.crf);
        args << "-preset" << cfg.preset;
        args << "-pix_fmt" << "yuv420p";

        if (cfg.resolutionScale == ResolutionScale::Scale4K) args << "-s" << "3840x2160";
        else if (cfg.resolutionScale == ResolutionScale::Scale1080p) args << "-s" << "1920x1080";
        else if (cfg.resolutionScale == ResolutionScale::Scale720p) args << "-s" << "1280x720";
        else if (cfg.resolutionScale == ResolutionScale::Scale480p) args << "-s" << "854x480";

        if (cfg.fpsOption == FpsOption::Fps60) args << "-r" << "60";
        else if (cfg.fpsOption == FpsOption::Fps30) args << "-r" << "30";
        else if (cfg.fpsOption == FpsOption::Fps24) args << "-r" << "24";
    }

    // 音频参数
    if (cfg.audioCodec == AudioCodecType::None) {
        args << "-an";
    } else if (cfg.audioCodec == AudioCodecType::Copy) {
        args << "-c:a" << "copy";
    } else {
        if (cfg.audioCodec == AudioCodecType::AAC) args << "-c:a" << "aac";
        else if (cfg.audioCodec == AudioCodecType::MP3) args << "-c:a" << "libmp3lame";

        args << "-b:a" << QString("%1k").arg(cfg.audioBitrate / 1000);
        args << "-ar" << QString::number(cfg.audioSampleRate);
    }

    args << QString("<output>.%1").arg(cfg.containerFormat);
    return args.join(" ");
}

} // namespace ffmpeg_transform
