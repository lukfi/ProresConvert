#include <cstdio>
#include <iostream>
#include <algorithm>
#include "fs/file.h"
#include "tiny-process-library/process.hpp"
#include <sstream>
#include "utils/utfconverter.h"
#include "utils/stringutils.h"
#include "utils/SimpleJSON/json.hpp"
#include "regexp/regexp.h"
#include <signal.h>

#define ENABLE_SDEBUG
#include "utils/screenlogger.h"

#define VER_MAJ 0
#define VER_MIN 1

static const std::wstring COMMAND = L" -c:v prores_ks -profile:v 3 -vendor apl0 -bits_per_mb 500 -pix_fmt yuv422p10le ";

class Process : public TinyProcessLib::Process
{
public:
    Process(const string_type& command, const string_type& path = string_type(),
        std::function<void(const char* bytes, size_t n)> read_stdout = nullptr,
        std::function<void(const char* bytes, size_t n)> read_stderr = nullptr) :
        TinyProcessLib::Process(command, path, read_stdout, read_stderr)
    {

    }
    virtual ~Process()
    {
        //SDEB("Killing process");
        //kill();
        //std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    }
};

std::string GetExtension(const std::string& name)
{
    // Find the position of the last dot in the file name
    size_t dotPosition = name.find_last_of('.');

    // Check if a dot is found and it's not the last character
    if (dotPosition != std::string::npos && dotPosition < name.length() - 1) 
    {
        // Extract the substring after the dot and convert it to lowercase
        std::string extension = name.substr(dotPosition + 1);
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

        return extension;
    }

    // If no dot or it's the last character, return an empty string (no extension)
    return "";
}
static std::weak_ptr<Process> gCurrentProcess;
std::mutex gProcessCreationMutex;

void SignalHandler(int signal)
{
    if (signal == SIGBREAK)
    {
        std::lock_guard<std::mutex> g(gProcessCreationMutex);
        std::shared_ptr<Process> p = gCurrentProcess.lock();
        if (p)
        {
            p->kill();
            SINFO("Process killed with status: %d", p->get_exit_status());
        }
    }
}

std::string StripExtension(const std::string& name)
{
    size_t dotPosition = name.find_last_of('.');
    return name.substr(0, dotPosition);
}

struct VideoFileInfo
{
    VideoFileInfo()
        : ok(false) {}
    bool ok;
    std::string filename;
    std::wstring outputFilename;
    std::string codec;
    int frames;
    int duration;
};

std::wstring GetDestinationFileName(std::string sourceFileName, const std::string& extension)
{
    auto baseFileName = StripExtension(sourceFileName) + "_converted";
    auto testFileName = baseFileName + "." + extension;
    int counter = 0;
    while (LF::fs::File::Exists(testFileName))
    {
        //SDEB("File %s exists: %d", testFileName.c_str(), LF::fs::File::Exists(testFileName));
        testFileName = baseFileName + "_" + std::to_string(++counter) + "." + extension;
    }
    //SDEB("File %s exists: %d", testFileName.c_str(), LF::fs::File::Exists(testFileName));
    return LF::utils::FromUtf8(testFileName);
}

std::list<VideoFileInfo> GetFilesWithExtensions(std::list<std::string>& validExtensions)
{
    std::list<VideoFileInfo> foundFiles;

    LF::fs::Directory d;
    //std::cout << d.PWD() << std::endl;
    auto files = d.GetFiles();


    auto extensionMach = [&](const std::string& fileName) -> bool
    {
        std::string extension = GetExtension(fileName);
        // Iterate through the list to check if the target string exists
        for (auto& e : validExtensions)
        {
            if (e == extension)
            {
                return true;  // String found in the list
            }
        }
        return false;  // String not found in the list
    };

    for (auto f : files)
    {
        if (extensionMach(f.Name))
        {
            VideoFileInfo info;
            info.filename = f.Name;
            foundFiles.push_back(info);
        }
    }

    return foundFiles;
}

void UpdateFileInfo(std::list<VideoFileInfo>& infos)
{
    for (auto& info : infos)
    {
        int exit_status;

        std::string fileInfoJson;
        std::wstringstream wss;
        wss << L"ffprobe.exe " << LF::utils::FromUtf8(info.filename) << L" -print_format json -v quiet -show_format -show_streams";
        //std::wcout << wss.str() << std::endl;
        Process process1(wss.str(), L"",
            [&](const char* bytes, size_t n) { fileInfoJson += std::string(bytes, n); },
            [](const char* bytes, size_t n) { std::cout << "Output from stderr: " << std::string(bytes, n); });
        while (!process1.try_get_exit_status(exit_status))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (exit_status == 0)
        {
            json::JSON j = json::JSON::Load(fileInfoJson);
            if (j.hasKey("streams"))
            {
                info.ok = true;
                if (info.ok && !j.at("streams").at(0).at("codec_name").IsNull())
                {
                    info.codec = j.at("streams").at(0).at("codec_name").ToString();
                }
                else
                {
                    info.ok = false;
                }
                if (info.ok && !j.at("streams").at(0).at("nb_frames").IsNull())
                {
                    info.ok = LF::utils::s2dec(j.at("streams").at(0).at("nb_frames").ToString(),
                        info.frames);
                    info.ok = info.ok && (info.frames > 0);
                }
                else
                {
                    info.ok = false;
                }
                if (info.ok && !j.at("streams").at(0).at("duration").IsNull())
                {
                    // TODO
                }
                else
                {
                    info.ok = false;
                }
            }
            //SINFO("File info:\ncodec: %s\nframes: %d", info.codec.c_str(), info.frames);
        }
    }
}

int main()
{
    SINFO("version: %d.%d\n", VER_MAJ, VER_MIN);
    signal(SIGBREAK, SignalHandler);

    std::list<std::string> validExtensions = { {"mp4"} };
    std::list<VideoFileInfo> filesToConvert = GetFilesWithExtensions(validExtensions);
    UpdateFileInfo(filesToConvert);

    if (filesToConvert.size())
    {
        PRINT("Files to convert:\n");
        for (auto f : filesToConvert)
        {
            PRINT("  * %s (%s)\n", f.filename.c_str(), f.codec.c_str());
        }
    }
    else
    {
        SWARN("There are no files to convert. exiting...");
    }

    PRINT("\n");

    int convertCount = 0;
    int successfullyConvertedFiles = 0;
    for (auto& info : filesToConvert)
    {
        ++convertCount;
        if (info.ok)
        {
            int exit_status;

            std::wstringstream wss2;
            info.outputFilename = GetDestinationFileName(info.filename, "mov");
            wss2 << L"ffmpeg -i " << LF::utils::FromUtf8(info.filename) << COMMAND << info.outputFilename;
            //std::wcout << wss2.str() << std::endl;

            SINFO("Converting [%d/%d]: %s -> %s", convertCount, filesToConvert.size(), info.filename.c_str(), LF::utils::ToUtf8(info.outputFilename).c_str());
            bool firstPercentInfoPrinted = false;
            std::shared_ptr<Process> process2;
            {
                std::lock_guard<std::mutex> g(gProcessCreationMutex);
                process2 =
                    std::make_shared<Process>(wss2.str(), L"",
                        [](const char* bytes, size_t n) { std::cout << "Output from stdout: " << std::string(bytes, n); },
                        [&](const char* bytes, size_t n) { //std::cout << "Output from stderr: " << std::string(bytes, n); 
                            std::string line = std::string(bytes, n);
                            LF::re::RegExp r("frame= *([0-9]*)");
                            r.IndexIn(line);
                            if (r.MatchedLength())
                            {
                                firstPercentInfoPrinted = true;
                                int frames = 0;
                                LF::utils::s2dec(r.Cap(1), frames);
                                PRINT("\r%3d", frames * 100 / info.frames); std::cout << "%";
                            }
                            else if (!firstPercentInfoPrinted)
                            {
                                PRINT("\r...")
                            }
                        });
                gCurrentProcess = process2;
            }

            while (!process2->try_get_exit_status(exit_status))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            PRINT("\n");
            if (exit_status == 0)
            {
                SINFO("Successfully converted: %s\n", LF::utils::ToUtf8(info.outputFilename).c_str());
                ++successfullyConvertedFiles;
            }
            else
            {
                SWARN("Failed to convert: %s\n", LF::utils::ToUtf8(info.outputFilename).c_str());
            }
        }
    }
    if (successfullyConvertedFiles)
    {
        SUCC("Successfully converted %d file%s", successfullyConvertedFiles, successfullyConvertedFiles > 1 ? "s" : "");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
}
