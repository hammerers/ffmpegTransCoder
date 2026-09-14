#pragma once

#include <QWidget>
#include <memory>

namespace ffmpeg_transform {

class NavSidebarPrivate;

/**
 * @brief 工作台纵向多级导航侧边栏 (遵循 Pimpl 与动态样式规范)
 */
class NavSidebar : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentChanged)

public:
    explicit NavSidebar(QWidget *parent = nullptr);
    ~NavSidebar() override;

    int currentIndex() const;
    void setCurrentIndex(int index);

signals:
    void currentChanged(int index);

private:
    std::unique_ptr<NavSidebarPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(NavSidebar)
};

} // namespace ffmpeg_transform
