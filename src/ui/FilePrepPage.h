#pragma once

#include <QWidget>
#include <QStringList>
#include <memory>

namespace ffmpeg_transform {

class FilePrepPagePrivate;

/**
 * @brief 准备文件 / 批量导入工作区 (遵循 Pimpl 与动态样式规范)
 */
class FilePrepPage : public QWidget {
    Q_OBJECT

public:
    explicit FilePrepPage(QWidget *parent = nullptr);
    ~FilePrepPage() override;

    QStringList fileList() const;
    void addFiles(const QStringList &files);

signals:
    void enqueueFilesRequested(const QStringList &files);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    std::unique_ptr<FilePrepPagePrivate> const d_ptr;
    Q_DECLARE_PRIVATE(FilePrepPage)
};

} // namespace ffmpeg_transform
