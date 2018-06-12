#include "tarcompressor.h"

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>

namespace Helper {

TarCompressor::TarCompressor(const std::string &archiveFilePath)
    : m_archiveFilePath(archiveFilePath) {
}


// using linux tar
bool TarCompressor::compressDirectory(const char *directoryPath, const char* relativePath) {
    if (!boost::filesystem::is_directory(directoryPath)) {
        return false;
    }

    std::stringstream ss{};
    ss <<"tar -cSzf "<< m_archiveFilePath;
    if (relativePath) {
        ss << " -C " << relativePath;
    }
    ss << " " << directoryPath;
    int rv = system(ss.str().c_str());
    if (rv < 0) {
        return false;
    }
    return true;
}

bool TarCompressor::decompressDirectory(const char* newDirectoryPath) {
    try {
        if (!boost::filesystem::exists(newDirectoryPath)) {
            boost::filesystem::create_directories(newDirectoryPath);
        } else if (!boost::filesystem::is_directory(newDirectoryPath)) {
            return false;
        }
    } catch (std::exception&) {
        return false;
    }

    std::stringstream ss{};
    ss << "tar -xf "<< m_archiveFilePath << " -C "<<newDirectoryPath;
    int rv = system(ss.str().c_str());
    if (rv < 0) {
        return false;
    }
    return true;
}

bool TarCompressor::extractFileFromArch(const char* filename, const char* fileNewPath) {
    boost::mutex siglock;
    boost::lock_guard<boost::mutex> lock(siglock);

    if (m_archiveFilePath.empty() || !boost::filesystem::exists(m_archiveFilePath)) {
        return false;
    }

    std::stringstream command{};
    command << "tar -xf " << m_archiveFilePath << " --no-anchored " << filename ;

    if (fileNewPath) {
        command << " -O > " << fileNewPath;
        try {
            boost::filesystem::create_directories(boost::filesystem::path(fileNewPath).parent_path());
            if (boost::filesystem::exists(fileNewPath)) {
                boost::filesystem::remove(fileNewPath);
            }
        } catch (std::exception&) {
            return false;
        }
    }

    int rv = system(command.str().c_str());
    if (rv < 0) {
        return false;
    }

    if ((fileNewPath && !boost::filesystem::exists(fileNewPath)) ||
       (!fileNewPath && !boost::filesystem::exists(filename))) {
        // no result fileZ
        return false;
    }

    return true;
}

}
