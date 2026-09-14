#pragma once

#include <QMainWindow>
#include <memory>

namespace ffmpeg_transform {

class MainWindowPrivate;

/**
 * @brief 格式转换器主窗口 (遵循 Pimpl 与动态样式规范)
 */
class MainWindow : public QMainWindow {
    Q_OBJECT
    Q_PROPERTY(bool hasTasks READ hasTasks WRITE setHasTasks NOTIFY tasksStateChanged)

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    bool hasTasks() const;
    void setHasTasks(bool hasTasks);

signals:
    void tasksStateChanged(bool hasTasks);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    std::unique_ptr<MainWindowPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(MainWindow)
};

} // namespace ffmpeg_transform
