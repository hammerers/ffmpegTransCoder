#include "TranscodeTaskManager.h"
#include "core/MetadataExtractor.h"
#include "core/ThumbnailExtractor.h"
#include "core/TranscodeEngine.h"
#include <QThreadPool>
#include <QtConcurrent>
#include <QMap>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <map>

namespace ffmpeg_transform {

class TranscodeTaskManagerPrivate {
public:
    TranscodeTaskManager *q_ptr{nullptr};
    QList<TranscodeTask*> tasks;
    std::map<QString, std::unique_ptr<TranscodeEngine>> activeEngines;
    int maxConcurrent{1};
    bool isQueueActive{false};

    ~TranscodeTaskManagerPrivate() {
        for (auto &pair : activeEngines) {
            if (pair.second) {
                pair.second->cancel();
            }
        }
        activeEngines.clear();
        qDeleteAll(tasks);
        tasks.clear();
    }

    void analyzeMediaAsync(TranscodeTask *task) {
        task->setState(TaskState::Analyzing);
        QString filePath = task->inputFilePath();

        // 后台异步分析媒体元数据与提取缩略图
        (void)QtConcurrent::run([this, task, filePath]() {
            MetadataExtractor metaExtractor;
            MediaInfo info;
            QString errorMsg;
            bool ok = metaExtractor.extract(filePath, info, errorMsg);

            ThumbnailExtractor thumbExtractor;
            QImage thumb;
            QString thumbErr;
            bool thumbOk = thumbExtractor.extractThumbnail(filePath, thumb, thumbErr);

            // 回调至主线程更新状态
            QMetaObject::invokeMethod(q_ptr, [task, ok, info, thumbOk, thumb, errorMsg]() {
                if (ok) {
                    task->setMediaInfo(info);
                } else {
                    task->setErrorMessage(errorMsg);
                }

                if (thumbOk) {
                    task->setThumbnail(thumb);
                }

                task->setState(TaskState::Pending);
            }, Qt::QueuedConnection);
        });
    }

    void scheduleNext() {
        if (!isQueueActive) {
            return;
        }

        int runningCount = 0;
        for (auto *t : tasks) {
            if (t->state() == TaskState::Converting) {
                runningCount++;
            }
        }

        if (runningCount >= maxConcurrent) {
            return;
        }

        // 寻找下一个处于 Pending 状态的任务
        for (auto *t : tasks) {
            if (t->state() == TaskState::Pending) {
                startTaskInternal(t);
                runningCount++;
                if (runningCount >= maxConcurrent) {
                    break;
                }
            }
        }

        // 检查是否全部已完成
        bool anyPendingOrRunning = false;
        for (auto *t : tasks) {
            if (t->state() == TaskState::Pending || t->state() == TaskState::Converting || t->state() == TaskState::Analyzing) {
                anyPendingOrRunning = true;
                break;
            }
        }

        if (!anyPendingOrRunning && !tasks.isEmpty()) {
            isQueueActive = false;
            emit q_ptr->allTasksCompleted();
        }
    }

