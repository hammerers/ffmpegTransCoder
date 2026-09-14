#pragma once

#include <QWidget>
#include <memory>

namespace ffmpeg_transform {

class HomePagePrivate;

/**
 * @brief 起始页面 / 仪表盘工作区 (遵循 Pimpl 与动态样式规范)
 */
class HomePage : public QWidget {
    Q_OBJECT

public:
    explicit HomePage(QWidget *parent = nullptr);
    ~HomePage() override;

signals:
    void navigateToQueue();
    void navigateToFilePrep();
    void navigateToParams();

private:
    std::unique_ptr<HomePagePrivate> const d_ptr;
    Q_DECLARE_PRIVATE(HomePage)
};

} // namespace ffmpeg_transform
