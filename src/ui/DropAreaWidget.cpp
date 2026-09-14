#include "DropAreaWidget.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QStyle>
#include <QCursor>

namespace ffmpeg_transform {

class DropAreaWidgetPrivate {
public:
    DropAreaWidget *q_ptr{nullptr};
    QLabel *iconLabel{nullptr};
    QLabel *titleLabel{nullptr};
    QLabel *hintLabel{nullptr};
    QPushButton *browseBtn{nullptr};
    bool isDragging{false};

    void initUI() {
        q_ptr->setAcceptDrops(true);
        q_ptr->setCursor(Qt::PointingHandCursor);

        auto *layout = new QVBoxLayout(q_ptr);
        layout->setContentsMargins(20, 24, 20, 24);
        layout->setSpacing(8);
        layout->setAlignment(Qt::AlignCenter);

        iconLabel = new QLabel("🎬", q_ptr);
        iconLabel->setObjectName("dropIconLabel");
        iconLabel->setAlignment(Qt::AlignCenter);

        titleLabel = new QLabel("拖拽视频到此处，或点击添加", q_ptr);
        titleLabel->setObjectName("dropTitleLabel");
        titleLabel->setAlignment(Qt::AlignCenter);

        hintLabel = new QLabel("支持 MP4, MKV, MOV, AVI, FLV, TS, WebM 等主流视频格式", q_ptr);
        hintLabel->setObjectName("dropHintLabel");
        hintLabel->setAlignment(Qt::AlignCenter);

        browseBtn = new QPushButton("选择视频文件", q_ptr);
        browseBtn->setObjectName("dropBrowseBtn");
        browseBtn->setCursor(Qt::PointingHandCursor);

        QObject::connect(browseBtn, &QPushButton::clicked, [this]() {
            emit q_ptr->fileSelected();
        });

        layout->addWidget(iconLabel);
        layout->addWidget(titleLabel);
        layout->addWidget(hintLabel);
        layout->addSpacing(8);
        layout->addWidget(browseBtn, 0, Qt::AlignCenter);
    }
};

DropAreaWidget::DropAreaWidget(QWidget *parent)
    : QWidget(parent), d_ptr(std::make_unique<DropAreaWidgetPrivate>()) {
    Q_D(DropAreaWidget);
    d->q_ptr = this;
    d->initUI();
}

DropAreaWidget::~DropAreaWidget() = default;

bool DropAreaWidget::isDragging() const {
    return d_ptr->isDragging;
}

void DropAreaWidget::setIsDragging(bool dragging) {
    Q_D(DropAreaWidget);
    if (d->isDragging == dragging) return;
    d->isDragging = dragging;

    style()->unpolish(this);
    style()->polish(this);
    update();

    emit draggingChanged(dragging);
}

void DropAreaWidget::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        setIsDragging(true);
    }
}

void DropAreaWidget::dragLeaveEvent(QDragLeaveEvent *event) {
    Q_UNUSED(event);
    setIsDragging(false);
}

void DropAreaWidget::dragMoveEvent(QDragMoveEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void DropAreaWidget::dropEvent(QDropEvent *event) {
    setIsDragging(false);
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty()) return;

    QStringList files;
    for (const auto &url : urls) {
        if (url.isLocalFile()) {
            files.append(url.toLocalFile());
        }
    }

    if (!files.isEmpty()) {
        emit filesDropped(files);
        event->acceptProposedAction();
    }
}

void DropAreaWidget::mousePressEvent(QMouseEvent *event) {
    emit fileSelected();
    QWidget::mousePressEvent(event);
}

} // namespace ffmpeg_transform
