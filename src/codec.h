#ifndef _CODEC_H_
#define _CODEC_H_

#include "frame_source.h"

enum class eEncoderType
{
    eh264 = 0,
    eh265
};

typedef struct
{
    int         width;
    int         height;
    int         maxWidth;
    int         maxHeight;
    int         fps_num;
    int         fps_den;
    int         bitrate;
    int         vbvMaxBitrate;
    int         vbvSize;
    int         rcMode; // 0 - Constant QP mode, 1 - Variable bitrate mode, 2 - Constant bitrate mode
    int         qp;
    float       i_quant_factor;
    float       b_quant_factor;
    float       i_quant_offset;
    float       b_quant_offset;
    FILE*       fOutput;
    int         invalidateRefFramesEnableFlag;
    int         intraRefreshEnableFlag;
    int         intraRefreshPeriod;
    int         intraRefreshDuration;
    int         deviceType;
    int         startFrameIdx;
    int         endFrameIdx;
    int         gopLength;
    int         numB;
    int         deviceID;
    char*       outputFileName;
    int         encoderProfile;
    int         level;
    std::string sProfile;
    std::string sLevel;

    ColorFormat inputFormat;
    int         nRC; // def 0
    int         nThreads;

} enc_config_t;

enum class MediaCompletion
{
    Success = 0,
    NotReady,
    InvalidParam,
    GeneralError,
    NotFound,
    NotImplemented,
    HWNotFound,
    EOS,
    InvalidStreams,
    InvalidState,
    GetFrameTimeout,
    MuxerError,
    DeMuxerError,
    FrameDropped,
    EncoderError,
    InitNotCompleted,
    OutOfLimits
};

typedef struct
{
    unsigned int    nWidth;         // frame width, pixels
    unsigned int    nHeight;        // frame height, pixels
    unsigned int    nFPSnum;        // fps, high part
    unsigned int    nFPSden;        // fps, low part
    unsigned int    nBitrate;       // encoder's settings, in kBps
    std::string     sCodecString;   // codec version and other text info
} video_stream_info_t;              // video stream info for writer

class CBaseEncoder
{
public:
    CBaseEncoder()
    {
        m_nRawBytesDone = 0;
        m_nCompressedBytesDone = 0;
        m_bSet = false;
        m_nFramesDone = 0;
    }

    virtual MediaCompletion clearContext() = 0;

    virtual MediaCompletion init(const char* pcFname, enc_config_t& tConfig) = 0;

    virtual MediaCompletion addVideoStream(const video_stream_info_t& info, unsigned int& nStreamID) = 0;

    virtual MediaCompletion addVideoFrame(const unsigned int nStreamID, const frame_info_t* pFrame) = 0;

    virtual MediaCompletion startMedia() = 0;

    virtual MediaCompletion stopMedia() = 0;

    virtual MediaCompletion closeStream(const unsigned int nStreamID) = 0;

	virtual const char* getVersionInfo() { return m_sVersionInfo.c_str(); };
	
	virtual bool getStat(size_t& nRawBytes, size_t& nCompressedBytes, size_t& nFramesDone)
	{
		nRawBytes = m_nRawBytesDone;
		nCompressedBytes = m_nCompressedBytesDone;
        nFramesDone = m_nFramesDone;
		return true;
	}

    virtual ~CBaseEncoder()
    {}

protected:

	size_t		m_nRawBytesDone;
	size_t		m_nCompressedBytesDone;
	bool		m_bSet;
	std::string m_sVersionInfo;
	int			m_nFramesDone;
};

//////////////////////////////////////////////////////////////////////////
// video defaults 
// TBD use constants 
// move to encoder specific file

//Format 	                Resolution(px)
//Standard Definition(SD)   4:3 aspect ratio 	    640 × 480
//Standard Definition(SD)   16 : 9 aspect ratio 	640 × 360
//720p HD                   16 : 9 aspect ratio 	1280 × 720
//1080p HD                  16 : 9 aspect ratio 	1920 × 1080
//2K                        16 : 9 aspect ratio 	2560 × 1440
//4K UHD                    16 : 9 aspect ratio 	3840 × 2160
//DCI 4K UHD                17 : 9 aspect ratio 	4096 × 2160
//4K Monoscopic 360         2 : 1 aspect ratio 	    4096 × 2048
//4K Stereoscopic 360       2 : 1 aspect ratio 	    4096 × 2048 
//8K UHD                    17 : 9 aspect ratio 	8192 × 4320

#define DEF_ENC_WIDTH           1920 // 4096   
#define DEF_ENC_WIDTH_MIN       128     
#define DEF_ENC_WIDTH_MAX       4096

#define DEF_ENC_HEIGHT          1080 // 2048
#define DEF_ENC_HEIGHT_MIN      128     
#define DEF_ENC_HEIGHT_MAX      2160

//Quality 	Bit rate(Mbps)
//SD 	    2 – 5
//720p 	    5 – 10
//1080p 	10 – 20
//2K 	    20 – 30
//4K 	    30 – 60
//8K 	    50 – 80

#define DEF_ENC_BITRATE         40000000 
#define DEF_ENC_BITRATE_MIN     10000000
#define DEF_ENC_BITRATE_MAX     80000000

#define DEF_ENC_QP_MAX          28
#define DEF_ENC_QP_MIN          18
#define DEF_ENC_LEVEL           51 // 5.1 or higher
#define DEF_ENC_QDIFF           4
#define DEF_ENC_PROFILE         ("high")
#define DEF_ENC_CBR             0
#define DEF_ENC_VBR             1
#define DEF_ENC_CQP             2

#define H264_TIMEBASE           90000

#define DEF_FPS_NUM             30
#define DEF_FPS_NUM_MAX         120000
#define DEF_FPS_NUM_MIN         1

#define DEF_FPS_DEN             1
#define DEF_FPS_DEN_MAX         1001
#define DEF_FPS_DEN_MIN         DEF_FPS_DEN

#define ENC_PROFILE_H264_BASELINE   66
#define ENC_PROFILE_H264_MAIN       77
#define ENC_PROFILE_H264_HIGH       100

#define ENC_DEFAULT_I_QFACTOR -0.8f
#define ENC_DEFAULT_B_QFACTOR 1.25f
#define ENC_DEFAULT_I_QOFFSET 0.f
#define ENC_DEFAULT_B_QOFFSET 1.25f

// rc modes
#define MIO_RC_CQP 0            // 0 - Constant QP mode 
#define MIO_RC_VBR 1            // 1 - Variable bitrate mode
#define MIO_RC_CBR 2            // 2 - Constant bitrate mode

#define ENC_IDR_PERIOD          10 // key frame

#endif // _CODEC_H_
