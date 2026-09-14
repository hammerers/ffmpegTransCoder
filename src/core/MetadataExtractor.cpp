#include "MetadataExtractor.h"
#include "FFmpegUtils.h"
#include <QFileInfo>
#include <QDebug>

namespace ffmpeg_transform {

class MetadataExtractorPrivate {
public:
    MetadataExtractor *q_ptr{nullptr};

    bool doExtract(const QString &filePath, MediaInfo &outInfo, QString &errorMsg) {
        AVFormatContext *fmtCtxRaw = nullptr;
        QByteArray pathUtf8 = filePath.toUtf8();

        int ret = avformat_open_input(&fmtCtxRaw, pathUtf8.constData(), nullptr, nullptr);
        if (ret < 0) {
            errorMsg = QString("无法打开媒体文件: %1 (错误: %2)").arg(filePath, ffmpegErrorToString(ret));
            return false;
        }
        UniqueAvFormatInput fmtCtx(fmtCtxRaw);

        ret = avformat_find_stream_info(fmtCtx.get(), nullptr);
        if (ret < 0) {
            errorMsg = QString("无法读取流信息: %1 (错误: %2)").arg(filePath, ffmpegErrorToString(ret));
            return false;
        }

        QFileInfo fileInfo(filePath);
        outInfo.filePath = filePath;
        outInfo.fileName = fileInfo.fileName();
        outInfo.fileSizeBytes = fileInfo.size();
        outInfo.containerFormat = QString::fromUtf8(fmtCtx->iformat->name ? fmtCtx->iformat->name : "unknown");
        outInfo.containerLongName = QString::fromUtf8(fmtCtx->iformat->long_name ? fmtCtx->iformat->long_name : "");

        if (fmtCtx->duration != AV_NOPTS_VALUE) {
            outInfo.durationSec = static_cast<double>(fmtCtx->duration) / AV_TIME_BASE;
        } else {
            outInfo.durationSec = 0.0;
        }
        outInfo.overallBitrate = fmtCtx->bit_rate;

        outInfo.videoStreams.clear();
        outInfo.audioStreams.clear();
        outInfo.tags.clear();

        for (unsigned int i = 0; i < fmtCtx->nb_streams; ++i) {
            AVStream *stream = fmtCtx->streams[i];
            AVCodecParameters *par = stream->codecpar;

            StreamInfo sInfo;
            sInfo.streamIndex = static_cast<int>(i);
            sInfo.bitrate = par->bit_rate;

            const AVCodec *decoder = avcodec_find_decoder(par->codec_id);
            sInfo.codecName = QString::fromUtf8(avcodec_get_name(par->codec_id));
            sInfo.codecLongName = decoder ? QString::fromUtf8(decoder->long_name) : sInfo.codecName;

            if (par->codec_type == AVMEDIA_TYPE_VIDEO) {
                sInfo.type = StreamType::Video;
                sInfo.width = par->width;
                sInfo.height = par->height;

                // 计算帧率
                if (stream->avg_frame_rate.den > 0 && stream->avg_frame_rate.num > 0) {
                    sInfo.fps = av_q2d(stream->avg_frame_rate);
                } else if (stream->r_frame_rate.den > 0 && stream->r_frame_rate.num > 0) {
                    sInfo.fps = av_q2d(stream->r_frame_rate);
                } else {
                    sInfo.fps = 0.0;
                }

                sInfo.totalFrames = stream->nb_frames;
                const char *pixFmtName = av_get_pix_fmt_name(static_cast<AVPixelFormat>(par->format));
                sInfo.pixelFormat = pixFmtName ? QString::fromUtf8(pixFmtName) : "unknown";

                if (par->sample_aspect_ratio.num > 0) {
                    sInfo.aspectRatio = QString("%1:%2").arg(par->sample_aspect_ratio.num).arg(par->sample_aspect_ratio.den);
                }

                outInfo.videoStreams.append(sInfo);
            } else if (par->codec_type == AVMEDIA_TYPE_AUDIO) {
                sInfo.type = StreamType::Audio;
                sInfo.sampleRate = par->sample_rate;
                sInfo.channels = par->channels;

                char layoutBuf[128] = {0};
                av_get_channel_layout_string(layoutBuf, sizeof(layoutBuf), par->channels, par->channel_layout);
                sInfo.channelLayout = QString::fromUtf8(layoutBuf);

                const char *sampleFmtName = av_get_sample_fmt_name(static_cast<AVSampleFormat>(par->format));
                sInfo.sampleFormat = sampleFmtName ? QString::fromUtf8(sampleFmtName) : "unknown";

                outInfo.audioStreams.append(sInfo);
            }
        }

        // 读取标签元数据
        AVDictionaryEntry *tag = nullptr;
        while ((tag = av_dict_get(fmtCtx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
            if (tag->key && tag->value) {
                outInfo.tags.insert(QString::fromUtf8(tag->key), QString::fromUtf8(tag->value));
            }
        }

        return true;
    }
};

MetadataExtractor::MetadataExtractor(QObject *parent)
    : QObject(parent), d_ptr(std::make_unique<MetadataExtractorPrivate>()) {
    Q_D(MetadataExtractor);
    d->q_ptr = this;
}

MetadataExtractor::~MetadataExtractor() = default;

bool MetadataExtractor::extract(const QString &filePath, MediaInfo &outInfo, QString &errorMsg) {
    Q_D(MetadataExtractor);
    return d->doExtract(filePath, outInfo, errorMsg);
}

} // namespace ffmpeg_transform
