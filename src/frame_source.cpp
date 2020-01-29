#include "frame_source.h"

CDCMFrameSource::CDCMFrameSource( const char* pName/* = nullptr*/)
{
    DJDecoderRegistration::registerCodecs();
    m_Img = nullptr;
    load(pName);
}

CDCMFrameSource::~CDCMFrameSource()
{
    close();
    DJDecoderRegistration::cleanup();
}

bool CDCMFrameSource::isOk()
{
    return (!m_dff.getDataset()->isEmpty());
}

bool CDCMFrameSource::close()
{
    m_dff.clear();
    if (m_Img != nullptr)
    {
        delete m_Img;
        m_Img = nullptr;
    }
    return true;
}

bool CDCMFrameSource::releaseFrame(frame_info_t& tInfo) // TBD REF
{
    if (tInfo.pFrameData != nullptr)
    {
        free(tInfo.pFrameData);
    }
    return true;
}

bool CDCMFrameSource::getImage(bool bInfoOnly, frame_info_t& tInfo, int nImageIdx)
{
    bool bRet = false;
    tInfo.pFrameData = nullptr;

    DcmDataset *dataset = m_dff.getDataset();

    long int nVal = 0;
    dataset->findAndGetLongInt(DCM_Rows, nVal);
    tInfo.height = nVal;
    dataset->findAndGetLongInt(DCM_Columns, nVal);
    tInfo.width = nVal;
    dataset->findAndGetLongInt(DCM_BitsAllocated, nVal);
    tInfo.nBits = nVal;
    tInfo.nFlags = 0;
    tInfo.eColor = ColorFormat::RGBA;
    tInfo.duration = 0; // use from settings, if any
    if (dataset->findAndGetLongInt(DCM_ActualFrameDuration, nVal).good()) // in ms
    {
        tInfo.duration = nVal;
    }
    tInfo.pts = 0;
    long int nTotalFrames = 0;
    dataset->findAndGetLongInt(DCM_NumberOfFrames, nTotalFrames);
    if (nTotalFrames == 0)
    {
        nTotalFrames = 1;
    }

    if (tInfo.height > 0 && tInfo.width > 0 && nTotalFrames > nImageIdx)
    {
        bRet = true;
    }

    if (!bInfoOnly)
    {
        if (m_Img == nullptr)
        {
            m_Img = new DicomImage( m_sFilename.c_str());
        }
        if (m_Img != nullptr)
        {
            void* pImgData = nullptr;
            int nLen = m_Img->createWindowsDIB(pImgData, nImageIdx);
            if (nLen == 0)
            {
                bRet = false;
            }
            else
            {
                tInfo.nBits = 24;
                tInfo.width = m_Img->getWidth();
                tInfo.height = m_Img->getHeight();
                tInfo.pFrameData = (unsigned char*)pImgData;
                tInfo.eColor = ColorFormat::RGB;
                tInfo.stride = (tInfo.width * 3 + 3) & ~(3);
            }
        }
    }
    return bRet;
}

bool CDCMFrameSource::getNextImage(bool bInfoOnly, frame_info_t& tInfo)
{
    return getImage(bInfoOnly, tInfo, m_nImageIdx++);
}

bool CDCMFrameSource::load(const char* pName)
{
    if (pName == nullptr)
    {
        return false;
    }
    m_nImageIdx = 0;
    m_sFilename = pName;
    if (m_Img != nullptr)
    {
        delete m_Img;
        m_Img = nullptr;
    }
    return m_dff.loadFile(pName).good();
}




