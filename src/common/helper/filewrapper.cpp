#include "filewrapper.h"

#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <boost/filesystem.hpp>

namespace Helper {

FileWrapper::FileWrapper(const std::string filepath, int mask, bool removeOnClose) :
    m_filepath(filepath), m_remove_on_close(removeOnClose)
{
    m_file_descriptor = open(m_filepath.c_str(), mask);
}

FileWrapper::~FileWrapper()
{
    close(m_file_descriptor);
    m_file_descriptor = -1;
    if (m_remove_on_close) {
        remove();
    }
}

bool FileWrapper::isOpen()
{
    return (m_file_descriptor >= 0);
}

bool FileWrapper::remove()
{
    try {
        boost::filesystem::remove(m_filepath);
    } catch (std::exception&) {
        return false;
    }
    return true;
}

bool FileWrapper::seek(int position, int whence)
{
    return (::lseek(m_file_descriptor, position, whence) != -1);
}

int FileWrapper::write(void* buffer, unsigned int size)
{
    return ::write(m_file_descriptor, buffer, size);
}

int FileWrapper::read(void *buffer, unsigned int size)
{
    return ::read(m_file_descriptor, buffer, size);
}

}
