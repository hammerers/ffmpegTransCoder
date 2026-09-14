#pragma once

#include <QString>
#include <QList>
#include <QMap>
#include <QImage>
#include <cstdint>

namespace ffmpeg_transform {

enum class StreamType {
    Unknown,
    Video,
    Audio,
    Subtitle
};

struct StreamInfo {
    StreamType type{StreamType::Unknown};
    int streamIndex{-1};
    QString codecName;
    QString codecLongName;
    int64_t bitrate{0};

    // 视频特有参数
    int width{0};
    int height{0};
    double fps{0.0};
    int64_t totalFrames{0};
    QString pixelFormat;
    QString aspectRatio;

    // 音频特有参数
    int sampleRate{0};
    int channels{0};
    QString channelLayout;
    QString sampleFormat;
};

struct MediaInfo {
    QString filePath;
    QString fileName;
    int64_t fileSizeBytes{0};
    QString containerFormat;
    QString containerLongName;
    double durationSec{0.0};
    int64_t overallBitrate{0};

    QList<StreamInfo> videoStreams;
    QList<StreamInfo> audioStreams;
    QMap<QString, QString> tags;

    bool hasVideo() const { return !videoStreams.isEmpty(); }
    bool hasAudio() const { return !audioStreams.isEmpty(); }

    const StreamInfo* primaryVideo() const {
        return videoStreams.isEmpty() ? nullptr : &videoStreams.first();
    }

    const StreamInfo* primaryAudio() const {
        return audioStreams.isEmpty() ? nullptr : &audioStreams.first();
    }

    QString resolutionString() const {
        if (const auto *v = primaryVideo()) {
            return QString("%1 x %2").arg(v->width).arg(v->height);
        }
        return "无视频流";
    }

    QString formattedDuration() const {
        int total = static_cast<int>(durationSec);
        int h = total / 3600;
        int m = (total % 3600) / 60;
        int s = total % 60;
        if (h > 0) {
            return QString("%1:%2:%3")
                .arg(h, 2, 10, QChar('0'))
                .arg(m, 2, 10, QChar('0'))
                .arg(s, 2, 10, QChar('0'));
        }
        return QString("%1:%2")
            .arg(m, 2, 10, QChar('0'))
            .arg(s, 2, 10, QChar('0'));
    }

    QString formattedFileSize() const {
        double bytes = static_cast<double>(fileSizeBytes);
        const char *units[] = {"B", "KB", "MB", "GB", "TB"};
        int idx = 0;
        while (bytes >= 1024.0 && idx < 4) {
            bytes /= 1024.0;
            idx++;
        }
        return QString::asprintf("%.2f %s", bytes, units[idx]);
    }
};

enum class VideoCodecType {
    Copy,
    H264,
    H265,
    None // 纯音频输出
};

enum class AudioCodecType {
    Copy,
    AAC,
    MP3,
    None // 静音视频
};

enum class ResolutionScale {
    Original,
    Scale4K,      // 3840x2160
    Scale1080p,   // 1920x1080
    Scale720p,    // 1280x720
    Scale480p,    // 854x480
    Custom
};

enum class FpsOption {
    Original,
    Fps60,
    Fps30,
    Fps24,
    Custom
};

enum class QualityMode {
    CRF,        // 恒定质量模式
    Bitrate     // 目标比特率模式
};

struct TranscodeConfig {
    QString inputPath;
    QString outputPath;

    QString containerFormat{"mp4"}; // mp4, mkv, avi, mov, mp3, aac, etc.

    VideoCodecType videoCodec{VideoCodecType::H264};
    AudioCodecType audioCodec{AudioCodecType::AAC};

    ResolutionScale resolutionScale{ResolutionScale::Original};
    int customWidth{1920};
    int customHeight{1080};

    FpsOption fpsOption{FpsOption::Original};
    double customFps{30.0};

    QualityMode qualityMode{QualityMode::CRF};
    int crf{23}; // 常用: H.264 (23), H.265 (28)
    int64_t videoBitrate{4000000}; // 4 Mbps

    int audioBitrate{192000};      // 192 kbps
    int audioSampleRate{44100};    // 44.1 kHz

    QString preset{"medium"};      // ultrafast, superfast, veryfast, faster, fast, medium, slow
    int threads{0};               // 0 = 自动
};

struct TranscodeProgress {
    double percent{0.0};           // 0.0 ~ 100.0
    double currentPtsSec{0.0};
    double totalDurationSec{0.0};
    double currentFps{0.0};
    double speedMultiplier{0.0};   // 例如 2.3x
    qint64 elapsedMs{0};
    int etaSec{0};

    QString formattedEta() const {
        if (etaSec <= 0) return "--:--";
        int m = etaSec / 60;
        int s = etaSec % 60;
        return QString("%1:%2")
            .arg(m, 2, 10, QChar('0'))
            .arg(s, 2, 10, QChar('0'));
    }

    QString formattedSpeed() const {
        return QString::asprintf("%.1fx", speedMultiplier);
    }
};

enum class TaskState {
    Pending,
    Analyzing,
    Converting,
    Paused,
    Completed,
    Failed,
    Canceled
};

inline const char* taskStateToString(TaskState state) {
    switch (state) {
    case TaskState::Pending: return "Pending";
    case TaskState::Analyzing: return "Analyzing";
    case TaskState::Converting: return "Converting";
    case TaskState::Paused: return "Paused";
    case TaskState::Completed: return "Completed";
    case TaskState::Failed: return "Failed";
    case TaskState::Canceled: return "Canceled";
    }
    return "Unknown";
}

} // namespace ffmpeg_transform
