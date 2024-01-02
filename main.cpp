#include <cstdio>
#include <iostream>
#include <algorithm>
#include "fs/directory.h"

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

int main()
{
    std::string validExtensions[] = {"mp4"};
    LF::fs::Directory d;
    std::cout << d.PWD() << std::endl;
    auto files = d.GetFiles();


    auto extensionMach = [&](const std::string& fileName) -> bool
    {
        std::string extension = GetExtension(fileName);
        // Iterate through the list to check if the target string exists
        for (int i = 0; i < sizeof(validExtensions) / sizeof(validExtensions[0]); ++i)
        {
            if (validExtensions[i] == extension)
            {
                return true;  // String found in the list
            }
        }
        return false;  // String not found in the list
    };

    std::list<std::string> filesToConvert;
    for (auto f : files)
    {
        if (extensionMach(f.Name))
        {
            std::cout << "\t" << f.Name << std::endl;
            filesToConvert.push_back(f.Name);
        }
    }

}