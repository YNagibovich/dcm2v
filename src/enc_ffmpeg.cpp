#include <assert.h>
#include <iostream>

#include "enc_ffmpeg.h"


#ifdef _WIN32
    #define strcmpX strcmpi
// _WIN32
#elif __linux__
    #include <strings.h>
    #define strcmpX strcasecmp
// __linux__
#else
    #define strcmpX strcmp
    // default
#endif

//////////////////////////////////////////////////////////////////////////
// FFMPEG globals

// ffmpeg has its own threads management
// 0 - auto
//#define FFMPEG_THREADS_NUM 0

#ifdef _DEBUG
#define FFMPEG_LOG_LEVEL AV_LOG_DEBUG
#else
#define FFMPEG_LOG_LEVEL AV_LOG_QUIET
//#define FFMPEG_LOG_LEVEL AV_LOG_DEBUG
#endif 

//////////////////////////////////////////////////////////////////////////

CWriterFFMPEG::CWriterFFMPEG()
{
    // Register ALL internals
    // TBD register only mp4 mux\demux and h264 codec
    av_register_all();
    avcodec_register_all();

    // reduce ffmpeg's log
    av_log_set_level(FFMPEG_LOG_LEVEL);
    vEncoder.pOutFormat = nullptr;
    vEncoder.pFormatContext = nullptr;
    vEncoder.pVideoStream = nullptr;
    vEncoder.pVideoCodec = nullptr;
    vEncoder.pAudioStream = nullptr;
    vEncoder.pAudioCodec = nullptr;
    vEncoder.pVideoCodecCtx = nullptr;
    vEncoder.pAudioCodecCtx = nullptr;
    vEncoder.pSws = nullptr;
    vEncoder.pOpts = nullptr;
    vEncoder.sVersion = "h264 sw."; // TBD get embedded 264 enc
    // default fps
    vEncoder.nFPS_num = DEF_FPS_NUM;
    vEncoder.nFPS_den = DEF_FPS_DEN;
    m_nFramesDone = 0;
}

CWriterFFMPEG::~CWriterFFMPEG()
{
    clearContext();
}

std::string CWriterFFMPEG::getVersionString() const
{
    return vEncoder.sVersion;
}

MediaCompletion CWriterFFMPEG::clearContext()
{
    if (vEncoder.pVideoCodecCtx)
    {
        avcodec_free_context(&vEncoder.pVideoCodecCtx);
    }
    if (vEncoder.pAudioCodecCtx)
    {
        avcodec_free_context(&vEncoder.pAudioCodecCtx);
    }
    if (vEncoder.pFormatContext!=nullptr)
    {
        avio_close(vEncoder.pFormatContext->pb);
        av_freep(&vEncoder.pFormatContext->streams[0]);
        if (vEncoder.pFormatContext->nb_streams > 1)
        {
            av_freep(&vEncoder.pFormatContext->streams[1]);
        }
        av_free(vEncoder.pFormatContext);
        vEncoder.pFormatContext = nullptr;
    }
    vEncoder.pVideoStream = nullptr;
    vEncoder.pVideoCodec = nullptr;
    vEncoder.pAudioStream = nullptr;
    vEncoder.pAudioCodec = nullptr;
    if (vEncoder.pSws)
    {
        sws_freeContext(vEncoder.pSws);
        vEncoder.pSws = nullptr;
    }
    vEncoder.pOutFormat = nullptr;
    if (vEncoder.pOpts)
    {
        av_dict_free(&vEncoder.pOpts);
        vEncoder.pOpts = nullptr;
    }
    // default fps
    vEncoder.nFPS_num = DEF_FPS_NUM;
    vEncoder.nFPS_den = DEF_FPS_DEN;
    m_nFramesDone = 0;
    return MediaCompletion::Success;
}

