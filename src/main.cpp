#if defined(_MSC_VER)
#include <conio.h>
#endif

#define VERSION_STRING "v0.3a "

//#include <sys/time.h>
#include <iostream>
#include <vector>
#include <thread>
#include <string>
#include <random>
#include <ctime>
#include <exception>

#include <json/reader.h>
#include <json/writer.h>

#include "codec.h"

#include "dcmtk/dcmdata/dctk.h"
#include "dcmtk/dcmdata/dcuid.h"
#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmimgle/diutils.h"
#include "dcmtk/dcmjpeg/djencode.h" 

#if defined(WIN32) || defined(_WIN32) || defined(WIN64)
#include <windows.h>
#include <tchar.h> 
#include <stdio.h>
#include <strsafe.h>
#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif
#ifndef STRCASECMP
#define STRCASECMP  _stricmp
#endif
#ifndef STRNCASECMP
#define STRNCASECMP _strnicmp
#endif
#else
#include <string.h>
#include <strings.h>
#include <dirent.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef STRCASECMP
#define STRCASECMP  strcasecmp
#endif
#ifndef STRNCASECMP
#define STRNCASECMP strncasecmp
#endif
#endif

int             g_nVerbose = 0;
std::string     g_sConfig; // json for parsing
std::string     g_sInput;
std::string     g_sOutput;
enc_config_t    g_tConfig;
int             g_nWidth = 0;
int             g_nHeight = 0;
int             g_nDuration = 0;
int             g_nFPS = DEF_FPS_NUM;
int             g_nThreads = 0; // auto
int             g_nQuality = 50; // good
eEncoderType    g_eEncoder = eEncoderType::eh264; // default
std::vector<std::string> g_vFiles;

#include "codec.h"

bool processFiles(const std::vector<std::string>& vFiles, const std::string& sOutName, enc_config_t& tConfig);

bool isVerbose()
{
    return (g_nVerbose > 0);
}

//////////////////////////////////////////////////////////////////////////
// cli stuff and helpers

inline int stringRemoveDelimiter(char delimiter, const char *string)
{
    int string_start = 0;

    while (string[string_start] == delimiter)
    {
        string_start++;
    }

    if (string_start >= (int)strlen(string) - 1)
    {
        return 0;
    }
    return string_start;
}

inline int checkCmdLineFlag(const int argc, const char **argv, const char *string_ref)
{
    bool bFound = false;

    if (argc >= 1)
    {
        for (int i = 1; i < argc; i++)
        {
            int string_start = stringRemoveDelimiter('-', argv[i]);
            const char *string_argv = &argv[i][string_start];

            const char *equal_pos = strchr(string_argv, '=');
            int argv_length = (int)(equal_pos == 0 ? strlen(string_argv) : equal_pos - string_argv);

            int length = (int)strlen(string_ref);

            if (length == argv_length && !STRNCASECMP(string_argv, string_ref, length))
            {

                bFound = true;
                continue;
            }
        }
    }
    return (int)bFound;
}

inline bool getCmdLineArgumentString(const int argc, const char **argv, const char *string_ref, char **string_retval)
{
    bool bFound = false;

    if (argc >= 1)
    {
        for (int i = 1; i < argc; i++)
        {
            int string_start = stringRemoveDelimiter('-', argv[i]);
            char *string_argv = (char *)&argv[i][string_start];
            int length = (int)strlen(string_ref);

            if (!STRNCASECMP(string_argv, string_ref, length))
            {
                *string_retval = &string_argv[length + 1];
                bFound = true;
                continue;
            }
        }
    }

    if (!bFound)
    {
        *string_retval = nullptr;
    }
    return bFound;
}

inline bool getCmdLineArgumentList(const int argc, const char **argv, std::vector<std::string>& vItems)
{
    bool bFound = false;

    if (argc >= 1)
    {
        for (int i = 1; i < argc; i++)
        {
            if (argv[i][0] != '-')
            {
                vItems.push_back(argv[i]);
                bFound = true;
            }
        }
    }
    return bFound;
}

inline int getCmdLineArgumentInt(const int argc, const char **argv, const char *string_ref)
{
    bool bFound = false;
    int value = -1;

    if (argc >= 1)
    {
        for (int i = 1; i < argc; i++)
        {
            int string_start = stringRemoveDelimiter('-', argv[i]);
            const char *string_argv = &argv[i][string_start];
            int length = (int)strlen(string_ref);

            if (!STRNCASECMP(string_argv, string_ref, length))
            {
                if (length + 1 <= (int)strlen(string_argv))
                {
                    int auto_inc = (string_argv[length] == '=') ? 1 : 0;
                    value = atoi(&string_argv[length + auto_inc]);
                }
                else
                {
                    value = 0;
                }

                bFound = true;
                continue;
            }
        }
    }

    if (bFound)
    {
        return value;
    }
    else
    {
        return 0;
    }
}

