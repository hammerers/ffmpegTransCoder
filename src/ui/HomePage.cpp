#include "HomePage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QScrollArea>

namespace ffmpeg_transform {

class HomePagePrivate {
public:
    HomePage *q_ptr{nullptr};

    void initUI() {
        q_ptr->setObjectName("homePage");

        auto *scroll = new QScrollArea(q_ptr);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);

        auto *container = new QWidget(scroll);
        container->setObjectName("homeContainer");
        auto *mainLayout = new QVBoxLayout(container);
        mainLayout->setContentsMargins(24, 20, 24, 20);
        mainLayout->setSpacing(20);

        // 三列矩阵信息卡片 (Three Columns)
        auto *columnsLayout = new QHBoxLayout();
        columnsLayout->setSpacing(16);

        auto createCard = [container](const QString &headerText, const QStringList &items, const QString &actionText = "", int actionType = 0, HomePage *parent = nullptr) -> QWidget* {
            auto *card = new QWidget(container);
            card->setObjectName("homeCard");
            auto *cLayout = new QVBoxLayout(card);
            cLayout->setContentsMargins(18, 16, 18, 16);
            cLayout->setSpacing(12);

            auto *hLabel = new QLabel(headerText, card);
            hLabel->setObjectName("homeCardHeader");
            cLayout->addWidget(hLabel);

            for (const auto &item : items) {
                auto *iLabel = new QLabel("• " + item, card);
                iLabel->setObjectName("homeCardItem");
                iLabel->setWordWrap(true);
                cLayout->addWidget(iLabel);
            }

            cLayout->addStretch();

            if (!actionText.isEmpty() && parent) {
                auto *actBtn = new QPushButton(actionText, card);
                actBtn->setObjectName("homeCardActionBtn");
                actBtn->setCursor(Qt::PointingHandCursor);
                cLayout->addWidget(actBtn);

                if (actionType == 1) {
                    QObject::connect(actBtn, &QPushButton::clicked, parent, &HomePage::navigateToQueue);
                } else if (actionType == 2) {
                    QObject::connect(actBtn, &QPushButton::clicked, parent, &HomePage::navigateToParams);
                } else if (actionType == 3) {
                    QObject::connect(actBtn, &QPushButton::clicked, parent, &HomePage::navigateToFilePrep);
                }
            }

            return card;
        };

        QStringList archItems = {
            "全链路音视频流水线直调，严禁伪管道封装",
            "解复用 -> 解码 -> 缩放 -> 重采样 -> FIFO缓冲 -> 编码复用",
            "严格遵循 Pimpl 设计模式，组件头文件无私有成员泄露",
            "QSS 动态样式彻底解耦，属性驱动状态渲染，零硬编码",
            "独家实时等效 FFmpeg 核心参数生成器，参数透明可复制"
        };
        auto *card1 = createCard("架构与设计哲学", archItems, "查看参数调优 ->", 2, q_ptr);

        QStringList specItems = {
            "视频编码器: H.264 (libx264), H.265 (libx265), Stream Copy",
            "音频编码器: AAC, MP3, PCM",
            "封装容器: MP4, MKV, MOV, AVI, MP3, AAC",
            "画质控制: CRF 视觉无损因子 (18-35) 与智能码率模式",
            "硬件加速接口: DXVA2 / D3D11VA / NVENC 预留 Ready"
        };
        auto *card2 = createCard("转码引擎状态与规格", specItems, "批量准备文件 ->", 3, q_ptr);

        QStringList guideItems = {
            "可直接将一个或多个视频拖入【编码队列】快速加入",
            "对于海量文件或目录树，可前往【准备文件】集中批处理",
            "在【参数面板】微调 CRF、分辨率、帧率与音频参数",
            "队列中实时显示 FPS、倍速 (15x~120x)、ETA 与完成状态",
            "转码完成后支持一键在系统资源管理器中高亮定位输出"
        };
        auto *card3 = createCard("操作指南与提示", guideItems, "前往编码队列 ->", 1, q_ptr);

        columnsLayout->addWidget(card1, 1);
        columnsLayout->addWidget(card2, 1);
        columnsLayout->addWidget(card3, 1);
        mainLayout->addLayout(columnsLayout, 1);

        scroll->setWidget(container);

        auto *rootLayout = new QVBoxLayout(q_ptr);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->addWidget(scroll);
    }
};

HomePage::HomePage(QWidget *parent)
    : QWidget(parent), d_ptr(std::make_unique<HomePagePrivate>()) {
    Q_D(HomePage);
    d->q_ptr = this;
    d->initUI();
}

HomePage::~HomePage() = default;

} // namespace ffmpeg_transform
