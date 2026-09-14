#include "PresetPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QComboBox>
#include <QSlider>
#include <QGroupBox>
#include <QStyle>

namespace ffmpeg_transform {

class PresetPanelPrivate {
public:
    PresetPanel *q_ptr{nullptr};

    QComboBox *presetCombo{nullptr};
    QComboBox *formatCombo{nullptr};
    QComboBox *videoCodecCombo{nullptr};
    QComboBox *audioCodecCombo{nullptr};
    QComboBox *resCombo{nullptr};
    QComboBox *fpsCombo{nullptr};
    QSlider *crfSlider{nullptr};
    QLabel *crfValLabel{nullptr};
    QComboBox *presetSpeedCombo{nullptr};
    QComboBox *audioBitrateCombo{nullptr};

    QWidget *videoSettingsGroup{nullptr};
    bool isAudioOnly{false};

    void initUI() {
        auto *mainLayout = new QVBoxLayout(q_ptr);
        mainLayout->setContentsMargins(12, 12, 12, 12);
        mainLayout->setSpacing(12);

        // 1. 快速预设
        auto *presetLayout = new QVBoxLayout();
        auto *presetTitle = new QLabel("快速目标预设", q_ptr);
        presetTitle->setObjectName("panelSectionTitle");
        presetCombo = new QComboBox(q_ptr);
        presetCombo->setObjectName("presetComboBox");
        presetCombo->addItem("🌟 通用 MP4 (H.264 + AAC) - 极致兼容");
        presetCombo->addItem("⚡ 高效 MKV (H.265/HEVC) - 极高压缩率");
        presetCombo->addItem("🎵 提取音频 (MP3 - 192kbps)");
        presetCombo->addItem("🎧 提取音频 (AAC - 高音质)");
        presetCombo->addItem("🍎 Apple 格式 (MOV / H.264)");
        presetCombo->addItem("🛠️ 自定义配置 (专家模式)");

        presetLayout->addWidget(presetTitle);
        presetLayout->addWidget(presetCombo);
        mainLayout->addLayout(presetLayout);

        // 2. 视频参数设置
        videoSettingsGroup = new QWidget(q_ptr);
        auto *vLayout = new QVBoxLayout(videoSettingsGroup);
        vLayout->setContentsMargins(0, 0, 0, 0);
        vLayout->setSpacing(8);

        auto *vTitle = new QLabel("视频编码设置", videoSettingsGroup);
        vTitle->setObjectName("panelSectionTitle");
        vLayout->addWidget(vTitle);

        auto *vGrid = new QGridLayout();
        vGrid->setHorizontalSpacing(10);
        vGrid->setVerticalSpacing(8);

        // 视频编码
        vGrid->addWidget(new QLabel("编码器:", videoSettingsGroup), 0, 0);
        videoCodecCombo = new QComboBox(videoSettingsGroup);
        videoCodecCombo->addItem("H.264 (AVC - 兼容性最佳)", static_cast<int>(VideoCodecType::H264));
        videoCodecCombo->addItem("H.265 (HEVC - 体积更小)", static_cast<int>(VideoCodecType::H265));
        videoCodecCombo->addItem("直接复制 (Stream Copy)", static_cast<int>(VideoCodecType::Copy));
        videoCodecCombo->addItem("无视频 (纯音频)", static_cast<int>(VideoCodecType::None));
        vGrid->addWidget(videoCodecCombo, 0, 1);

        // 分辨率
        vGrid->addWidget(new QLabel("分辨率:", videoSettingsGroup), 1, 0);
        resCombo = new QComboBox(videoSettingsGroup);
        resCombo->addItem("保持原始分辨率", static_cast<int>(ResolutionScale::Original));
        resCombo->addItem("4K (3840 x 2160)", static_cast<int>(ResolutionScale::Scale4K));
        resCombo->addItem("1080P (1920 x 1080)", static_cast<int>(ResolutionScale::Scale1080p));
        resCombo->addItem("720P (1280 x 720)", static_cast<int>(ResolutionScale::Scale720p));
        resCombo->addItem("480P (854 x 480)", static_cast<int>(ResolutionScale::Scale480p));
        vGrid->addWidget(resCombo, 1, 1);

        // 帧率
        vGrid->addWidget(new QLabel("帧率:", videoSettingsGroup), 2, 0);
        fpsCombo = new QComboBox(videoSettingsGroup);
        fpsCombo->addItem("保持原始帧率", static_cast<int>(FpsOption::Original));
        fpsCombo->addItem("60 FPS (高流畅)", static_cast<int>(FpsOption::Fps60));
        fpsCombo->addItem("30 FPS (通用)", static_cast<int>(FpsOption::Fps30));
        fpsCombo->addItem("24 FPS (电影质感)", static_cast<int>(FpsOption::Fps24));
        vGrid->addWidget(fpsCombo, 2, 1);

        // CRF 画质滑块
        vGrid->addWidget(new QLabel("画质 (CRF):", videoSettingsGroup), 3, 0);
        auto *crfLayout = new QHBoxLayout();
        crfSlider = new QSlider(Qt::Horizontal, videoSettingsGroup);
        crfSlider->setRange(18, 35);
        crfSlider->setValue(23);
        crfValLabel = new QLabel("23 (推荐)", videoSettingsGroup);
        crfValLabel->setFixedWidth(70);
        crfLayout->addWidget(crfSlider);
        crfLayout->addWidget(crfValLabel);
        vGrid->addLayout(crfLayout, 3, 1);

        // 编码速度预设
        vGrid->addWidget(new QLabel("转码速度:", videoSettingsGroup), 4, 0);
        presetSpeedCombo = new QComboBox(videoSettingsGroup);
        presetSpeedCombo->addItem("超高速 (ultrafast)", "ultrafast");
        presetSpeedCombo->addItem("快速 (fast)", "fast");
        presetSpeedCombo->addItem("平衡 (medium - 推荐)", "medium");
        presetSpeedCombo->addItem("慢速 (slow - 高质量)", "slow");
        presetSpeedCombo->setCurrentIndex(2);
        vGrid->addWidget(presetSpeedCombo, 4, 1);

        vLayout->addLayout(vGrid);
        mainLayout->addWidget(videoSettingsGroup);

        // 3. 音频参数设置
        auto *audioGroup = new QWidget(q_ptr);
        auto *aLayout = new QVBoxLayout(audioGroup);
        aLayout->setContentsMargins(0, 0, 0, 0);
        aLayout->setSpacing(8);

        auto *aTitle = new QLabel("音频编码与封装设置", audioGroup);
        aTitle->setObjectName("panelSectionTitle");
        aLayout->addWidget(aTitle);

        auto *aGrid = new QGridLayout();
        aGrid->setHorizontalSpacing(10);
        aGrid->setVerticalSpacing(8);

        // 容器格式
        aGrid->addWidget(new QLabel("输出格式:", audioGroup), 0, 0);
        formatCombo = new QComboBox(audioGroup);
        formatCombo->addItem("MP4 (.mp4)", "mp4");
        formatCombo->addItem("MKV (.mkv)", "mkv");
        formatCombo->addItem("MOV (.mov)", "mov");
        formatCombo->addItem("AVI (.avi)", "avi");
        formatCombo->addItem("MP3 (.mp3)", "mp3");
        formatCombo->addItem("AAC (.aac)", "aac");
        aGrid->addWidget(formatCombo, 0, 1);

        // 音频编码
        aGrid->addWidget(new QLabel("音频编码:", audioGroup), 1, 0);
        audioCodecCombo = new QComboBox(audioGroup);
        audioCodecCombo->addItem("AAC (通用高保真)", static_cast<int>(AudioCodecType::AAC));
        audioCodecCombo->addItem("MP3 (经典标准)", static_cast<int>(AudioCodecType::MP3));
        audioCodecCombo->addItem("直接复制 (Stream Copy)", static_cast<int>(AudioCodecType::Copy));
        audioCodecCombo->addItem("静音 (无音频)", static_cast<int>(AudioCodecType::None));
        aGrid->addWidget(audioCodecCombo, 1, 1);

        // 音频码率
        aGrid->addWidget(new QLabel("音频码率:", audioGroup), 2, 0);
        audioBitrateCombo = new QComboBox(audioGroup);
        audioBitrateCombo->addItem("320 kbps (录音室级)", 320000);
        audioBitrateCombo->addItem("256 kbps (高品质)", 256000);
        audioBitrateCombo->addItem("192 kbps (标准 - 推荐)", 192000);
        audioBitrateCombo->addItem("128 kbps (适中)", 128000);
        audioBitrateCombo->setCurrentIndex(2);
        aGrid->addWidget(audioBitrateCombo, 2, 1);

        aLayout->addLayout(aGrid);
        mainLayout->addWidget(audioGroup);
        mainLayout->addStretch();

        // 信号绑定
        QObject::connect(presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int idx) {
            onPresetChanged(idx);
        });

