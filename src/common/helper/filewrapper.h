#ifndef FILEWRAPPER_H
#define FILEWRAPPER_H

#include <string>
#include <fcntl.h>

namespace Helper {

/**
 * @brief RAII File descriptor wrapper, allows to close and optionally remove file in destructor.
 */
class FileWrapper
{
public:
    FileWrapper();
    FileWrapper(const std::string filepath, int mask, int mode = 0644, bool removeOnClose = false);
    ~FileWrapper();

    bool isOpen();
    bool remove();

    bool open(const char* filename, int mask = O_RDONLY, int mode = 0644, bool removeOnClose = false);
    bool seek(int position, int whence);
    int write(void* buffer, unsigned int size);
    int read(void* buffer, unsigned int size);

private:
    int m_file_descriptor;
    std::string m_filepath;
    bool m_remove_on_close;
};

}

#endif // FILEWRAPPER_H