//////////////////////////////////////////////////////////////////////////
// info

void print_usage()
{
    std::cout << "usage : dcm2v [options] [file1] ... [fileN]" << std::endl;
    std::cout << "\t-v=on               - verbose" << std::endl;
    std::cout << "\t-c=dcm2v.json       - configuration file ( optional)" << std::endl;
    std::cout << "\t-i=inputFolder      - input folder ( overrides files given in command line)" << std::endl;
    std::cout << "\t-o=outputFilename   - output file ( default ./dcm2v_out.mp4" << std::endl;
    std::cout << "\t-e=h264             - encoder, h264 ( default) or h265" << std::endl;
    std::cout << "\t-q=1..100           - compression level ( 100 - lossless)" << std::endl;
    std::cout << "\t-f=30               - FPS , if 0 try to guess from input data, default value is 30" << std::endl;
    std::cout << "\t-d=frameDurationMs  - exposition time in milliseconds" << std::endl;
    std::cout << "\t-w=W                - enforce width W" << std::endl;
    std::cout << "\t-h=H                - enforce height H" << std::endl;
    std::cout << "\t-t=T                - threads, default 0 (auto)" << std::endl;
}

//////////////////////////////////////////////////////////////////////////

bool check_params(int argc, char* argv[])
{
    if (argc < 2)
    {
        print_usage();
        return false;
    }
    return true;
}

void get_params(int argc, char* argv[])
{
    if (checkCmdLineFlag(argc, (const char **)argv, "v"))
    {
        g_nVerbose = 1;
    }
	if (checkCmdLineFlag(argc, (const char **)argv, "c"))
	{
		char *temp = nullptr;
		getCmdLineArgumentString(argc, (const char **)argv, "c", &temp);
		g_sConfig = temp;
	}
    if (checkCmdLineFlag(argc, (const char **)argv, "i"))
    {
        char *temp = nullptr;
        getCmdLineArgumentString(argc, (const char **)argv, "i", &temp);
        g_sInput = temp;
    }
    if (checkCmdLineFlag(argc, (const char **)argv, "o"))
    {
        char *temp = nullptr;
        getCmdLineArgumentString(argc, (const char **)argv, "o", &temp);
        g_sOutput = temp;
    }
    if (checkCmdLineFlag(argc, (const char **)argv, "w"))
    {
        g_nWidth = getCmdLineArgumentInt(argc, (const char **)argv, "w");
    }
    if (checkCmdLineFlag(argc, (const char **)argv, "h"))
    {
        g_nHeight = getCmdLineArgumentInt(argc, (const char **)argv, "h");
    }
    if (checkCmdLineFlag(argc, (const char **)argv, "t"))
    {
        g_nThreads = getCmdLineArgumentInt(argc, (const char **)argv, "t");
    }
    if (checkCmdLineFlag(argc, (const char **)argv, "e"))
    {
        int nVal = getCmdLineArgumentInt(argc, (const char **)argv, "e");
        if (nVal == 0)
        {
            g_eEncoder = eEncoderType::eh264;
        }
        else
        {
            g_eEncoder = eEncoderType::eh265;
        }
    }
    if (checkCmdLineFlag(argc, (const char **)argv, "q"))
    {
        g_nQuality = getCmdLineArgumentInt(argc, (const char **)argv, "q");
    }

    // load files
    getCmdLineArgumentList(argc, (const char **)argv, g_vFiles);
}

#if defined(WIN32)
void read_directory(const std::string& name, std::vector<std::string>& v)
{
    std::string pattern(name);
    pattern.append("/*");
    WIN32_FIND_DATA data;
    HANDLE hFind;

    std::wstring ws;
    ws.assign(pattern.begin(), pattern.end());

    if ((hFind = FindFirstFile(ws.c_str(), &data)) != INVALID_HANDLE_VALUE) 
    {
        do 
        {
            if (!(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))
            {
                ws = data.cFileName;
                std::string fname(ws.begin(), ws.end());

				std::string fullname = name;
				fullname += "/";
				fullname += fname;
                v.push_back(fullname);
            }
        } 
        while (FindNextFile(hFind, &data) != 0);
        FindClose(hFind);
    }
}
#else
void read_directory(const std::string& name, std::vector<std::string>& v)
{
	DIR*			dirp;
	struct dirent*	directory;

	dirp = opendir(name.c_str());
	if (dirp)
	{
		while ((directory = readdir(dirp)) != nullptr)
		{
            std::string fname = directory->d_name;

            if(!strcmp(fname.c_str(), "."))
            {
                continue;
            }

            if (!strcmp(fname.c_str(), ".."))
            {
                continue;
            }

            if(!strcmp(fname.c_str(), ".DS_Store")) // mac specific  
            {
                continue;
            }

            //printf("check > %s \r\n", fname.c_str());

            struct stat myStat;
            if ((stat(fname.c_str(), &myStat) == 0) && (((myStat.st_mode) & S_IFMT) == S_IFDIR)) 
            {
                continue;
            }
            else
            { 
                std::string fullname = name;
                fullname += "/";
                fullname += fname;
                v.push_back(fullname);
            }
		}
		closedir(dirp);
	}
}
#endif

