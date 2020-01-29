#ifndef _MEDIA_WRITER_FFMPEG_H_
#define _MEDIA_WRITER_FFMPEG_H_

#include <stdint.h>
#include <fstream>
#include <thread>
#include "codec.h"

extern "C" {

#include <ffmpeg/libavcodec/avcodec.h>
#include <ffmpeg/libavformat/avformat.h>
#include <ffmpeg/libswscale/swscale.h>
#include <ffmpeg/libavutil/imgutils.h>

};

typedef struct
{
    AVOutputFormat  *pOutFormat;
    AVFormatContext *pFormatContext;
    // video
    AVStream		*pVideoStream;
    AVCodec			*pVideoCodec;
    AVCodecContext	*pVideoCodecCtx;
    SwsContext      *pSws;
    AVDictionary    *pOpts;
    // audio
    AVStream		*pAudioStream;
    AVCodec			*pAudioCodec;
    AVCodecContext	*pAudioCodecCtx;

    // common
    std::string     sFilePath;
    int             nFPS_num; // use 1 for 30fps
    int             nFPS_den; // use 30 for 30fps
    std::string     sVersion;
    std::thread     tEncoder;
} ffw_video_encoder_context_t;


class CWriterFFMPEG : public CBaseEncoder
{
public:

    CWriterFFMPEG();

    virtual ~CWriterFFMPEG();

    virtual MediaCompletion init(const char* pcFname, enc_config_t& tConfig);

    virtual MediaCompletion addVideoStream(const video_stream_info_t& info, unsigned int& nStreamID);

    virtual MediaCompletion addVideoFrame(const unsigned int nStreamID, const frame_info_t* pFrame);

    virtual MediaCompletion startMedia();
    
    virtual MediaCompletion stopMedia();

    virtual std::string getVersionString() const;

    virtual MediaCompletion closeStream(const unsigned int nStreamID);

    virtual MediaCompletion clearContext();

protected:

    bool loadSettings();

    ffw_video_encoder_context_t vEncoder;

    enc_config_t                m_encConfig;
};

#endif // _MEDIA_WRITER_FFMPEG_H_
