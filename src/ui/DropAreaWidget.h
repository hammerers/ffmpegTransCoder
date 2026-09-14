#pragma once

#include <QWidget>
#include <QStringList>
#include <memory>

namespace ffmpeg_transform {

class DropAreaWidgetPrivate;

/**
 * @brief 文件拖拽放置与快捷导入区域 (遵循 Pimpl 与动态样式规范)
 */
class DropAreaWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(bool isDragging READ isDragging WRITE setIsDragging NOTIFY draggingChanged)

public:
    explicit DropAreaWidget(QWidget *parent = nullptr);
    ~DropAreaWidget() override;

    bool isDragging() const;
    void setIsDragging(bool dragging);

signals:
    void filesDropped(const QStringList &filePaths);
    void fileSelected();
    void draggingChanged(bool dragging);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    std::unique_ptr<DropAreaWidgetPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(DropAreaWidget)
};

} // namespace ffmpeg_transform
