#ifndef _FRAME_SOURCE_H_
#define _FRAME_SOURCE_H_

#ifdef __linux__

#include <arpa/inet.h>
typedef long long int timestamp_t;
#else
typedef int64_t timestamp_t;
#endif

#include <string>
#include <vector>

#include "dcmtk/dcmdata/dctk.h"
#include "dcmtk/dcmdata/dcuid.h"
#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmnet/scu.h"
#include "dcmtk/dcmnet/diutil.h"
#include "dcmtk/dcmimgle/diutils.h"
#include "dcmtk/dcmjpeg/djencode.h" 
#include "dcmtk/dcmjpeg/djdecode.h" 
#include "dcmtk/dcmjpeg/djrploss.h" 
#include "dcmtk/dcmjpeg/djrplol.h" 
#include "dcmtk/dcmimgle/dcmimage.h"

enum class ColorFormat
{
    NV12 = 0,
    RGBA,
    RGB,
    YUV420P,
};

typedef struct
{
    unsigned char*      pFrameData;   // 0 - data, 1 - aux. Host memory.
    ColorFormat         eColor;       // color format for frame data
    unsigned int        width;        // frame width
    unsigned int        height;       // frame height
    unsigned int        stride;       // stride (pitch) for frame data
    unsigned int        nBits;        // bits per color, default is 8
    timestamp_t         pts;          // presentation timestamp for the frame, ms
    timestamp_t         duration;     // presentation timestamp for the frame, ms
    uint64_t            nFrameIdx;    // frame index, stream based
    unsigned int        nFlags;       // user-defined flags, bitmask
}frame_info_t;


class CDCMFrameSource
{
public:
    CDCMFrameSource(const char* pName = nullptr);

    virtual bool isOk( );

    virtual bool load( const char* pName);

    virtual bool getImage(bool bInfoOnly, frame_info_t& tInfo, int nImageIdx);
    
    virtual bool getNextImage(bool bInfoOnly, frame_info_t& tInfo);

	virtual bool close();

    virtual bool releaseFrame(frame_info_t& tInfo);

	virtual ~CDCMFrameSource();

protected:

    DcmFileFormat   m_dff;
    DicomImage*     m_Img;
    int             m_nImageIdx;
    std::string     m_sFilename;
};

#endif // _FRAME_SOURCE_H_
