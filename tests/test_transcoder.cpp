#include "core/CommonTypes.h"
#include "core/MetadataExtractor.h"
#include "core/ThumbnailExtractor.h"
#include "core/TranscodeEngine.h"
#include "core/PreviewPlayer.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <iostream>
#include <thread>
#include <chrono>

using namespace ffmpeg_transform;

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    std::cout << "========================================\n";
    std::cout << "  FFmpeg Native Transcoder Core Tests   \n";
    std::cout << "========================================\n";

    QString testVideo = "E:/temp/FTP/me/v1.mp4";
    if (argc > 1) {
        testVideo = QString::fromLocal8Bit(argv[1]);
    } else if (!QFile::exists(testVideo)) {
        if (QFile::exists("v1.mp4")) testVideo = "v1.mp4";
        else if (QFile::exists("../v1.mp4")) testVideo = "../v1.mp4";
    }

    if (!QFile::exists(testVideo)) {
        std::cerr << "[ERROR] Test video does not exist: " << testVideo.toStdString() << std::endl;
        std::cerr << "Usage: test_transcoder [path_to_video.mp4]" << std::endl;
        return 1;
    }

    // 1. 测试元数据解析
    std::cout << "\n[TEST 1] Testing MetadataExtractor..." << std::endl;
    MetadataExtractor metaExtractor;
    MediaInfo info;
    QString err;
    bool ok = metaExtractor.extract(testVideo, info, err);
    if (!ok) {
        std::cerr << "[FAIL] Failed to extract metadata: " << err.toStdString() << std::endl;
        return 1;
    }

    std::cout << "  File Name:      " << info.fileName.toStdString() << std::endl;
    std::cout << "  File Size:      " << info.formattedFileSize().toStdString() << std::endl;
    std::cout << "  Container:      " << info.containerFormat.toStdString() << " (" << info.containerLongName.toStdString() << ")\n";
    std::cout << "  Duration:       " << info.formattedDuration().toStdString() << " (" << info.durationSec << "s)\n";
    std::cout << "  Overall Bitrate:" << (info.overallBitrate / 1000) << " kbps\n";
    std::cout << "  Video Streams:  " << info.videoStreams.size() << std::endl;
    if (const auto *v = info.primaryVideo()) {
        std::cout << "    Resolution:   " << v->width << "x" << v->height << std::endl;
        std::cout << "    Codec:        " << v->codecName.toStdString() << " (" << v->codecLongName.toStdString() << ")\n";
        std::cout << "    FPS:          " << v->fps << std::endl;
        std::cout << "    Pixel Format: " << v->pixelFormat.toStdString() << std::endl;
    }
    std::cout << "  Audio Streams:  " << info.audioStreams.size() << std::endl;
    if (const auto *a = info.primaryAudio()) {
        std::cout << "    Codec:        " << a->codecName.toStdString() << std::endl;
        std::cout << "    Sample Rate:  " << a->sampleRate << " Hz\n";
        std::cout << "    Channels:     " << a->channels << " (" << a->channelLayout.toStdString() << ")\n";
    }
    std::cout << "[PASS] MetadataExtractor test passed!" << std::endl;

    // 2. 测试缩略图提取
    std::cout << "\n[TEST 2] Testing ThumbnailExtractor..." << std::endl;
    ThumbnailExtractor thumbExtractor;
    QImage thumbnail;
    ok = thumbExtractor.extractThumbnail(testVideo, thumbnail, err, 0.08, 480);
    if (!ok || thumbnail.isNull()) {
        std::cerr << "[FAIL] Failed to extract thumbnail: " << err.toStdString() << std::endl;
        return 1;
    }
    std::cout << "  Thumbnail size: " << thumbnail.width() << "x" << thumbnail.height() << std::endl;
    QDir().mkpath("test_output");
    QString thumbPath = "test_output/thumb_v1.png";
    thumbnail.save(thumbPath);
    std::cout << "  Saved thumbnail to: " << thumbPath.toStdString() << std::endl;
    std::cout << "[PASS] ThumbnailExtractor test passed!" << std::endl;

    // 3. 测试转码引擎 (测试视频转 MP3 音频提取与轻量转码)
    std::cout << "\n[TEST 3] Testing TranscodeEngine (Audio Extraction MP4 -> MP3)..." << std::endl;
    TranscodeConfig audioCfg;
    audioCfg.inputPath = testVideo;
    audioCfg.outputPath = "test_output/v1_extracted.mp3";
    audioCfg.containerFormat = "mp3";
    audioCfg.videoCodec = VideoCodecType::None; // 提取音频
    audioCfg.audioCodec = AudioCodecType::MP3;
    audioCfg.audioBitrate = 192000;

    TranscodeEngine audioEngine;
    QObject::connect(&audioEngine, &TranscodeEngine::progressUpdated, [](const TranscodeProgress &p) {
        std::cout << "\r  Progress: " << p.percent << "% | Speed: " << p.formattedSpeed().toStdString() << " | ETA: " << p.formattedEta().toStdString() << std::flush;
    });

    bool transcodeFinished = false;
    bool transcodeSuccess = false;
    QObject::connect(&audioEngine, &TranscodeEngine::finished, [&](bool success, const QString &msg) {
        std::cout << "\n  Finished: " << (success ? "SUCCESS" : "FAIL") << " - " << msg.toStdString() << std::endl;
        transcodeSuccess = success;
        transcodeFinished = true;
        app.quit();
    });

    if (!audioEngine.start(audioCfg)) {
        std::cerr << "[FAIL] Failed to start transcode engine!" << std::endl;
        return 1;
    }

    app.exec();

    if (!transcodeSuccess || !QFile::exists(audioCfg.outputPath)) {
        std::cerr << "[FAIL] Audio extraction test failed!" << std::endl;
        return 1;
    }

    std::cout << "  Output file generated: " << audioCfg.outputPath.toStdString()
              << " (" << QFileInfo(audioCfg.outputPath).size() << " bytes)\n";
    std::cout << "[PASS] TranscodeEngine (Audio) test passed!" << std::endl;

    // 4. 测试视频转码 (MP4 -> MKV H.264 ultrafast)
    std::cout << "\n[TEST 4] Testing TranscodeEngine (Video Transcoding MP4 -> MKV)..." << std::endl;
    TranscodeConfig videoCfg;
    videoCfg.inputPath = testVideo;
    videoCfg.outputPath = "test_output/v1_converted.mkv";
    videoCfg.containerFormat = "mkv";
    videoCfg.videoCodec = VideoCodecType::H264;
    videoCfg.audioCodec = AudioCodecType::AAC;
    videoCfg.resolutionScale = ResolutionScale::Scale720p; // 测试分辨率变换 852x480 -> 1280x720
    videoCfg.preset = "ultrafast";
    videoCfg.qualityMode = QualityMode::CRF;
    videoCfg.crf = 26;

    TranscodeEngine videoEngine;
    QObject::connect(&videoEngine, &TranscodeEngine::progressUpdated, [](const TranscodeProgress &p) {
        std::cout << "\r  Progress: " << p.percent << "% | Speed: " << p.formattedSpeed().toStdString() << " | FPS: " << p.currentFps << " | ETA: " << p.formattedEta().toStdString() << std::flush;
    });

    bool videoSuccess = false;
    QObject::connect(&videoEngine, &TranscodeEngine::finished, [&](bool success, const QString &msg) {
        std::cout << "\n  Finished: " << (success ? "SUCCESS" : "FAIL") << " - " << msg.toStdString() << std::endl;
        videoSuccess = success;
        app.quit();
    });

    if (!videoEngine.start(videoCfg)) {
        std::cerr << "[FAIL] Failed to start video transcode engine!" << std::endl;
        return 1;
    }

    app.exec();

    if (!videoSuccess || !QFile::exists(videoCfg.outputPath)) {
        std::cerr << "[FAIL] Video transcoding test failed!" << std::endl;
        return 1;
    }

    std::cout << "  Output video generated: " << videoCfg.outputPath.toStdString()
              << " (" << QFileInfo(videoCfg.outputPath).size() << " bytes)\n";
    std::cout << "[PASS] TranscodeEngine (Video) test passed!" << std::endl;

    // 5. 测试轻量级视频预览播放引擎 (单流调参预览 + 双流同步对比)
    std::cout << "\n[TEST 5] Testing PreviewPlayer (Single and Dual mode)..." << std::endl;
    PreviewPlayer player;
    bool openSingle = player.open(testVideo);
    if (!openSingle || player.mode() != PlaybackMode::SingleSource || player.duration() <= 0.0) {
        std::cerr << "[FAIL] Failed to open PreviewPlayer in SingleSource mode!" << std::endl;
        return 1;
    }
    std::cout << "  SingleSource mode opened successfully. Duration: " << player.duration() << "s\n";
    std::cout << "  Source video size: " << player.sourceVideoSize().width() << "x" << player.sourceVideoSize().height() << std::endl;
    if (!player.sourceVideoSize().isValid() || player.sourceVideoSize().width() <= 0) {
        std::cerr << "[FAIL] Invalid sourceVideoSize in PreviewPlayer!" << std::endl;
        return 1;
    }

    bool openDual = player.open(testVideo, videoCfg.outputPath);
    if (!openDual || player.mode() != PlaybackMode::DualSource) {
        std::cerr << "[FAIL] Failed to open PreviewPlayer in DualSource mode!" << std::endl;
        return 1;
    }
    std::cout << "  DualSource mode opened successfully (Source vs Result).\n";
    std::cout << "  Compared video size: " << player.comparedVideoSize().width() << "x" << player.comparedVideoSize().height() << std::endl;
    if (!player.comparedVideoSize().isValid() || player.comparedVideoSize().width() <= 0) {
        std::cerr << "[FAIL] Invalid comparedVideoSize in PreviewPlayer!" << std::endl;
        return 1;
    }

    player.play();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    player.pause();
    player.seek(1.0);
    player.stop();
    player.close();
    std::cout << "[PASS] PreviewPlayer test passed!" << std::endl;

    std::cout << "\n========================================" << std::endl;
    std::cout << "  ALL CORE TESTS PASSED SUCCESSFULLY!  " << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
