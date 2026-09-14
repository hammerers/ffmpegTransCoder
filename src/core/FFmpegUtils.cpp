#include "FFmpegUtils.h"

namespace ffmpeg_transform {

QString ffmpegErrorToString(int errNum) {
    char errBuf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(errNum, errBuf, sizeof(errBuf));
    return QString::fromUtf8(errBuf);
}

} // namespace ffmpeg_transform
