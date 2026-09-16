#include "VideoPlayControlBar.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QComboBox>
#include <QMouseEvent>
#include <QStyle>
#include <algorithm>

namespace ffmpeg_transform {

class VideoPlayControlBarPrivate {
public:
    VideoPlayControlBar *q_ptr{nullptr};

    QPushButton *playPauseBtn{nullptr};
    QPushButton *stopBtn{nullptr};
    QSlider *slider{nullptr};
    QLabel *timeLabel{nullptr};
    QComboBox *speedCombo{nullptr};
    QLabel *modeBadge{nullptr};

    bool isPlaying{false};
    QString playbackMode{"idle"};
    double durationSec{0.0};
    double currentSec{0.0};
    bool isDraggingSlider{false};

    static QString formatTime(double sec) {
        if (sec < 0.0) sec = 0.0;
        int total = static_cast<int>(sec);
        int h = total / 3600;
        int m = (total % 3600) / 60;
        int s = total % 60;
        if (h > 0) {
            return QString("%1:%2:%3")
                .arg(h, 2, 10, QChar('0'))
                .arg(m, 2, 10, QChar('0'))
                .arg(s, 2, 10, QChar('0'));
        }
        return QString("%1:%2")
            .arg(m, 2, 10, QChar('0'))
            .arg(s, 2, 10, QChar('0'));
    }

    void initUI() {
        q_ptr->setObjectName("videoPlayControlBar");
        q_ptr->setFixedHeight(44);

        auto *layout = new QHBoxLayout(q_ptr);
        layout->setContentsMargins(12, 4, 12, 4);
        layout->setSpacing(10);

        // 播放 / 暂停按钮
        playPauseBtn = new QPushButton("播放", q_ptr);
        playPauseBtn->setObjectName("btnPlayPause");
        playPauseBtn->setMinimumWidth(64);
        layout->addWidget(playPauseBtn);

        // 复位停止按钮
        stopBtn = new QPushButton("复位", q_ptr);
        stopBtn->setObjectName("btnPlayStop");
        stopBtn->setMinimumWidth(52);
        layout->addWidget(stopBtn);

        // 进度滑动条
        slider = new QSlider(Qt::Horizontal, q_ptr);
        slider->setObjectName("playSlider");
        slider->setRange(0, 1000);
        slider->setValue(0);
        slider->installEventFilter(q_ptr);
        layout->addWidget(slider, 1);

        // 时间标签 (当前 / 总长)
        timeLabel = new QLabel("00:00 / 00:00", q_ptr);
        timeLabel->setObjectName("playTimeLabel");
        timeLabel->setMinimumWidth(110);
        timeLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(timeLabel);

        // 倍速下拉框
        speedCombo = new QComboBox(q_ptr);
        speedCombo->setObjectName("playSpeedCombo");
        speedCombo->addItem("0.5x", 0.5);
        speedCombo->addItem("1.0x", 1.0);
        speedCombo->addItem("1.25x", 1.25);
        speedCombo->addItem("1.5x", 1.5);
        speedCombo->addItem("2.0x", 2.0);
        speedCombo->setCurrentIndex(1); // 默认 1.0x
        speedCombo->setMinimumWidth(72);
        layout->addWidget(speedCombo);

        // 模式状态指示徽标
        modeBadge = new QLabel("未载入视频", q_ptr);
        modeBadge->setObjectName("playModeBadge");
        modeBadge->setProperty("mode", "idle");
        modeBadge->setAlignment(Qt::AlignCenter);
        modeBadge->setMinimumWidth(140);
        layout->addWidget(modeBadge);

        bindSignals();
        setControlEnabled(false);
    }

    void bindSignals() {
        QObject::connect(playPauseBtn, &QPushButton::clicked, [this]() {
            if (isPlaying) {
                emit q_ptr->pauseClicked();
            } else {
                emit q_ptr->playClicked();
            }
        });

        QObject::connect(stopBtn, &QPushButton::clicked, [this]() {
            emit q_ptr->stopClicked();
        });

        QObject::connect(slider, &QSlider::sliderPressed, [this]() {
            isDraggingSlider = true;
        });

        QObject::connect(slider, &QSlider::sliderMoved, [this](int val) {
            if (durationSec > 0.0) {
                double targetSec = static_cast<double>(val) / slider->maximum() * durationSec;
                timeLabel->setText(QString("%1 / %2").arg(formatTime(targetSec), formatTime(durationSec)));
            }
        });

        QObject::connect(slider, &QSlider::sliderReleased, [this]() {
            isDraggingSlider = false;
            if (durationSec > 0.0) {
                double targetSec = static_cast<double>(slider->value()) / slider->maximum() * durationSec;
                emit q_ptr->seekRequested(targetSec);
            }
        });

        QObject::connect(speedCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int idx) {
            double s = speedCombo->itemData(idx).toDouble();
            if (s > 0.0) {
                emit q_ptr->speedChanged(s);
            }
        });
    }