MediaCompletion CWriterFFMPEG::startMedia()
{
    ffw_video_encoder_context_t*pCtx = &vEncoder;
    int nRet = avcodec_open2(pCtx->pVideoCodecCtx, pCtx->pVideoCodec, &pCtx->pOpts);

    if ( nRet < 0)
    {
        //ERR("avcodec_open2 [video] FAILED with code %d", nRet);
        return MediaCompletion::InvalidParam;
    }

    if (pCtx->pAudioCodec != nullptr)
    {
        nRet = avcodec_open2(pCtx->pAudioCodecCtx, pCtx->pAudioCodec, nullptr);
        if (nRet < 0)
        {
            //ERR("avcodec_open2 [audio] FAILED with code %d", nRet);
            return MediaCompletion::InvalidParam;
        }
    }
    nRet = avio_open(&pCtx->pFormatContext->pb, pCtx->sFilePath.c_str(), AVIO_FLAG_WRITE);
    if ( nRet < 0)
    {
        //ERR("avio_open FAILED with code %d", nRet);
        return MediaCompletion::InvalidParam;
    }

    // set extradata
    AVStream* pStream = nullptr;

    pStream = pCtx->pFormatContext->streams[0];

    if (pStream)
    {
        if (pStream->codecpar->extradata_size == 0)
        {
            pStream->codecpar->extradata_size = pCtx->pVideoCodecCtx->extradata_size;
            pStream->codecpar->extradata = (uint8_t*)malloc(pCtx->pVideoCodecCtx->extradata_size);
            memcpy(pStream->codecpar->extradata, pCtx->pVideoCodecCtx->extradata, pCtx->pVideoCodecCtx->extradata_size);
        }
        pStream->codecpar->profile = pCtx->pVideoCodecCtx->profile;
        pStream->codecpar->level = pCtx->pVideoCodecCtx->level;
        pStream->codecpar->bit_rate = pCtx->pVideoCodecCtx->bit_rate;
        //pStream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }
    else
    {
        assert(0);
    }
    nRet = avformat_write_header(pCtx->pFormatContext, nullptr);
    if ( nRet < 0)
    {
        //ERR("avformat_write_header FAILED with code %d", nRet);
        return MediaCompletion::InvalidParam;
    }
    return MediaCompletion::Success;
}

MediaCompletion CWriterFFMPEG::init(const char* pcFname, enc_config_t& tConfig)
{
    ffw_video_encoder_context_t* pCtx = &vEncoder;
    char* _pcFname = nullptr;
    _pcFname = (char*)pcFname;
    m_encConfig = tConfig;

    pCtx->pOutFormat = av_guess_format("mp4", nullptr, nullptr);
    // hard coded AVI container
    if (avformat_alloc_output_context2(&pCtx->pFormatContext, pCtx->pOutFormat, "mp4", _pcFname) < 0)
    {
        return MediaCompletion::InvalidParam;
    }
    else
    {
        loadSettings();
        pCtx->sFilePath = _pcFname;
        return MediaCompletion::Success;
    }
}

bool CWriterFFMPEG::loadSettings()
{
    ffw_video_encoder_context_t*pCtx = &vEncoder;
   
    if (pCtx->pVideoCodecCtx)
    {
        // set defaults
        // copy main parameters 
        pCtx->pVideoCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO; 
        pCtx->pVideoCodecCtx->codec_id = AV_CODEC_ID_H264;
        // most codecs use this colorspace
        // TBD check AV_PIX_FMT_NV12
        pCtx->pVideoCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P; 
        pCtx->pVideoCodecCtx->sample_aspect_ratio = av_make_q( 0, 1);
        pCtx->pVideoCodecCtx->thread_count = m_encConfig.nThreads;
        pCtx->pVideoCodecCtx->rc_max_rate = 0;
        pCtx->pVideoCodecCtx->rc_buffer_size = 0;
        pCtx->pVideoCodecCtx->rc_min_rate = 0;
        pCtx->pVideoCodecCtx->me_subpel_quality = 5;
        pCtx->pVideoCodecCtx->flags |= CODEC_FLAG_LOOP_FILTER;
        pCtx->pVideoCodecCtx->flags2 |= CODEC_FLAG2_FAST;
        
        //if (pCtx->pVideoCodec && pCtx->pVideoCodec->long_name && strlen(pCtx->pVideoCodec->long_name))
        //{
        //    pCtx->sVersion = pCtx->pVideoCodec->long_name;
        //}

        pCtx->nFPS_num = DEF_FPS_NUM;
        pCtx->nFPS_den = DEF_FPS_DEN;

        // set x264 options
        // https://trac.ffmpeg.org/wiki/Encode/H.264
        //av_dict_set(&pCtx->pOpts, "tune", "zerolatency", 0);
        //av_dict_set(&pCtx->pOpts, "preset", "fast", 0);
        //av_dict_set(&pCtx->pOpts, "crf", "18", 0);
        //av_dict_set(&pCtx->pOpts, "bitrate", "5000", 0);

        // overridables
        pCtx->pVideoCodecCtx->level = DEF_ENC_LEVEL; //Level 5.1, up to 4K30
        pCtx->pVideoCodecCtx->profile = FF_PROFILE_H264_HIGH;
        pCtx->pVideoCodecCtx->qmin = DEF_ENC_QP_MIN; // the lower - the better
        pCtx->pVideoCodecCtx->qmax = DEF_ENC_QP_MAX;
        pCtx->pVideoCodecCtx->max_qdiff = DEF_ENC_QDIFF; 
        pCtx->pVideoCodecCtx->gop_size = m_encConfig.fps_num; // 0 - intra-only
        pCtx->pVideoCodecCtx->bit_rate = DEF_ENC_BITRATE ; // x264 use kBits
        if (pCtx->pVideoCodecCtx->width == 0)
        {
            pCtx->pVideoCodecCtx->width = DEF_ENC_WIDTH;
        }
        if (pCtx->pVideoCodecCtx->height == 0)
        {
            pCtx->pVideoCodecCtx->height = DEF_ENC_HEIGHT;
        }
        pCtx->pVideoCodecCtx->level = m_encConfig.level;
        pCtx->pVideoCodecCtx->gop_size = m_encConfig.gopLength;
        pCtx->nFPS_num = m_encConfig.fps_num;
        pCtx->nFPS_den = m_encConfig.fps_den;
        pCtx->pVideoCodecCtx->bit_rate = m_encConfig.bitrate;
        pCtx->pVideoCodecCtx->width = m_encConfig.width;
        pCtx->pVideoCodecCtx->height = m_encConfig.height;
        // calculated values
        av_dict_set(&pCtx->pOpts, "vprofile", m_encConfig.sProfile.c_str(), 0); // x264 specific
        if (!strcmpX(m_encConfig.sProfile.c_str(), "high"))
        {
            pCtx->pVideoCodecCtx->profile = FF_PROFILE_H264_HIGH;
        }
        else if (!strcmpX(m_encConfig.sProfile.c_str(), "base"))
        {
            pCtx->pVideoCodecCtx->profile = FF_PROFILE_H264_BASELINE;
        }
        else
        {
            pCtx->pVideoCodecCtx->profile = FF_PROFILE_H264_MAIN;
        }
        pCtx->pVideoCodecCtx->time_base = av_make_q(pCtx->nFPS_den, pCtx->nFPS_num);
        pCtx->pVideoCodecCtx->bit_rate_tolerance = (int)pCtx->pVideoCodecCtx->bit_rate;
        if (m_encConfig.nRC == DEF_ENC_CBR) // 0.0 - cbr, >=1 constant qp
        {
            pCtx->pVideoCodecCtx->qcompress = 0.0f;
        }
        else
        {
            pCtx->pVideoCodecCtx->qcompress = 0.6f; // TBD further calcs
        }
    }
    return true;
}

MediaCompletion CWriterFFMPEG::closeStream(const unsigned int nStreamID)
{
    MediaCompletion eRet = MediaCompletion::Success;

    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = nullptr;
    pkt.size = 0;
    int nDone = 1;

    (void)nStreamID;
    ffw_video_encoder_context_t*pCtx = &vEncoder;

    // fflush encoder
    int nRet = avcodec_send_frame(pCtx->pVideoCodecCtx, nullptr);
    {
        if (nRet >= 0)
        {
            while (nDone)
            {
                nRet = avcodec_receive_packet(pCtx->pVideoCodecCtx, &pkt);
                if (nRet == 0)
                {
                    m_nFramesDone++;
                    av_packet_rescale_ts(&pkt, pCtx->pVideoCodecCtx->time_base, pCtx->pVideoStream->time_base);
                    if (av_interleaved_write_frame(pCtx->pFormatContext, &pkt) != 0)
                    {
                        eRet = MediaCompletion::GeneralError;
                    }
                    else
                    {
                        eRet = MediaCompletion::Success;
                    }
                    av_packet_unref(&pkt);
                    if (eRet == MediaCompletion::GeneralError)
                    {
                        break;
                    }
                }
                else
                {
                    nDone = 0;
                }
            }
        }
    }
    return eRet;
}

MediaCompletion CWriterFFMPEG::stopMedia()
{
    MediaCompletion eRet = MediaCompletion::Success;
    
    ffw_video_encoder_context_t*pCtx = &vEncoder;

    if (pCtx->pFormatContext != nullptr)
    {
        int nStreamID = 0; // use only 1 video stream;
        eRet = closeStream(nStreamID);

        if (pCtx->pFormatContext)
        {
            av_write_trailer(pCtx->pFormatContext);
        }
        //clearContext();
    }
    return eRet;
}

MediaCompletion CWriterFFMPEG::addVideoStream(const video_stream_info_t& sinfo, unsigned int& nStreamID)
{
    ffw_video_encoder_context_t*pCtx = &vEncoder;

    // setup stream
    pCtx->pVideoStream = avformat_new_stream(pCtx->pFormatContext, nullptr);
    if (pCtx->pVideoStream == nullptr)
    {
        return MediaCompletion::InvalidParam;
    }
    // use settings from media file
    pCtx->pVideoStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    pCtx->pVideoStream->codecpar->codec_id = AV_CODEC_ID_H264;
    pCtx->pVideoStream->codecpar->height = sinfo.nHeight;
    pCtx->pVideoStream->codecpar->width = sinfo.nWidth;

    // setup codec
    pCtx->pVideoCodec = avcodec_find_encoder(pCtx->pVideoStream->codecpar->codec_id);
    if (!pCtx->pVideoCodec)
    {
        //ERR("avcodec_find_encoder FAILED with code %d", (int)MediaCompletion::InvalidParam);
        return MediaCompletion::InvalidParam;
    }
    AVCodecParameters* pCodecParam = pCtx->pVideoStream->codecpar;
    pCtx->pVideoCodecCtx = avcodec_alloc_context3(pCtx->pVideoCodec);
    if (avcodec_parameters_to_context(pCtx->pVideoCodecCtx, pCodecParam) != 0)
    {
        return MediaCompletion::InvalidParam;
    }
    pCtx->pVideoCodecCtx->height = sinfo.nHeight;
    pCtx->pVideoCodecCtx->width = sinfo.nWidth;
    pCtx->nFPS_den = sinfo.nFPSden;
    pCtx->nFPS_num = sinfo.nFPSnum;
    loadSettings();
    pCtx->pVideoCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
    nStreamID = 0; // the only streamID
    return MediaCompletion::Success;
}

MediaCompletion CWriterFFMPEG::addVideoFrame(const unsigned int nStreamID, const frame_info_t* pFrame)
{
    MediaCompletion eRet = MediaCompletion::GeneralError;
    (void)nStreamID;
    ffw_video_encoder_context_t* pCtx = &vEncoder;

    if (pFrame == nullptr)
    {
        // EOS
        eRet = MediaCompletion::Success;
        return eRet;
    }

    // step 1 . prepare AVFrame and convert colorspace if needed

    AVFrame *pAVFrame;
    pAVFrame = av_frame_alloc();
    if (pAVFrame == nullptr)
    {
        return eRet;
    }
    pAVFrame->height = pFrame->height;
    pAVFrame->width = pFrame->width;
    pAVFrame->format = AV_PIX_FMT_YUV420P; // X encoder supports only this format
    av_image_fill_arrays(pAVFrame->data, pAVFrame->linesize, nullptr, (AVPixelFormat)pAVFrame->format, pFrame->width, pFrame->height, 1);
    av_image_alloc(pAVFrame->data, pAVFrame->linesize, pFrame->width, pFrame->height, (AVPixelFormat)pAVFrame->format, 1);

    ColorFormat eColor = pFrame->eColor;

    if (eColor != ColorFormat::YUV420P)
    {
        AVPixelFormat format = AV_PIX_FMT_YUV420P;

        if (eColor == ColorFormat::NV12)
        {
            format = AV_PIX_FMT_NV12; 
        }
        else if (eColor == ColorFormat::RGBA)
        {
            format = AV_PIX_FMT_RGBA; 
        }
        else if (eColor == ColorFormat::RGB)
        {
            format = AV_PIX_FMT_RGB24; 
        }
        else
        {
            assert(0);
            //printf("InvalidColor");
            return eRet;
        }

        // TBD opt
        uint8_t*    pdata[AV_NUM_DATA_POINTERS];
        int         linesize[AV_NUM_DATA_POINTERS];

        for (size_t i = 0; i < AV_NUM_DATA_POINTERS; i++)
        {
            pdata[i] = nullptr;
            linesize[i] = 0;
        }

        pdata[0] = pFrame->pFrameData;
        linesize[0] = pFrame->stride;

        // we need colorspace\size conversion
        if (pCtx->pSws == nullptr)
        {
            // init context
            pCtx->pSws = sws_getContext(pFrame->width, pFrame->height, format,
                //pFrame->width, pFrame->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, nullptr, nullptr, nullptr);
                m_encConfig.width, m_encConfig.height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, nullptr, nullptr, nullptr);
            if (pCtx->pSws == nullptr)
            {
                assert(0);
                return eRet;
            }
        }

        unsigned int ret = sws_scale(pCtx->pSws, pdata, linesize, 0, pFrame->height, pAVFrame->data, pAVFrame->linesize);
        if (ret != pFrame->height)
        {
            assert(0);
        }
    }
    else
    {
        int nSize = pAVFrame->linesize[0];
        int nHeight = pFrame->height;
        uint8_t* pSrc = pFrame->pFrameData;
        uint8_t* pDst = pAVFrame->data[0];
        uint8_t* pDstU = pAVFrame->data[1];
        uint8_t* pDstV = pAVFrame->data[2];

        if (pSrc == nullptr)
        {
            return MediaCompletion::InvalidParam;
        }

        // TBD opt
        if (eColor == ColorFormat::RGBA)
        {
            for (int i = 0; i < nHeight; i++)
            {
                memcpy(pDst, pSrc, nSize);
                pSrc += nSize; // pFrame->stride;
                pDst += nSize;
            }
        }
        else // legacy YUV420P
        {
            // Y plane
            for (int i = 0; i < nHeight; i++)
            {
                memcpy(pDst, pSrc, nSize);
                pSrc += nSize; // pFrame->stride;
                pDst += nSize;
            }
            // TBD opt
            // U plane 
            nSize = pAVFrame->linesize[1];
            nHeight >>= 1;
            for (int i = 0; i < nHeight; i++)
            {
                memcpy(pDstU, pSrc, nSize);
                pSrc += nSize; // pFrame->stride;
                pDstU += nSize;
            }
            // V plane
            nSize = pAVFrame->linesize[2];
            for (int i = 0; i < nHeight; i++)
            {
                memcpy(pDstV, pSrc, nSize);
                pSrc += nSize; // pFrame->stride;
                pDstV += nSize;
            }
        }
    }

    // step 2. push to encoder
    pAVFrame->key_frame = 1;
    pAVFrame->coded_picture_number = m_nFramesDone;
    //pAVFrame->pkt_duration = (int64_t)(pCtx->m_nFPS_nom  * H264_TIMEBASE / pCtx->m_nFPS_den);
    //pAVFrame->pts = pAVFrame->coded_picture_number * pAVFrame->pkt_duration;
    pAVFrame->pkt_duration = 0;
    pAVFrame->pts = pAVFrame->coded_picture_number;

    int nDone = 0;
    int nRet = 0;
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = nullptr;
    pkt.size = 0;
    pkt.stream_index = pCtx->pVideoStream->index;

    nRet = avcodec_send_frame(pCtx->pVideoCodecCtx, pAVFrame);
    if (nRet >= 0)
    {
        while (1)
        {
            nRet = avcodec_receive_packet(pCtx->pVideoCodecCtx, &pkt);

            if (nRet == 0)
            {
                nDone = 1;
                m_nFramesDone++;
                eRet = MediaCompletion::Success;
            }
            else if (nRet == AVERROR(EAGAIN))
            {
                eRet = MediaCompletion::Success;
                break;
            }
            else
            {
                eRet = MediaCompletion::EncoderError;
                break;
            }
            if (nDone)
            {
                av_packet_rescale_ts(&pkt, pCtx->pVideoCodecCtx->time_base, pCtx->pVideoStream->time_base);
                if (av_interleaved_write_frame(pCtx->pFormatContext, &pkt) != 0)
                {
                    eRet = MediaCompletion::GeneralError;
                }
                else
                {
                    eRet = MediaCompletion::Success;
                }
                break;
            }
        }
    }
    else
    {
        assert(0);
    }
    // free buffers
    av_freep(&pAVFrame->data[0]);
    av_frame_free(&pAVFrame);
    return eRet;
}



