#include "MediaInspectorPage.h"
#include "MediaInfoCard.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>

namespace ffmpeg_transform {

class MediaInspectorPagePrivate {
public:
    MediaInspectorPage *q_ptr{nullptr};
    MediaInfoCard *infoCard{nullptr};

    void initUI() {
        q_ptr->setObjectName("mediaInspectorPage");

        auto *layout = new QVBoxLayout(q_ptr);
        layout->setContentsMargins(20, 16, 20, 16);
        layout->setSpacing(12);

        auto *title = new QLabel("媒体信息深度解析 (Native ffprobe C API)", q_ptr);
        title->setObjectName("inspectorPageTitle");
        layout->addWidget(title);

        infoCard = new MediaInfoCard(q_ptr);
        layout->addWidget(infoCard, 1);
    }
};

MediaInspectorPage::MediaInspectorPage(QWidget *parent)
    : QWidget(parent), d_ptr(std::make_unique<MediaInspectorPagePrivate>()) {
    Q_D(MediaInspectorPage);
    d->q_ptr = this;
    d->initUI();
}

MediaInspectorPage::~MediaInspectorPage() = default;

void MediaInspectorPage::setMedia(const MediaInfo &info, const QImage &thumbnail) {
    d_ptr->infoCard->setMedia(info, thumbnail);
}

void MediaInspectorPage::clearMedia() {
    d_ptr->infoCard->clearMedia();
}

} // namespace ffmpeg_transform
