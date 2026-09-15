#include "FilePrepPage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QFileDialog>
#include <QFileInfo>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>

namespace ffmpeg_transform {

class FilePrepPagePrivate {
public:
    FilePrepPage *q_ptr{nullptr};

    QPushButton *enqueueBtn{nullptr};
    QPushButton *addFileBtn{nullptr};
    QPushButton *addFolderBtn{nullptr};
    QPushButton *removeSelectedBtn{nullptr};
    QPushButton *removeAllBtn{nullptr};
    QComboBox *sortCombo{nullptr};

    QTableWidget *fileTable{nullptr};
    QLabel *bottomHintLabel{nullptr};

    void initUI() {
        q_ptr->setObjectName("filePrepPage");
        q_ptr->setAcceptDrops(true);

        auto *mainLayout = new QVBoxLayout(q_ptr);
        mainLayout->setContentsMargins(16, 12, 16, 12);
        mainLayout->setSpacing(10);

        // 1. 顶部操作栏
        auto *actionBar = new QWidget(q_ptr);
        actionBar->setObjectName("prepActionBar");
        auto *actionLayout = new QHBoxLayout(actionBar);
        actionLayout->setContentsMargins(0, 4, 0, 8);
        actionLayout->setSpacing(8);

        enqueueBtn = new QPushButton("加入编码队列", actionBar);
        enqueueBtn->setObjectName("btnPrimaryPrep");
        enqueueBtn->setCursor(Qt::PointingHandCursor);

        addFileBtn = new QPushButton("添加文件", actionBar);
        addFileBtn->setCursor(Qt::PointingHandCursor);

        addFolderBtn = new QPushButton("添加文件夹及子目录", actionBar);
        addFolderBtn->setCursor(Qt::PointingHandCursor);

        removeSelectedBtn = new QPushButton("移除选中", actionBar);
        removeSelectedBtn->setCursor(Qt::PointingHandCursor);

        removeAllBtn = new QPushButton("移除全部", actionBar);
        removeAllBtn->setCursor(Qt::PointingHandCursor);

        sortCombo = new QComboBox(actionBar);
        sortCombo->setObjectName("prepSortCombo");
        sortCombo->addItem("排序: 默认顺序");
        sortCombo->addItem("按文件名排序");
        sortCombo->addItem("按文件大小排序");

        actionLayout->addWidget(enqueueBtn);
        actionLayout->addWidget(addFileBtn);
        actionLayout->addWidget(addFolderBtn);
        actionLayout->addWidget(removeSelectedBtn);
        actionLayout->addWidget(removeAllBtn);
        actionLayout->addWidget(sortCombo);
        actionLayout->addStretch();

        mainLayout->addWidget(actionBar);

        // 2. 文件列表表格
        fileTable = new QTableWidget(0, 4, q_ptr);
        fileTable->setObjectName("prepFileTable");
        fileTable->setHorizontalHeaderLabels({"文件名", "路径", "输出后缀", "大小"});
        fileTable->horizontalHeader()->setStretchLastSection(false);
        fileTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        fileTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        fileTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        fileTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        fileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        fileTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
        fileTable->setShowGrid(false);
        fileTable->setAlternatingRowColors(true);
        fileTable->verticalHeader()->setVisible(false);

        mainLayout->addWidget(fileTable, 1);

        // 3. 底部提示栏 (与截图 3 完全一致)
        bottomHintLabel = new QLabel("可以直接把文件拖进编码队列来开始，如果文件很多或者有其他需求再用这个页面", q_ptr);
        bottomHintLabel->setObjectName("prepBottomHint");
        mainLayout->addWidget(bottomHintLabel);

        bindActions();
    }