    void setControlEnabled(bool enabled) {
        playPauseBtn->setEnabled(enabled);
        stopBtn->setEnabled(enabled);
        slider->setEnabled(enabled);
        speedCombo->setEnabled(enabled);
    }
};

VideoPlayControlBar::VideoPlayControlBar(QWidget *parent)
    : QWidget(parent), d_ptr(std::make_unique<VideoPlayControlBarPrivate>()) {
    Q_D(VideoPlayControlBar);
    d->q_ptr = this;
    d->initUI();
}

VideoPlayControlBar::~VideoPlayControlBar() = default;

bool VideoPlayControlBar::isPlaying() const {
    return d_ptr->isPlaying;
}

void VideoPlayControlBar::setPlaying(bool playing) {
    Q_D(VideoPlayControlBar);
    if (d->isPlaying == playing) return;

    d->isPlaying = playing;
    d->playPauseBtn->setText(playing ? "暂停" : "播放");

    setProperty("isPlaying", playing);
    style()->unpolish(this);
    style()->polish(this);
    update();

    emit playingChanged(playing);
}

QString VideoPlayControlBar::playbackMode() const {
    return d_ptr->playbackMode;
}

void VideoPlayControlBar::setPlaybackMode(const QString &mode) {
    Q_D(VideoPlayControlBar);
    if (d->playbackMode == mode) return;

    d->playbackMode = mode;
    if (mode == "single") {
        d->modeBadge->setText("调参动态预览");
        d->modeBadge->setProperty("mode", "single");
    } else if (mode == "dual") {
        d->modeBadge->setText("原画 vs 成品同步对比");
        d->modeBadge->setProperty("mode", "dual");
    } else {
        d->modeBadge->setText("未载入视频");
        d->modeBadge->setProperty("mode", "idle");
    }

    d->modeBadge->style()->unpolish(d->modeBadge);
    d->modeBadge->style()->polish(d->modeBadge);
    d->modeBadge->update();

    setProperty("playbackMode", mode);
    style()->unpolish(this);
    style()->polish(this);
    update();

    emit playbackModeChanged(mode);
}

void VideoPlayControlBar::setModeDescription(const QString &text) {
    Q_D(VideoPlayControlBar);
    d->modeBadge->setText(text);
}

void VideoPlayControlBar::setDuration(double totalSec) {
    Q_D(VideoPlayControlBar);
    d->durationSec = totalSec;
    d->timeLabel->setText(QString("%1 / %2")
        .arg(VideoPlayControlBarPrivate::formatTime(d->currentSec), VideoPlayControlBarPrivate::formatTime(totalSec)));
    d->setControlEnabled(totalSec > 0.0);
}

void VideoPlayControlBar::setPosition(double currentSec, double totalSec) {
    Q_D(VideoPlayControlBar);
    d->currentSec = currentSec;
    d->durationSec = totalSec;

    if (!d->isDraggingSlider && totalSec > 0.0) {
        int val = static_cast<int>(std::clamp(currentSec / totalSec, 0.0, 1.0) * d->slider->maximum());
        d->slider->blockSignals(true);
        d->slider->setValue(val);
        d->slider->blockSignals(false);
    }

    d->timeLabel->setText(QString("%1 / %2")
        .arg(VideoPlayControlBarPrivate::formatTime(currentSec), VideoPlayControlBarPrivate::formatTime(totalSec)));
}

void VideoPlayControlBar::setControlEnabled(bool enabled) {
    d_ptr->setControlEnabled(enabled);
}

void VideoPlayControlBar::reset() {
    Q_D(VideoPlayControlBar);
    d->isPlaying = false;
    d->playPauseBtn->setText("播放");
    d->durationSec = 0.0;
    d->currentSec = 0.0;
    d->slider->blockSignals(true);
    d->slider->setValue(0);
    d->slider->blockSignals(false);
    d->timeLabel->setText("00:00 / 00:00");
    setPlaybackMode("idle");
    d->setControlEnabled(false);
}

bool VideoPlayControlBar::eventFilter(QObject *watched, QEvent *event) {
    Q_D(VideoPlayControlBar);
    if (watched == d->slider) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                int w = d->slider->width();
                if (w > 0 && d->durationSec > 0.0) {
                    int x = std::clamp(me->pos().x(), 0, w);
                    int val = static_cast<int>(static_cast<double>(x) / w * d->slider->maximum());
                    d->slider->setValue(val);
                    double targetSec = static_cast<double>(val) / d->slider->maximum() * d->durationSec;
                    emit seekRequested(targetSec);
                    return true;
                }
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace ffmpeg_transform
