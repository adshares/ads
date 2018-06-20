#include "libarchivereader.h"

#include <fcntl.h>
#include <unistd.h>

#include "libarchive.h"

namespace Helper {

LibArchiveReader::LibArchiveReader(const char* archPath) :
    m_archFilePath(archPath), m_filePath(nullptr), m_fileDescriptor(-1), m_seek(0), m_fileSize(0)
{
}

LibArchiveReader::LibArchiveReader(const char* archPath, const char* filepath) :
    m_archFilePath(archPath), m_filePath(filepath), m_fileDescriptor(-1), m_seek(0), m_fileSize(0)
{
    this->open(filepath);
}

LibArchiveReader::~LibArchiveReader()
{
    this->close();
}

bool LibArchiveReader::open(const char* filepath)
{
    if (!m_archFilePath) return false;
    m_filePath = filepath;

    LibArchive lib(m_archFilePath);
    if (!lib.getFileHandle(m_filePath, &m_fileDescriptor, &m_seek, &m_fileSize))
    {
        m_fileDescriptor = -1;
        m_seek = 0;
        m_fileSize = 0;
        return false;
    }
    ::lseek(m_fileDescriptor, m_seek, SEEK_SET);
    return true;
}

void LibArchiveReader::close()
{
    if (m_fileDescriptor >= 0)
    {
        ::close(m_fileDescriptor);
    }
}

int LibArchiveReader::read(void* buffer, unsigned int size)
{
    int current_position = ::lseek(m_fileDescriptor, 0, SEEK_CUR);
    if ((size + current_position) > (m_fileSize + m_seek))
    {
        // allow to read only till end of this file
        size = (m_fileSize + m_seek) - current_position;
    }
    return ::read(m_fileDescriptor, buffer, size);
}

bool LibArchiveReader::lseek(int position, int whence)
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

bool LibArchiveReader::isOpen()
{
    return (m_fileDescriptor >=0 && m_seek > 0 && m_fileSize > 0);
}

unsigned int LibArchiveReader::getSize()
{
    return m_fileSize;
}

}
