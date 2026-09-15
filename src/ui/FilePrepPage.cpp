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
#include <QDir>
#include <QDirIterator>
#include <QSet>
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
    QPushButton *refreshBtn{nullptr};
    QPushButton *removeSelectedBtn{nullptr};
    QPushButton *removeAllBtn{nullptr};
    QComboBox *sortCombo{nullptr};

    QTableWidget *fileTable{nullptr};
    QLabel *bottomHintLabel{nullptr};

    QSet<QString> trackedFolders;

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

        refreshBtn = new QPushButton("刷新", actionBar);
        refreshBtn->setCursor(Qt::PointingHandCursor);

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
        actionLayout->addWidget(refreshBtn);
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
                q_ptr->addFolder(dir);
            }
        });

        QObject::connect(refreshBtn, &QPushButton::clicked, [this]() {
            q_ptr->refresh();
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
            trackedFolders.clear();
            if (bottomHintLabel) {
                bottomHintLabel->setText("可以直接把文件拖进编码队列来开始，如果文件很多或者有其他需求再用这个页面");
            }
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

    void refresh() {
        int removedCount = 0;

        // 1. 检查表格中已存在的文件：如果文件在磁盘上已被删除则移除，否则重新获取并更新大小
        for (int r = fileTable->rowCount() - 1; r >= 0; --r) {
            auto *pathItem = fileTable->item(r, 1);
            if (!pathItem) continue;
            QString path = pathItem->text();
            QFileInfo fi(path);
            if (!fi.exists()) {
                fileTable->removeRow(r);
                removedCount++;
            } else {
                qint64 bytes = fi.size();
                QString sizeStr;
                if (bytes < 1024 * 1024) {
                    sizeStr = QString::asprintf("%.1f KB", bytes / 1024.0);
                } else if (bytes < 1024 * 1024 * 1024) {
                    sizeStr = QString::asprintf("%.2f MB", bytes / (1024.0 * 1024.0));
                } else {
                    sizeStr = QString::asprintf("%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
                }
                if (auto *sizeItem = fileTable->item(r, 3)) {
                    sizeItem->setText(sizeStr);
                }
            }
        }

        // 2. 重新扫描所有追踪的文件夹及子目录中的新媒体文件
        QStringList filters = {"*.mp4", "*.mkv", "*.mov", "*.avi", "*.flv", "*.ts", "*.webm", "*.wmv", "*.mp3", "*.aac"};
        int countBeforeScan = fileTable->rowCount();
        for (const QString &folderPath : trackedFolders) {
            if (!QDir(folderPath).exists()) continue;
            QDirIterator it(folderPath, filters, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                appendFileRow(it.next());
            }
        }
        int addedCount = fileTable->rowCount() - countBeforeScan;

        // 3. 重新应用当前排序规则
        int sortIdx = sortCombo->currentIndex();
        if (sortIdx == 1) {
            fileTable->sortItems(0, Qt::AscendingOrder);
        } else if (sortIdx == 2) {
            fileTable->sortItems(3, Qt::AscendingOrder);
        }

        // 4. 更新底部状态反馈
        if (bottomHintLabel) {
            bottomHintLabel->setText(
                QString("刷新完成: 新增 %1 个文件，移除 %2 个失效文件，当前共 %3 个文件")
                    .arg(addedCount).arg(removedCount).arg(fileTable->rowCount())
            );
        }
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

void FilePrepPage::addFolder(const QString &dirPath) {
    Q_D(FilePrepPage);
    QDir dir(dirPath);
    if (!dir.exists()) return;

    d->trackedFolders.insert(QDir::cleanPath(dir.absolutePath()));

    QStringList filters = {"*.mp4", "*.mkv", "*.mov", "*.avi", "*.flv", "*.ts", "*.webm", "*.wmv", "*.mp3", "*.aac"};
    QDirIterator it(dirPath, filters, QDir::Files, QDirIterator::Subdirectories);
    int beforeCount = d->fileTable->rowCount();
    while (it.hasNext()) {
        d->appendFileRow(it.next());
    }
    int added = d->fileTable->rowCount() - beforeCount;
    if (d->bottomHintLabel) {
        d->bottomHintLabel->setText(
            QString("已导入文件夹: %1 (包含子目录，新增 %2 个媒体文件)")
                .arg(dir.dirName()).arg(added)
        );
    }
}

void FilePrepPage::refresh() {
    Q_D(FilePrepPage);
    d->refresh();
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
            QString path = u.toLocalFile();
            QFileInfo fi(path);
            if (fi.isDir()) {
                addFolder(path);
            } else if (fi.isFile()) {
                files.append(path);
            }
        }
    }
    if (!files.isEmpty()) {
        addFiles(files);
    }
    event->acceptProposedAction();
}

} // namespace ffmpeg_transform
