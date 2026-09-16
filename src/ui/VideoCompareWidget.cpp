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

    bool isStaticPreview{false};
    bool isCompleted{false};
    QString customStatus;

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

    void refreshProcessedFrame() {
        if (originFrame.isNull()) {
            processedFrame = QImage();
            return;
        }
        if (!isStaticPreview) return; // 实时流由引擎直接提供

        processedFrame = originFrame.copy();
        if (!delogoEnabled || delogoRect.isEmpty() || !delogoRect.isValid()) {
            return;
        }

        QRect r = delogoRect.intersected(processedFrame.rect());
        if (r.width() <= 2 || r.height() <= 2) return;

        int x0 = r.left(), y0 = r.top(), x1 = r.right(), y1 = r.bottom();
        int w = r.width(), h = r.height();
        for (int y = y0; y <= y1; ++y) {
            float ty = static_cast<float>(y - y0) / static_cast<float>(h > 1 ? h - 1 : 1);
            QRgb topCol = processedFrame.pixel(std::clamp(x0, 0, processedFrame.width() - 1), std::max(0, y0 - 1));
            QRgb botCol = processedFrame.pixel(std::clamp(x0, 0, processedFrame.width() - 1), std::min(processedFrame.height() - 1, y1 + 1));
            for (int x = x0; x <= x1; ++x) {
                float tx = static_cast<float>(x - x0) / static_cast<float>(w > 1 ? w - 1 : 1);
                QRgb leftCol = processedFrame.pixel(std::max(0, x0 - 1), std::clamp(y, 0, processedFrame.height() - 1));
                QRgb rightCol = processedFrame.pixel(std::min(processedFrame.width() - 1, x1 + 1), std::clamp(y, 0, processedFrame.height() - 1));

                int rH = static_cast<int>((1.0f - tx) * qRed(leftCol) + tx * qRed(rightCol));
                int gH = static_cast<int>((1.0f - tx) * qGreen(leftCol) + tx * qGreen(rightCol));
                int bH = static_cast<int>((1.0f - tx) * qBlue(leftCol) + tx * qBlue(rightCol));

                int rV = static_cast<int>((1.0f - ty) * qRed(topCol) + ty * qRed(botCol));
                int gV = static_cast<int>((1.0f - ty) * qGreen(topCol) + ty * qGreen(botCol));
                int bV = static_cast<int>((1.0f - ty) * qBlue(topCol) + ty * qBlue(botCol));

                processedFrame.setPixel(x, y, qRgb((rH + rV) / 2, (gH + gV) / 2, (bH + bV) / 2));
            }
        }
    }

    void drawDelogoBox(QPainter &p, int ox, int oy, int scaledW, int scaledH, const QSize &origSize) {
        if (!delogoEnabled || delogoRect.isEmpty() || origSize.width() <= 0 || origSize.height() <= 0) return;

        float sx = static_cast<float>(scaledW) / static_cast<float>(origSize.width());
        float sy = static_cast<float>(scaledH) / static_cast<float>(origSize.height());

        int rx = ox + static_cast<int>(delogoRect.x() * sx);
        int ry = oy + static_cast<int>(delogoRect.y() * sy);
        int rw = static_cast<int>(delogoRect.width() * sx);
        int rh = static_cast<int>(delogoRect.height() * sy);

        QRect box(rx, ry, rw, rh);

        p.save();
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QPen(QColor("#f43f5e"), 2, Qt::DashLine));
        p.setBrush(QColor(244, 63, 94, 40));
        p.drawRect(box);

        // 绘制四角加粗标记
        int cornerLen = std::min(8, std::min(rw / 3, rh / 3));
        p.setPen(QPen(QColor("#ffffff"), 2, Qt::SolidLine));
        p.drawLine(rx, ry, rx + cornerLen, ry);
        p.drawLine(rx, ry, rx, ry + cornerLen);
        p.drawLine(rx + rw, ry, rx + rw - cornerLen, ry);
        p.drawLine(rx + rw, ry, rx + rw, ry + cornerLen);
        p.drawLine(rx, ry + rh, rx + cornerLen, ry + rh);
        p.drawLine(rx, ry + rh, rx, ry + rh - cornerLen);
        p.drawLine(rx + rw, ry + rh, rx + rw - cornerLen, ry + rh);
        p.drawLine(rx + rw, ry + rh, rx + rw, ry + rh - cornerLen);

        QString label = QString("去水印选区: %1,%2 %3x%4").arg(delogoRect.x()).arg(delogoRect.y()).arg(delogoRect.width()).arg(delogoRect.height());
        QRect textRect(rx, std::max(0, ry - 18), 160, 16);
        p.fillRect(textRect, QColor(15, 23, 42, 220));
        p.setPen(QColor("#f43f5e"));
        QFont f = p.font();
        f.setPixelSize(10);
        f.setBold(true);
        p.setFont(f);
        p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, label);
        p.restore();
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
    if (d->isStaticPreview) {
        d->refreshProcessedFrame();
    }
    update();
}

