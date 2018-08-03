#include "blockfilereader.h"

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <iostream>
#include <boost/filesystem.hpp>

#include "libarchive.h"
#include "blocks.h"

namespace Helper {

BlockFileReader::BlockFileReader() :
    m_archFilePath(""), m_filePath(""), m_fileDescriptor(-1), m_seek(0), m_fileSize(0)
{
}

BlockFileReader::BlockFileReader(const char* filepath) :
    m_archFilePath(filepath), m_filePath(""), m_fileDescriptor(-1), m_seek(0), m_fileSize(0)
{
    if (Helper::is_file_not_compressed(m_archFilePath.c_str()))
    {
        this->openFile();
    } else
    {
        boost::filesystem::path path(filepath);
        boost::filesystem::path::iterator it = path.begin();
        std::string blockpath(it->string());
        ++it;
        for (unsigned int i=1; i<3 && it!=path.end(); ++it, ++i)
        {
            blockpath += "/";
            blockpath += it->string();
        }
        blockpath += Helper::ARCH_EXTENSION;

        m_archFilePath = blockpath;
        m_filePath = path.filename().string();

        this->openFromArch();
    }
}

BlockFileReader::BlockFileReader(const char* archPath, const char* filepath) :
    m_archFilePath(archPath), m_filePath(filepath), m_fileDescriptor(-1), m_seek(0), m_fileSize(0)
{
    this->openFromArch();
}

BlockFileReader::~BlockFileReader()
{
    this->close();
}

bool BlockFileReader::openFile()
{
   m_fileDescriptor = ::open(m_archFilePath.c_str(), O_RDONLY);
   if (m_fileDescriptor >= 0)
   {
       m_seek = 0;
       struct stat st = {};
       ::stat(m_archFilePath.c_str(), &st);
       m_fileSize = st.st_size;
       ::lseek(m_fileDescriptor, 0, SEEK_SET);
       return true;
   }
   return false;
}

bool BlockFileReader::openFromArch(const std::string filepath)
{
    if (!filepath.empty()) {
        m_filePath = filepath;
    }
    if (m_archFilePath.empty() || m_filePath.empty()) return false;

    LibArchive lib(m_archFilePath.c_str());
    if (!lib.getFileHandle(m_filePath.c_str(), &m_fileDescriptor, &m_seek, &m_fileSize))
    {
        m_fileDescriptor = -1;
        m_seek = 0;
        m_fileSize = 0;
        return false;
    }
    ::lseek(m_fileDescriptor, m_seek, SEEK_SET);

    return true;
}

void BlockFileReader::close()
{
    if (m_fileDescriptor >= 0)
    {
        ::close(m_fileDescriptor);
    }
    m_fileDescriptor = -1;
    m_seek = 0;
    m_fileSize = 0;
}

int BlockFileReader::read(void* buffer, unsigned int size)
{
    int current_position = ::lseek(m_fileDescriptor, 0, SEEK_CUR);
    if (current_position < 0)
    {
        // means that handle (fd) was released somehow
        assert(current_position >= 0);
        return -1;
    }

    if ((size + current_position) > (m_fileSize + m_seek))
    {
        // allow to read only till end of this file
        size = (m_fileSize + m_seek) - current_position;
    }

    return ::read(m_fileDescriptor, buffer, size);
}

bool BlockFileReader::lseek(int position, int whence)
{
    int64_t offset = 0;
    switch(whence)
    {
    case SEEK_CUR:
        offset = position;
        break;
    case SEEK_END:
        offset = m_fileSize + position;
        whence = SEEK_SET;
        break;
    case SEEK_SET:
        offset = position + m_seek;
        break;
    default:
        break;
    }

    if (offset > (int64_t)(m_fileSize + m_seek))
    {
        offset = m_fileSize + m_seek;
        whence = SEEK_SET;
    }

    return (::lseek(m_fileDescriptor, offset, whence) != -1);
}

bool BlockFileReader::isOpen()
{
    return (m_fileDescriptor >=0 && m_fileSize > 0);
}

unsigned int BlockFileReader::getSize()
{
    return m_fileSize;
}

bool BlockFileReader::isArchFile()
{
    if (!m_archFilePath.empty())
    {
        const char* extension = strrchr(m_archFilePath.c_str(), '.');
        return (strcmp(extension, ARCH_EXTENSION) == 0);
    }
    return false;
}

}