//////////////////////////////////////////////////////////////////////////

void loadConfig(std::string& sPath)
{
    std::ifstream file_id;

    // set encoder's defaults
    g_tConfig.fps_den = DEF_FPS_DEN;
    g_tConfig.fps_num = g_nFPS;
    g_tConfig.width = g_tConfig.maxWidth = g_nWidth;
    g_tConfig.height = g_tConfig.maxHeight = g_nHeight;
    g_tConfig.fOutput = nullptr;
    g_tConfig.level = DEF_ENC_LEVEL;
    g_tConfig.sProfile = DEF_ENC_PROFILE;
    g_tConfig.nThreads = g_nThreads;

    file_id.open(sPath.c_str(), std::ifstream::binary);
    if (file_id.is_open())
    {
        Json::Value root;
        Json::CharReaderBuilder rbuilder;
        std::string errs;

        bool parsingSuccessful = Json::parseFromStream(rbuilder, file_id, &root, &errs);
        if (parsingSuccessful)
        {
            try
            {
                if (root.isMember("source"))
                {
                    g_sInput = root["source"].asString();
                }

                if (root.isMember("destination"))
                {
                    g_sOutput = root["destination"].asString();
                }
                if (root.isMember("fps"))
                {
                    g_nFPS = root["fps"].asInt();
                }
                if (root.isMember("frameduration"))
                {
                    g_nDuration = root["frameduration"].asInt();
                }
                if (root.isMember("width"))
                {
                    g_nWidth = root["width"].asInt();
                }
                if (root.isMember("height"))
                {
                    g_nHeight = root["height"].asInt();
                }
                if (root.isMember("threads"))
                {
                    g_nThreads = root["threads"].asInt();
                }
                if (root.isMember("quality"))
                {
                    g_nQuality = root["quality"].asInt();
                }
                if (root.isMember("encoder"))
                {
                    if (root["encoder"].asString() == "h264")
                    {
                        g_eEncoder = eEncoderType::eh264;
                    }
                    else
                    {
                        g_eEncoder = eEncoderType::eh265;
                    }
                }
            }
            catch (const std::exception& e)
            {
                if (isVerbose())
                {
                    std::cout << "Something went wrong : " << e.what() << std::endl;
                }
            }
        }
    }
}

int main(int argc, char* argv[])
{
    if (!check_params( argc, argv))
    {
        return -1;
    }

    get_params(argc, argv);

    if (isVerbose())
    {
        std::cout << "dcm2v " << VERSION_STRING << "built : " << BUILDDATE << std::endl;
    }

    if (g_sConfig.empty())
    {
        g_sConfig = "./dcm2v.json";
        if (isVerbose())
        {
            std::cout << "using default config depdf.json" << std::endl;
        }
    }

    // step 1. Get config
    loadConfig(g_sConfig);
    if (isVerbose())
    {
        //std::cout << "DPI : " << g_nDPI << ", rectangles : " << g_vTransforms.size() << ", tags to transform : " << g_vTags.size() << std::endl;
    }

    if (g_sOutput.empty())
    {
        g_sOutput = "./dcm2v_output.mp4";
    }

    // step 2. Get files list
    if (!g_sInput.empty())
    {
        if (isVerbose())
        {
            std::cout << "Load directory : " << g_sInput << std::endl;
        }
        read_directory(g_sInput, g_vFiles);
    }
    if (g_vFiles.empty())
    {
        if (isVerbose())
        {
            std::cout << "No files found!" << std::endl;
        }
        return -1;
    }

    DJEncoderRegistration::registerCodecs();

    bool bRet = processFiles(g_vFiles, g_sOutput, g_tConfig);

    // print stat
    if (isVerbose())
    {
        std::cout << "Total files done : " << g_vFiles.size() << std::endl;
        if (!bRet)
        {
            // extra stat
        }
    }

    DJEncoderRegistration::cleanup(); 
    return 0;
}

