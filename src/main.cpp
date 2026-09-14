#include "ui/MainWindow.h"
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QDebug>

int main(int argc, char *argv[]) {
    // 启用高分屏支持
    #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    #endif

    QApplication app(argc, argv);
    app.setApplicationName("FFmpegVideoTransform");
    app.setApplicationDisplayName("FFmpeg 视频格式转换器");
    app.setOrganizationName("MediaStudio");

    // 加载全局 QSS 样式表 (资源解耦加载)
    QFile qssFile(":/styles/theme.qss");
    if (qssFile.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream ts(&qssFile);
        app.setStyleSheet(ts.readAll());
        qssFile.close();
    } else {
        qWarning() << "未能加载主题样式表 :/styles/theme.qss";
    }

    ffmpeg_transform::MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