bool VideoCompareWidget::hasFrames() const {
    return !d_ptr->originFrame.isNull() || !d_ptr->processedFrame.isNull();
}

void VideoCompareWidget::setStaticPreview(const QImage &origin, const QString &taskName) {
    Q_D(VideoCompareWidget);
    d->originFrame = origin;
    d->isStaticPreview = true;
    d->isCompleted = false;
    d->currentPts = 0.0;
    d->customStatus = taskName;
    d->refreshProcessedFrame();
    update();
}

void VideoCompareWidget::setCompleted(bool completed, const QString &summaryText) {
    Q_D(VideoCompareWidget);
    d->isCompleted = completed;
    d->isStaticPreview = false;
    d->customStatus = summaryText;
    update();
}

void VideoCompareWidget::updateFrames(const QImage &origin, const QImage &processed, double ptsSec) {
    Q_D(VideoCompareWidget);
    d->originFrame = origin;
    d->processedFrame = processed;
    d->currentPts = ptsSec;
    d->isStaticPreview = false;
    d->isCompleted = false;
    d->frameCount++;
    update();
}

void VideoCompareWidget::resetToIdle() {
    Q_D(VideoCompareWidget);
    d->originFrame = QImage();
    d->processedFrame = QImage();
    d->currentPts = 0.0;
    d->frameCount = 0;
    d->isStaticPreview = false;
    d->isCompleted = false;
    d->customStatus.clear();
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

        p.setPen(QColor("#38bdf8"));
        QFont f = p.font();
        f.setPixelSize(15);
        f.setBold(true);
        p.setFont(f);
        p.drawText(rect().adjusted(0, -26, 0, -26), Qt::AlignCenter, "实时双分屏画面对比视口");

        f.setPixelSize(12);
        f.setBold(false);
        p.setFont(f);
        p.setPen(QColor("#94a3b8"));
        p.drawText(rect().adjusted(0, 4, 0, 4), Qt::AlignCenter, "队列已有任务时自动加载预览 | 点击右上角 [开始转码检视] 启动实时动态对比");

        p.setPen(QColor("#64748b"));
        f.setPixelSize(11);
        p.setFont(f);
        p.drawText(rect().adjusted(0, 30, 0, 30), Qt::AlignCenter, "支持原画 vs 压制输出左右并排、无级卷帘拖拽与内存去水印实时调参");
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

            // 在原画侧绘制去水印 ROI 选区框
            d->drawDelogoBox(p, ox, oy, scaledOrigin.width(), scaledOrigin.height(), d->originFrame.size());
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
        QString rText = d->delogoEnabled ? "去水印/压制输出" : "压制输出";
        d->drawBadge(p, QRect(rightRect.x() + 10, 10, 110, 22), rText, QColor(15, 60, 40, 210), QColor("#34d399"));

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
            d->drawDelogoBox(p, ox, oy, scaledOrigin.width(), scaledOrigin.height(), d->originFrame.size());
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
        QString rText = d->delogoEnabled ? "去水印/压制输出" : "压制输出";
        d->drawBadge(p, QRect(w - 120, 10, 110, 22), rText, QColor(15, 23, 42, 200), QColor("#34d399"));

    } else {
        // 仅处理后成品模式
        if (!d->processedFrame.isNull()) {
            QImage scaledProc = d->processedFrame.scaled(rect().size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            int px = (w - scaledProc.width()) / 2;
            int py = (h - scaledProc.height()) / 2;
            p.drawImage(px, py, scaledProc);
        }
        QString rText = d->delogoEnabled ? "去水印/压制输出" : "压制输出";
        d->drawBadge(p, QRect(10, 10, 110, 22), rText, QColor(15, 23, 42, 200), QColor("#34d399"));
    }

    // 底部浮动状态与信息条
    QString timeText;
    if (d->isCompleted) {
        timeText = QString("转码已完成 | 共压制 %1 帧 | 支持拖拽卷帘对比终版画质").arg(d->frameCount);
    } else if (d->isStaticPreview) {
        timeText = QString("待命调参预览 | 选区已对齐 | 拖拽卷帘对比去水印平滑效果");
    } else {
        timeText = QString("PTS %1 | 渲染帧 %2").arg(d->formatPts(d->currentPts)).arg(d->frameCount);
    }

    int pillW = std::max(220, static_cast<int>(timeText.length() * 7 + 30));
    QRect pillRect(12, h - 34, pillW, 24);
    p.setBrush(QColor(15, 23, 42, 220));
    p.setPen(QPen(d->isCompleted ? QColor("#059669") : (d->isStaticPreview ? QColor("#0284c7") : QColor("#334155")), 1));
    p.drawRoundedRect(pillRect, 4, 4);

    p.setPen(d->isCompleted ? QColor("#34d399") : (d->isStaticPreview ? QColor("#38bdf8") : QColor("#94a3b8")));
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
