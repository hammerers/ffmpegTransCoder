#include "NavSidebar.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QFrame>
#include <QLabel>
#include <QVector>
#include <QStyle>

namespace ffmpeg_transform {

struct NavItemData {
    int index;
    QString title;
};

class NavSidebarPrivate {
public:
    NavSidebar *q_ptr{nullptr};
    QLineEdit *searchEdit{nullptr};
    QVector<QPushButton*> navButtons;
    int currentIndex{0};

    void initUI() {
        q_ptr->setObjectName("navSidebar");
        q_ptr->setFixedWidth(190);

        auto *mainLayout = new QVBoxLayout(q_ptr);
        mainLayout->setContentsMargins(10, 12, 10, 12);
        mainLayout->setSpacing(4);

        // 顶部搜索框
        searchEdit = new QLineEdit(q_ptr);
        searchEdit->setObjectName("navSearchEdit");
        searchEdit->setPlaceholderText("搜索选项卡标题");
        searchEdit->setClearButtonEnabled(true);
        mainLayout->addWidget(searchEdit);
        mainLayout->addSpacing(8);

        // 导航项分组生成器
        auto addSeparator = [this, mainLayout]() {
            auto *line = new QFrame(q_ptr);
            line->setObjectName("navSeparator");
            line->setFrameShape(QFrame::HLine);
            line->setFrameShadow(QFrame::Plain);
            mainLayout->addSpacing(6);
            mainLayout->addWidget(line);
            mainLayout->addSpacing(6);
        };

        auto createBtn = [this, mainLayout](int index, const QString &text) {
            auto *btn = new QPushButton(text, q_ptr);
            btn->setObjectName("navItemBtn");
            btn->setCheckable(true);
            btn->setCursor(Qt::PointingHandCursor);
            navButtons.append(btn);
            mainLayout->addWidget(btn);

            QObject::connect(btn, &QPushButton::clicked, [this, index]() {
                q_ptr->setCurrentIndex(index);
            });
        };

        // Group 1: 核心页面
        createBtn(0, "起始页面");
        createBtn(1, "编码队列");

        addSeparator();

        // Group 2: 转码工作流与媒体信息
        createBtn(2, "准备文件");
        createBtn(3, "参数面板");
        createBtn(4, "媒体信息");
        createBtn(5, "实时检视");

        mainLayout->addStretch();

        // 搜索过滤联动
        QObject::connect(searchEdit, &QLineEdit::textChanged, [this](const QString &text) {
            QString query = text.trimmed();
            for (auto *btn : navButtons) {
                bool match = query.isEmpty() || btn->text().contains(query, Qt::CaseInsensitive);
                btn->setVisible(match);
            }
        });

        updateActiveState();
    }

    void updateActiveState() {
        for (int i = 0; i < navButtons.size(); ++i) {
            bool active = (i == currentIndex);
            navButtons[i]->setChecked(active);
            navButtons[i]->setProperty("isActive", active);
            navButtons[i]->style()->unpolish(navButtons[i]);
            navButtons[i]->style()->polish(navButtons[i]);
            navButtons[i]->update();
        }
    }
};

NavSidebar::NavSidebar(QWidget *parent)
    : QWidget(parent), d_ptr(std::make_unique<NavSidebarPrivate>()) {
    Q_D(NavSidebar);
    d->q_ptr = this;
    d->initUI();
}

NavSidebar::~NavSidebar() = default;

int NavSidebar::currentIndex() const {
    return d_ptr->currentIndex;
}

void NavSidebar::setCurrentIndex(int index) {
    Q_D(NavSidebar);
    if (index < 0 || index >= d->navButtons.size()) return;
    if (d->currentIndex == index) return;
    d->currentIndex = index;
    d->updateActiveState();
    emit currentChanged(index);
}

} // namespace ffmpeg_transform
