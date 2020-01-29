#if defined(_MSC_VER)
#include <conio.h>
#endif

#include <iostream>
#include <string>
#include <vector>
#include <set>

#include "frame_source.h"
#include "enc_ffmpeg.h"

bool isVerbose(); // TBD

bool processFiles(const std::vector<std::string>& vFiles, const std::string& sOutName, enc_config_t& tConfig)
{
    if (isVerbose())
    {
        std::cout << "Start pre-processing!" << std::endl;
    }

    CDCMFrameSource src;

    frame_info_t info;
    std::set<int> nsWidth;
    std::set<int> nsHeight;
    std::vector<std::string> vGoodFiles; // TBD ref
    int nFramesCounter = 0;

    for (auto& it : vFiles)
    {
        memset(&info, 0, sizeof(frame_info_t));
        if (src.load(it.c_str()))
        {
            while (src.getNextImage(true, info))
            {
                nsWidth.insert(info.width);
                nsHeight.insert(info.width);
                vGoodFiles.push_back(it);
            }
        }
    }

    if (isVerbose())
    {
        std::cout << "Pre-processing done :" << vGoodFiles.size() << " files." << std::endl;
    }
    if (vGoodFiles.empty())
    {
        return false;
    }

    CWriterFFMPEG writer;
    if (isVerbose())
    {
        std::cout << "Start processing!" << std::endl;
    }

    tConfig.maxWidth = tConfig.width = *(nsWidth.rbegin());
    tConfig.maxHeight = tConfig.height = *(nsHeight.rbegin());

    if (isVerbose())
    {
        std::cout << "Video resolution : " << tConfig.maxWidth << "x" << tConfig.maxWidth << ", fps : " << tConfig.fps_num <<std::endl;
    }

    if (writer.init(sOutName.c_str(), tConfig) != MediaCompletion::Success)
    {
        if (isVerbose())
        {
            std::cout << "FAILED to init writer for " << sOutName << std::endl;
        }
        return false;
    }

    video_stream_info_t vstream;
    vstream.nWidth = tConfig.maxWidth;
    vstream.nHeight = tConfig.maxHeight;
    vstream.nBitrate = tConfig.bitrate;
    vstream.nFPSnum = tConfig.fps_num;
    vstream.nFPSden = tConfig.fps_den;
    unsigned int nStreamID = 0;

    if (writer.addVideoStream(vstream, nStreamID) != MediaCompletion::Success)
    {
        if (isVerbose())
        {
            std::cout << "addVideoStream FAILED  for " << sOutName << std::endl;
        }
        return false;
    }

    if (writer.startMedia() == MediaCompletion::Success)
    {
        for (auto& it : vGoodFiles)
        {
            frame_info_t frame;

            if (src.load(it.c_str()))
            {
                while (src.getNextImage(false, frame))
                {
                    frame.nFrameIdx = nFramesCounter;
                    frame.pts = nFramesCounter++;
                    if (writer.addVideoFrame(nStreamID, &frame) != MediaCompletion::Success)
                    {
                        if (isVerbose())
                        {
                            std::cout << "addVideoFrame FAILED  for " << it << std::endl;
                        }
                    }
                    src.releaseFrame(frame);
                }
            }
        }
        
        writer.stopMedia();

        size_t nTotalBytes = 0;
        size_t nCompressedBytes = 0;
        size_t nTotalFrames = 0;
        writer.getStat(nTotalBytes, nCompressedBytes, nTotalFrames);
        if (isVerbose())
        {
            std::cout << "Processing done :" << nTotalFrames << " files." << std::endl;
        }
    }
    else
    {
        if (isVerbose())
        {
            std::cout << "startMedia FAILED  for " << sOutName << std::endl;
        }
    }

    return true;
}