    void startTaskInternal(TranscodeTask *task) {
        if (!task || task->state() == TaskState::Converting) return;

        QString taskId = task->id();
        auto engine = std::make_unique<TranscodeEngine>();

        QObject::connect(engine.get(), &TranscodeEngine::stateChanged, q_ptr, [task](TaskState state) {
            task->setState(state);
        });

        QObject::connect(engine.get(), &TranscodeEngine::progressUpdated, q_ptr, [task](const TranscodeProgress &p) {
            task->setProgress(p);
        });

        QObject::connect(engine.get(), &TranscodeEngine::finished, q_ptr, [this, task, taskId](bool success, const QString &message) {
            if (!success) {
                task->setErrorMessage(message);
            }
            activeEngines.erase(taskId);
            scheduleNext();
        });

        TranscodeConfig cfg = task->config();
        if (engine->start(cfg)) {
            activeEngines[taskId] = std::move(engine);
        } else {
            task->setState(TaskState::Failed);
            task->setErrorMessage("启动转码引擎失败");
            scheduleNext();
        }
    }
};

TranscodeTaskManager::TranscodeTaskManager(QObject *parent)
    : QObject(parent), d_ptr(std::make_unique<TranscodeTaskManagerPrivate>()) {
    Q_D(TranscodeTaskManager);
    d->q_ptr = this;
}

TranscodeTaskManager::~TranscodeTaskManager() = default;

TranscodeTask* TranscodeTaskManager::addTask(const QString &inputFilePath) {
    Q_D(TranscodeTaskManager);
    auto *task = new TranscodeTask(inputFilePath, this);
    d->tasks.append(task);

    // 绑定单个任务状态变化信号，向上统一派发状态变更通知
    connect(task, &TranscodeTask::stateChanged, this, [this, task](TaskState state) {
        emit taskStateChanged(task, state);
    });

    emit taskAdded(task);
    d->analyzeMediaAsync(task);
    return task;
}

void TranscodeTaskManager::removeTask(const QString &taskId) {
    Q_D(TranscodeTaskManager);
    cancelTask(taskId);

    for (int i = 0; i < d->tasks.size(); ++i) {
        if (d->tasks[i]->id() == taskId) {
            auto *t = d->tasks.takeAt(i);
            if (t->state() != TaskState::Completed) {
                QString outPath = t->config().outputPath;
                if (!outPath.isEmpty() && QFile::exists(outPath)) {
                    QFileInfo fi(outPath);
                    if (fi.size() == 0 || t->state() == TaskState::Canceled || t->state() == TaskState::Pending || t->state() == TaskState::Failed) {
                        QFile::remove(outPath);
                    }
                }
            }
            emit taskRemoved(taskId);
            t->deleteLater();
            break;
        }
    }
    if (d->isQueueActive) {
        d->scheduleNext();
    }
}

void TranscodeTaskManager::clearAllTasks() {
    Q_D(TranscodeTaskManager);
    cancelAll();
    for (auto *t : d->tasks) {
        if (t->state() != TaskState::Completed) {
            QString outPath = t->config().outputPath;
            if (!outPath.isEmpty() && QFile::exists(outPath)) {
                QFileInfo fi(outPath);
                if (fi.size() == 0 || t->state() == TaskState::Canceled || t->state() == TaskState::Pending || t->state() == TaskState::Failed) {
                    QFile::remove(outPath);
                }
            }
        }
        emit taskRemoved(t->id());
        t->deleteLater();
    }
    d->tasks.clear();
    d->isQueueActive = false;
}

QList<TranscodeTask*> TranscodeTaskManager::allTasks() const {
    return d_ptr->tasks;
}

TranscodeTask* TranscodeTaskManager::getTask(const QString &taskId) const {
    for (auto *t : d_ptr->tasks) {
        if (t->id() == taskId) return t;
    }
    return nullptr;
}

void TranscodeTaskManager::startAll() {
    Q_D(TranscodeTaskManager);
    d->isQueueActive = true;
    for (auto *t : d->tasks) {
        if (t->state() == TaskState::Paused) {
            resumeTask(t->id());
        } else if (t->state() == TaskState::Failed || t->state() == TaskState::Canceled) {
            t->setState(TaskState::Pending);
        }
    }
    d->scheduleNext();
}

void TranscodeTaskManager::pauseAll() {
    Q_D(TranscodeTaskManager);
    d->isQueueActive = false;
    for (auto &pair : d->activeEngines) {
        if (pair.second) pair.second->pause();
    }
}

void TranscodeTaskManager::resumeAll() {
    Q_D(TranscodeTaskManager);
    d->isQueueActive = true;
    for (auto &pair : d->activeEngines) {
        if (pair.second) pair.second->resume();
    }
    d->scheduleNext();
}

void TranscodeTaskManager::cancelAll() {
    Q_D(TranscodeTaskManager);
    d->isQueueActive = false;
    for (auto &pair : d->activeEngines) {
        if (pair.second) pair.second->cancel();
    }
    d->activeEngines.clear();
    for (auto *t : d->tasks) {
        if (t->state() == TaskState::Pending || t->state() == TaskState::Converting || t->state() == TaskState::Paused) {
            t->setState(TaskState::Canceled);
            QString outPath = t->config().outputPath;
            if (!outPath.isEmpty() && QFile::exists(outPath)) {
                QFileInfo fi(outPath);
                if (fi.size() == 0) {
                    QFile::remove(outPath);
                }
            }
        }
    }
}

void TranscodeTaskManager::startTask(const QString &taskId) {
    Q_D(TranscodeTaskManager);
    auto *task = getTask(taskId);
    if (task && task->state() == TaskState::Pending) {
        d->startTaskInternal(task);
    }
}

void TranscodeTaskManager::pauseTask(const QString &taskId) {
    Q_D(TranscodeTaskManager);
    auto it = d->activeEngines.find(taskId);
    if (it != d->activeEngines.end() && it->second) {
        it->second->pause();
    }
}

void TranscodeTaskManager::resumeTask(const QString &taskId) {
    Q_D(TranscodeTaskManager);
    auto it = d->activeEngines.find(taskId);
    if (it != d->activeEngines.end() && it->second) {
        it->second->resume();
    } else {
        startTask(taskId);
    }
}

void TranscodeTaskManager::cancelTask(const QString &taskId) {
    Q_D(TranscodeTaskManager);
    auto it = d->activeEngines.find(taskId);
    if (it != d->activeEngines.end()) {
        if (it->second) it->second->cancel();
        d->activeEngines.erase(it);
    }
    auto *task = getTask(taskId);
    if (task && task->state() != TaskState::Completed) {
        task->setState(TaskState::Canceled);
        QString outPath = task->config().outputPath;
        if (!outPath.isEmpty() && QFile::exists(outPath)) {
            QFileInfo fi(outPath);
            if (fi.size() == 0) {
                QFile::remove(outPath);
            }
        }
    }
    if (d->isQueueActive) {
        d->scheduleNext();
    }
}

void TranscodeTaskManager::setMaxConcurrentTasks(int count) {
    d_ptr->maxConcurrent = std::max(1, count);
}

int TranscodeTaskManager::maxConcurrentTasks() const {
    return d_ptr->maxConcurrent;
}

} // namespace ffmpeg_transform
