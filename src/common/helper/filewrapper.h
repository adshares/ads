#ifndef FILEWRAPPER_H
#define FILEWRAPPER_H

#include <string>

namespace Helper {

/**
 * @brief RAII File descriptor wrapper, allows to close and optionally remove file in destructor.
 */
class FileWrapper
{
public:
    FileWrapper(const std::string filepath, int mask, bool removeOnClose = false);
    ~FileWrapper();
    FileWrapper(const FileWrapper& obj) = default;
    FileWrapper &operator=(const FileWrapper&) = delete;

    bool isOpen();
    bool remove();

    bool seek(int position, int whence);
    int write(void* buffer, unsigned int size);
    int read(void* buffer, unsigned int size);

private:
    int m_file_descriptor;
    const std::string m_filepath;
    bool m_remove_on_close;
};

}

#endif // FILEWRAPPER_H
