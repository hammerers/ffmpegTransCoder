#include "VideoCompareWidget.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QFontDatabase>
#include <QPainterPath>
#include <algorithm>

namespace ffmpeg_transform {

class VideoCompareWidgetPrivate {
public:
    VideoCompareWidget *q_ptr{nullptr};

    CompareMode mode{CompareMode::SideBySide};
    float splitPos{0.5f}; // 0.0 ~ 1.0 卷帘位置
    bool isDraggingSplitter{false};

    QImage originFrame;
    QImage processedFrame;
    double currentPts{0.0};
    int frameCount{0};

    bool delogoEnabled{false};
    QRect delogoRect;

    QString formatPts(double sec) const {
        int total = static_cast<int>(sec);
        int h = total / 3600;
        int m = (total % 3600) / 60;
        int s = total % 60;
        int ms = static_cast<int>((sec - total) * 1000.0);
        return QString("%1:%2:%3.%4")
            .arg(h, 2, 10, QChar('0'))
            .arg(m, 2, 10, QChar('0'))
            .arg(s, 2, 10, QChar('0'))
            .arg(ms, 3, 10, QChar('0'));
    }

    void drawBadge(QPainter &p, const QRect &rect, const QString &text, const QColor &bgColor, const QColor &textColor) {
        p.save();
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(bgColor);
        p.drawRoundedRect(rect, 4, 4);

        p.setPen(textColor);
        QFont f = p.font();
        f.setPixelSize(11);
        f.setBold(true);
        p.setFont(f);
        p.drawText(rect, Qt::AlignCenter, text);
        p.restore();
    }
};

VideoCompareWidget::VideoCompareWidget(QWidget *parent)
    : QWidget(parent), d_ptr(std::make_unique<VideoCompareWidgetPrivate>()) {
    Q_D(VideoCompareWidget);
    d->q_ptr = this;

    setObjectName("videoCompareWidget");
    setMinimumSize(480, 270);
    setMouseTracking(true);
}

VideoCompareWidget::~VideoCompareWidget() = default;

CompareMode VideoCompareWidget::compareMode() const {
    return d_ptr->mode;
}

void VideoCompareWidget::setCompareMode(CompareMode mode) {
    Q_D(VideoCompareWidget);
    if (d->mode == mode) return;
    d->mode = mode;
    update();
    emit compareModeChanged(mode);
}

int VideoCompareWidget::compareModeInt() const {
    return static_cast<int>(d_ptr->mode);
}

void VideoCompareWidget::setCompareModeInt(int mode) {
    setCompareMode(static_cast<CompareMode>(mode));
}

float VideoCompareWidget::splitPosition() const {
    return d_ptr->splitPos;
}

void VideoCompareWidget::setSplitPosition(float pos) {
    Q_D(VideoCompareWidget);
    float clamped = std::clamp(pos, 0.05f, 0.95f);
    if (std::abs(d->splitPos - clamped) > 0.001f) {
        d->splitPos = clamped;
        update();
        emit splitPositionChanged(clamped);
    }
}

void VideoCompareWidget::setDelogoHighlight(bool enabled, const QRect &rect) {
    Q_D(VideoCompareWidget);
    d->delogoEnabled = enabled;
    d->delogoRect = rect;
    update();
}

void VideoCompareWidget::updateFrames(const QImage &origin, const QImage &processed, double ptsSec) {
    Q_D(VideoCompareWidget);
    d->originFrame = origin;
    d->processedFrame = processed;
    d->currentPts = ptsSec;
    d->frameCount++;
    update();
}

void VideoCompareWidget::resetToIdle() {
    Q_D(VideoCompareWidget);
    d->originFrame = QImage();
    d->processedFrame = QImage();
    d->currentPts = 0.0;
    d->frameCount = 0;
    update();
}

void VideoCompareWidget::paintEvent(QPaintEvent *) {
    Q_D(VideoCompareWidget);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    int w = width();
    int h = height();

    // 绘制视口底色
    p.fillRect(rect(), QColor("#0c0e14"));

    // 空态占位
    if (d->originFrame.isNull() && d->processedFrame.isNull()) {
        p.setPen(QColor("#2d3345"));
        p.drawRect(rect().adjusted(1, 1, -2, -2));

        p.setPen(QColor("#64748b"));
        QFont f = p.font();
        f.setPixelSize(14);
        f.setBold(true);
        p.setFont(f);
        p.drawText(rect().adjusted(0, -16, 0, -16), Qt::AlignCenter, "实时双分屏画面对比视口");

        f.setPixelSize(12);
        f.setBold(false);
        p.setFont(f);
        p.setPen(QColor("#475569"));
        p.drawText(rect().adjusted(0, 16, 0, 16), Qt::AlignCenter, "等待转码流水线启动... 启动后在此呈现零拷贝双流画面与滤镜实时对比");
        return;
    }

    if (d->mode == CompareMode::SideBySide) {
        // 左右并排模式
        int halfW = w / 2;
        QRect leftRect(0, 0, halfW - 2, h);
        QRect rightRect(halfW + 2, 0, halfW - 2, h);

        // 绘制原画 (左)
        if (!d->originFrame.isNull()) {
            QImage scaledOrigin = d->originFrame.scaled(leftRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            int ox = leftRect.x() + (leftRect.width() - scaledOrigin.width()) / 2;
            int oy = leftRect.y() + (leftRect.height() - scaledOrigin.height()) / 2;
            p.drawImage(ox, oy, scaledOrigin);
        }

        // 绘制处理后画面 (右)
        if (!d->processedFrame.isNull()) {
            QImage scaledProc = d->processedFrame.scaled(rightRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            int px = rightRect.x() + (rightRect.width() - scaledProc.width()) / 2;
            int py = rightRect.y() + (rightRect.height() - scaledProc.height()) / 2;
            p.drawImage(px, py, scaledProc);
        }

        // 中间分割线
        p.setPen(QPen(QColor("#22283a"), 2));
        p.drawLine(halfW, 0, halfW, h);

        // 徽标指示
        d->drawBadge(p, QRect(leftRect.x() + 10, 10, 80, 22), "原画输入", QColor(30, 41, 59, 210), QColor("#38bdf8"));
        d->drawBadge(p, QRect(rightRect.x() + 10, 10, 95, 22), "压制/滤镜输出", QColor(15, 60, 40, 210), QColor("#34d399"));

    } else if (d->mode == CompareMode::CurtainSplit) {
        // 卷帘分屏模式 (单个画面同屏切割对比)
        QRect fullRect = rect();
        QSize targetSize = fullRect.size();

        QImage scaledOrigin = d->originFrame.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QImage scaledProc = d->processedFrame.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        int ox = (w - scaledOrigin.width()) / 2;
        int oy = (h - scaledOrigin.height()) / 2;
        QRect imgRect(ox, oy, scaledOrigin.width(), scaledOrigin.height());

        int splitX = static_cast<int>(w * d->splitPos);

        // 绘制左侧原画部分
        p.save();
        p.setClipRect(0, 0, splitX, h);
        if (!scaledOrigin.isNull()) {
            p.drawImage(ox, oy, scaledOrigin);
        }
        p.restore();

        // 绘制右侧处理后画面部分
        p.save();
        p.setClipRect(splitX, 0, w - splitX, h);
        if (!scaledProc.isNull()) {
            p.drawImage(ox, oy, scaledProc);
        }
        p.restore();

        // 绘制卷帘分割竖线与控制手柄
        p.setPen(QPen(QColor("#38bdf8"), 2));
        p.drawLine(splitX, 0, splitX, h);

        // 手柄圆钮
        p.setBrush(QColor("#0284c7"));
        p.setPen(QPen(QColor("#ffffff"), 2));
        p.drawEllipse(QPoint(splitX, h / 2), 14, 14);

        p.setPen(QColor("#ffffff"));
        p.drawText(QRect(splitX - 14, h / 2 - 14, 28, 28), Qt::AlignCenter, "< >");

        // 左右浮动标识
        d->drawBadge(p, QRect(10, 10, 80, 22), "原画输入", QColor(15, 23, 42, 200), QColor("#38bdf8"));
        d->drawBadge(p, QRect(w - 105, 10, 95, 22), "压制/滤镜输出", QColor(15, 23, 42, 200), QColor("#34d399"));

    } else {
        // 仅处理后成品模式
        if (!d->processedFrame.isNull()) {
            QImage scaledProc = d->processedFrame.scaled(rect().size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            int px = (w - scaledProc.width()) / 2;
            int py = (h - scaledProc.height()) / 2;
            p.drawImage(px, py, scaledProc);
        }
        d->drawBadge(p, QRect(10, 10, 95, 22), "压制/滤镜输出", QColor(15, 23, 42, 200), QColor("#34d399"));
    }

    // 底部浮动 PTS 与信息条
    QString timeText = QString("PTS %1 | 渲染帧 %2").arg(d->formatPts(d->currentPts)).arg(d->frameCount);
    int pillW = 200;
    QRect pillRect(12, h - 34, pillW, 24);
    p.setBrush(QColor(15, 23, 42, 220));
    p.setPen(QPen(QColor("#334155"), 1));
    p.drawRoundedRect(pillRect, 4, 4);

    p.setPen(QColor("#94a3b8"));
    QFont f = p.font();
    f.setPixelSize(11);
    p.setFont(f);
    p.drawText(pillRect, Qt::AlignCenter, timeText);
}

void VideoCompareWidget::mousePressEvent(QMouseEvent *event) {
    Q_D(VideoCompareWidget);
    if (d->mode == CompareMode::CurtainSplit) {
        int splitX = static_cast<int>(width() * d->splitPos);
        if (std::abs(event->pos().x() - splitX) < 20) {
            d->isDraggingSplitter = true;
            setCursor(Qt::SplitHCursor);
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void VideoCompareWidget::mouseMoveEvent(QMouseEvent *event) {
    Q_D(VideoCompareWidget);
    if (d->mode == CompareMode::CurtainSplit) {
        int splitX = static_cast<int>(width() * d->splitPos);
        if (d->isDraggingSplitter) {
            float newPos = static_cast<float>(event->pos().x()) / static_cast<float>(width());
            setSplitPosition(newPos);
            return;
        } else if (std::abs(event->pos().x() - splitX) < 20) {
            setCursor(Qt::SplitHCursor);
            return;
        } else {
            setCursor(Qt::ArrowCursor);
        }
    } else {
        setCursor(Qt::ArrowCursor);
    }
    QWidget::mouseMoveEvent(event);
}

void VideoCompareWidget::mouseReleaseEvent(QMouseEvent *event) {
    Q_D(VideoCompareWidget);
    if (d->isDraggingSplitter) {
        d->isDraggingSplitter = false;
        setCursor(Qt::ArrowCursor);
    }
    QWidget::mouseReleaseEvent(event);
}

void VideoCompareWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    update();
}

} // namespace ffmpeg_transform