        QObject::connect(crfSlider, &QSlider::valueChanged, [this](int val) {
            QString desc = (val <= 20) ? "视觉无损" : (val <= 24) ? "高画质(推荐)" : "较低画质";
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
    }

    void onPresetChanged(int index) {
        switch (index) {
        case 0: // 通用 MP4
            formatCombo->setCurrentIndex(0); // mp4
            videoCodecCombo->setCurrentIndex(0); // h264
            audioCodecCombo->setCurrentIndex(0); // aac
            resCombo->setCurrentIndex(0); // original
            fpsCombo->setCurrentIndex(0); // original
            crfSlider->setValue(23);
            q_ptr->setIsAudioOnly(false);
            break;
        case 1: // 高效 MKV
            formatCombo->setCurrentIndex(1); // mkv
            videoCodecCombo->setCurrentIndex(1); // h265
            audioCodecCombo->setCurrentIndex(0); // aac
            resCombo->setCurrentIndex(0);
            fpsCombo->setCurrentIndex(0);
            crfSlider->setValue(28);
            q_ptr->setIsAudioOnly(false);
            break;
        case 2: // 提取音频 MP3
            formatCombo->setCurrentIndex(4); // mp3
            videoCodecCombo->setCurrentIndex(3); // none
            audioCodecCombo->setCurrentIndex(1); // mp3
            audioBitrateCombo->setCurrentIndex(2); // 192k
            q_ptr->setIsAudioOnly(true);
            break;
        case 3: // 提取音频 AAC
            formatCombo->setCurrentIndex(5); // aac
            videoCodecCombo->setCurrentIndex(3); // none
            audioCodecCombo->setCurrentIndex(0); // aac
            audioBitrateCombo->setCurrentIndex(0); // 320k
            q_ptr->setIsAudioOnly(true);
            break;
        case 4: // Apple MOV
            formatCombo->setCurrentIndex(2); // mov
            videoCodecCombo->setCurrentIndex(0); // h264
            audioCodecCombo->setCurrentIndex(0); // aac
            q_ptr->setIsAudioOnly(false);
            break;
        default: // 自定义
            break;
        }
        notifyChange();
    }

    void notifyChange() {
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

    d->videoSettingsGroup->setVisible(!audioOnly);

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

    setIsAudioOnly(cfg.videoCodec == VideoCodecType::None);
}

} // namespace ffmpeg_transform