    void bindActions() {
        QObject::connect(addFileBtn, &QPushButton::clicked, [this]() {
            QStringList files = QFileDialog::getOpenFileNames(
                q_ptr, "选择媒体文件", "",
                "音视频文件 (*.mp4 *.mkv *.mov *.avi *.flv *.ts *.webm *.wmv *.mp3 *.aac);;所有文件 (*.*)"
            );
            if (!files.isEmpty()) {
                q_ptr->addFiles(files);
            }
        });

        QObject::connect(addFolderBtn, &QPushButton::clicked, [this]() {
            QString dir = QFileDialog::getExistingDirectory(q_ptr, "选择包含媒体文件的文件夹");
            if (!dir.isEmpty()) {
                QStringList filters = {"*.mp4", "*.mkv", "*.mov", "*.avi", "*.flv", "*.ts", "*.webm", "*.wmv", "*.mp3", "*.aac"};
                QDirIterator it(dir, filters, QDir::Files, QDirIterator::Subdirectories);
                QStringList foundFiles;
                while (it.hasNext()) {
                    foundFiles.append(it.next());
                }
                if (!foundFiles.isEmpty()) {
                    q_ptr->addFiles(foundFiles);
                }
            }
        });

        QObject::connect(removeSelectedBtn, &QPushButton::clicked, [this]() {
            auto sel = fileTable->selectionModel()->selectedRows();
            // 从后往前删避免索引错乱
            std::sort(sel.begin(), sel.end(), [](const QModelIndex &a, const QModelIndex &b) {
                return a.row() > b.row();
            });
            for (const auto &idx : sel) {
                fileTable->removeRow(idx.row());
            }
        });

        QObject::connect(removeAllBtn, &QPushButton::clicked, [this]() {
            fileTable->setRowCount(0);
        });

        QObject::connect(enqueueBtn, &QPushButton::clicked, [this]() {
            QStringList files = q_ptr->selectedFileList();
            if (files.isEmpty()) {
                files = q_ptr->fileList();
            }
            if (!files.isEmpty()) {
                emit q_ptr->enqueueFilesRequested(files);
            }
        });

        QObject::connect(sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int idx) {
            if (idx == 1) {
                fileTable->sortItems(0, Qt::AscendingOrder);
            } else if (idx == 2) {
                fileTable->sortItems(3, Qt::AscendingOrder);
            }
        });
    }

    void appendFileRow(const QString &filePath) {
        QFileInfo fi(filePath);
        if (!fi.exists()) return;

        // 去重
        for (int r = 0; r < fileTable->rowCount(); ++r) {
            if (fileTable->item(r, 1) && fileTable->item(r, 1)->text() == fi.absoluteFilePath()) {
                return;
            }
        }

        int row = fileTable->rowCount();
        fileTable->insertRow(row);

        auto *nameItem = new QTableWidgetItem(fi.fileName());
        auto *pathItem = new QTableWidgetItem(fi.absoluteFilePath());
        auto *extItem = new QTableWidgetItem("." + fi.suffix().toLower());

        // 计算大小
        qint64 bytes = fi.size();
        QString sizeStr;
        if (bytes < 1024 * 1024) {
            sizeStr = QString::asprintf("%.1f KB", bytes / 1024.0);
        } else if (bytes < 1024 * 1024 * 1024) {
            sizeStr = QString::asprintf("%.2f MB", bytes / (1024.0 * 1024.0));
        } else {
            sizeStr = QString::asprintf("%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
        }
        auto *sizeItem = new QTableWidgetItem(sizeStr);

        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        pathItem->setFlags(pathItem->flags() & ~Qt::ItemIsEditable);
        extItem->setFlags(extItem->flags() & ~Qt::ItemIsEditable);
        sizeItem->setFlags(sizeItem->flags() & ~Qt::ItemIsEditable);

        fileTable->setItem(row, 0, nameItem);
        fileTable->setItem(row, 1, pathItem);
        fileTable->setItem(row, 2, extItem);
        fileTable->setItem(row, 3, sizeItem);
    }
};

FilePrepPage::FilePrepPage(QWidget *parent)
    : QWidget(parent), d_ptr(std::make_unique<FilePrepPagePrivate>()) {
    Q_D(FilePrepPage);
    d->q_ptr = this;
    d->initUI();
}

FilePrepPage::~FilePrepPage() = default;

QStringList FilePrepPage::fileList() const {
    QStringList list;
    for (int r = 0; r < d_ptr->fileTable->rowCount(); ++r) {
        auto *item = d_ptr->fileTable->item(r, 1);
        if (item) {
            list.append(item->text());
        }
    }
    return list;
}

QStringList FilePrepPage::selectedFileList() const {
    QStringList list;
    auto sel = d_ptr->fileTable->selectionModel()->selectedRows();
    std::sort(sel.begin(), sel.end(), [](const QModelIndex &a, const QModelIndex &b) {
        return a.row() < b.row();
    });
    for (const auto &idx : sel) {
        auto *item = d_ptr->fileTable->item(idx.row(), 1);
        if (item) {
            list.append(item->text());
        }
    }
    return list;
}

void FilePrepPage::addFiles(const QStringList &files) {
    for (const auto &f : files) {
        d_ptr->appendFileRow(f);
    }
}

void FilePrepPage::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void FilePrepPage::dragMoveEvent(QDragMoveEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void FilePrepPage::dropEvent(QDropEvent *event) {
    QStringList files;
    for (const auto &u : event->mimeData()->urls()) {
        if (u.isLocalFile()) {
            files.append(u.toLocalFile());
        }
    }
    if (!files.isEmpty()) {
        addFiles(files);
    }
    event->acceptProposedAction();
}

} // namespace ffmpeg_transform
