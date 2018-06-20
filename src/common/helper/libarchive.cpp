#include "libarchive.h"

#include <vector>
#include <string>
#include <ctype.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <fcntl.h>
#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>

namespace Helper {

const int NO_OF_FILES_FIELD_SIZE = 2;

LibArchive::LibArchive(const char* archPath) : m_filePath(archPath)
{

}

bool LibArchive::createArch(const char* directoryPath)
{
    if (!m_filePath) return false;

    std::ofstream outputFile;
    outputFile.exceptions(std::ofstream::failbit | std::ofstream::badbit);
    outputFile.open(m_filePath, std::ofstream::out | std::ofstream::binary);
    if (!outputFile.is_open()) return false;

    std::vector<std::pair <uint32_t, std::string> > file_index;
    boost::filesystem::path path(directoryPath);
    boost::filesystem::recursive_directory_iterator directory(path), end;

    // preapre header
    struct stat st = {};
    uint32_t offset = 0;
    unsigned int headerLength = NO_OF_FILES_FIELD_SIZE; // no of files on 2 bytes
    while (directory != end)
    {
        if (boost::filesystem::is_regular_file(*directory))
        {
            std::string entryPath(directory->path().string());
            file_index.push_back(std::make_pair(offset, entryPath));
            stat(entryPath.c_str(), &st);
            offset += st.st_size;

            headerLength += sizeof(uint32_t);
            headerLength += entryPath.length();
            headerLength += 1; // null terminated string
        }
        ++directory;
    }

    try
    {
        // write header
        uint16_t files = file_index.size();
        outputFile.write((char*)&files, NO_OF_FILES_FIELD_SIZE);
        for (auto it = file_index.begin(); it != file_index.end(); ++it)
        {
            offset = (*it).first + headerLength;
            outputFile.write((char*)&offset, sizeof(uint32_t));
            outputFile << (*it).second << "\n";
        }

        // put files
        std::ifstream entry;
        entry.exceptions(std::ifstream::failbit | std::ifstream::badbit | std::ifstream::eofbit);
        for (auto it = file_index.begin(); it != file_index.end(); ++it)
        {
            entry.open((*it).second, std::ifstream::in | std::ifstream::binary);
            outputFile << entry.rdbuf();
            entry.close();
        }

        outputFile.close();

    } catch (std::exception&)
    {
        outputFile.close();
        boost::filesystem::remove(m_filePath);
        return false;
    }

    return true;
}

bool LibArchive::getFileHandle(const char* filepath, int *file_descriptor, uint64_t *offset, uint64_t *file_size)
{

    uint16_t no_of_files;
    uint32_t item_pos, next_item_pos;
    std::string item_name;
    bool found = false;
    std::ifstream arch(m_filePath, std::iostream::in | std::iostream::binary);
    if (arch.is_open())
    {
        arch.read((char*)&no_of_files, NO_OF_FILES_FIELD_SIZE);
        while(arch && no_of_files)
        {

            arch.read((char*)&item_pos, sizeof(uint32_t));
            std::getline(arch, item_name);
            if (item_name.compare(filepath) == 0)
            {
                if (no_of_files > 1)
                {
                    arch.read((char*)&next_item_pos, sizeof(uint32_t));
                } else
                {
                    arch.seekg(0, std::ios_base::end);
                    next_item_pos = arch.tellg();
                }
                *file_size = (next_item_pos - item_pos);
                *offset = item_pos;
                found = true;
                break;
            }
            --no_of_files;
        }
        arch.close();
    }

    if (found)
    {
        int fd = open(m_filePath, O_RDONLY);
        if (fd <= 0) return false;
        *file_descriptor = fd;
    }

    return true;
}

}
