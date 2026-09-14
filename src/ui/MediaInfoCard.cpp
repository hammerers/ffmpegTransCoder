#include "MediaInfoCard.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QStyle>
#include <QPixmap>
#include <QStackedLayout>

namespace ffmpeg_transform {

class MediaInfoCardPrivate {
public:
    MediaInfoCard *q_ptr{nullptr};

    QStackedLayout *stackedLayout{nullptr};
    QWidget *placeholderWidget{nullptr};
    QWidget *contentWidget{nullptr};

    QLabel *thumbLabel{nullptr};
    QLabel *fileNameLabel{nullptr};
    QLabel *filePathLabel{nullptr};

    // 各种信息标签
    QLabel *resValLabel{nullptr};
    QLabel *vCodecValLabel{nullptr};
    QLabel *aCodecValLabel{nullptr};
    QLabel *fpsValLabel{nullptr};
    QLabel *durationValLabel{nullptr};
    QLabel *sizeValLabel{nullptr};
    QLabel *bitrateValLabel{nullptr};

    bool hasMedia{false};

    void initUI() {
        stackedLayout = new QStackedLayout(q_ptr);

        // 占位视图
        placeholderWidget = new QWidget(q_ptr);
        auto *phLayout = new QVBoxLayout(placeholderWidget);
        phLayout->setAlignment(Qt::AlignCenter);
        auto *phLabel = new QLabel("👈 请在左侧选择视频以查看详细信息与设置参数", placeholderWidget);
        phLabel->setObjectName("cardPlaceholderLabel");
        phLabel->setAlignment(Qt::AlignCenter);
        phLayout->addWidget(phLabel);

        // 内容视图
        contentWidget = new QWidget(q_ptr);
        auto *mainLayout = new QVBoxLayout(contentWidget);
        mainLayout->setContentsMargins(12, 12, 12, 12);
        mainLayout->setSpacing(10);

        // 缩略图展示区
        thumbLabel = new QLabel(contentWidget);
        thumbLabel->setObjectName("thumbLabel");
        thumbLabel->setAlignment(Qt::AlignCenter);
        thumbLabel->setFixedHeight(170);
        thumbLabel->setScaledContents(false);

        // 文件名
        fileNameLabel = new QLabel(contentWidget);
        fileNameLabel->setObjectName("fileNameLabel");
        fileNameLabel->setWordWrap(true);

        filePathLabel = new QLabel(contentWidget);
        filePathLabel->setObjectName("filePathLabel");
        filePathLabel->setWordWrap(true);

        // 参数网格表
        auto *grid = new QGridLayout();
        grid->setHorizontalSpacing(12);
        grid->setVerticalSpacing(6);

        auto addRow = [grid](int row, const QString &title, QLabel* &valLabel, QWidget *parent) {
            auto *tLabel = new QLabel(title, parent);
            tLabel->setObjectName("metaTitleLabel");
            valLabel = new QLabel("-", parent);
            valLabel->setObjectName("metaValueLabel");
            grid->addWidget(tLabel, row, 0);
            grid->addWidget(valLabel, row, 1);
        };

        addRow(0, "分辨率", resValLabel, contentWidget);
        addRow(1, "视频编码", vCodecValLabel, contentWidget);
        addRow(2, "音频编码", aCodecValLabel, contentWidget);
        addRow(3, "视频帧率", fpsValLabel, contentWidget);
        addRow(4, "总时长", durationValLabel, contentWidget);
        addRow(5, "文件大小", sizeValLabel, contentWidget);
        addRow(6, "总码率", bitrateValLabel, contentWidget);

        mainLayout->addWidget(thumbLabel);
        mainLayout->addWidget(fileNameLabel);
        mainLayout->addWidget(filePathLabel);
        mainLayout->addSpacing(4);
        mainLayout->addLayout(grid);
        mainLayout->addStretch();

        stackedLayout->addWidget(placeholderWidget);
        stackedLayout->addWidget(contentWidget);
        stackedLayout->setCurrentWidget(placeholderWidget);
    }

    void updateInfo(const MediaInfo &info, const QImage &thumbnail) {
        if (!thumbnail.isNull()) {
            QPixmap pix = QPixmap::fromImage(thumbnail);
            thumbLabel->setPixmap(pix.scaled(thumbLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            thumbLabel->setText("🎬 暂无缩略图");
        }

        fileNameLabel->setText(info.fileName);
        filePathLabel->setText(info.filePath);

        resValLabel->setText(info.resolutionString());

        if (const auto *v = info.primaryVideo()) {
            vCodecValLabel->setText(v->codecName.toUpper() + (!v->pixelFormat.isEmpty() ? (" (" + v->pixelFormat + ")") : ""));
            fpsValLabel->setText(QString::asprintf("%.2f fps", v->fps));
        } else {
            vCodecValLabel->setText("无视频流");
            fpsValLabel->setText("-");
        }

        if (const auto *a = info.primaryAudio()) {
            aCodecValLabel->setText(QString("%1 (%2Hz, %3)").arg(a->codecName.toUpper()).arg(a->sampleRate).arg(a->channelLayout));
        } else {
            aCodecValLabel->setText("无音频流");
        }

        durationValLabel->setText(info.formattedDuration());
        sizeValLabel->setText(info.formattedFileSize());
        bitrateValLabel->setText(info.overallBitrate > 0 ? QString("%1 kbps").arg(info.overallBitrate / 1000) : "-");
    }
};

MediaInfoCard::MediaInfoCard(QWidget *parent)
    : QWidget(parent), d_ptr(std::make_unique<MediaInfoCardPrivate>()) {
    Q_D(MediaInfoCard);
    d->q_ptr = this;
    d->initUI();
}

MediaInfoCard::~MediaInfoCard() = default;

bool MediaInfoCard::hasMedia() const {
    return d_ptr->hasMedia;
}

void MediaInfoCard::setHasMedia(bool hasMedia) {
    Q_D(MediaInfoCard);
    if (d->hasMedia == hasMedia) return;
    d->hasMedia = hasMedia;

    d->stackedLayout->setCurrentWidget(hasMedia ? d->contentWidget : d->placeholderWidget);

    style()->unpolish(this);
    style()->polish(this);
    update();

    emit mediaStateChanged(hasMedia);
}

void MediaInfoCard::setMedia(const MediaInfo &info, const QImage &thumbnail) {
    Q_D(MediaInfoCard);
    d->updateInfo(info, thumbnail);
    setHasMedia(true);
}

void MediaInfoCard::clearMedia() {
    setHasMedia(false);
}

} // namespace ffmpeg_transform
