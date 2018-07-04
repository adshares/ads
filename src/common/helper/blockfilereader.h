#ifndef BLOCKFILEREADER_H
#define BLOCKFILEREADER_H

#include <stdint.h>
#include <string>

namespace Helper {

/**
 * @brief Class to read file directly from archive without extracting.
 * Note that you can use it also for read regular file, just put as argument file without .arch extension
 */
class BlockFileReader
{
public:

    BlockFileReader();

    /**
     * @brief set path to regular file, reader (depends on path) localize it in block or archive and will try to open
     * @param filepath - full path to file in block (eg. blk/5AB/27000/servers.srv
     */
    BlockFileReader(const char* filepath);

    /**
     * @brief Set paths and open file from archive.
     * @param archPath - if has .arch extension then will be handled as archive otherwise as regular file
     * @param filepath - path to file you're looking for. eg. server.srv
     */
    BlockFileReader(const char* archPath, const char* filepath);

    //! dstr close file descriptor if exists.
    ~BlockFileReader();

    /**
     * @brief Search for file in archive and get handle to it.
     * Function successful invoke allows to use manipulation functions like: open, read, lseek.
     * @param filepath - new filepath, for already existing object
     * @return true if success, otherwise false.
     */
    bool openFromArch(const std::string filepath = "");

    /**
     * @brief open regular file.
     * @return true if success otherwise false.
     */
    bool openFile();

    /**
     * @brief Read data from file
     * @param buffer - data buffer
     * @param size - size to read
     * @return status of unistd read function invoke.
     */
    int read(void* buffer, unsigned int size);

    /**
     * @brief Seek cursor in file
     * @param position - new position
     * @param whence - SEEK_CUR, SEEK_SET, SEEK_END
     * @return status of unisdt lseek function invoke.
     */
    bool lseek(int position, int whence);

    //! close file descriptor if exists
    void close();

    /**
     * @brief Check is file successfuly open and assigned, ready to use.
     * @return true if open and ready, otherwise false.
     */
    bool isOpen();

    /**
     * @brief Return real size of file inside archive.
     * @return 0 if failed otherwise positive value.
     */
    unsigned int getSize();

    //    unsigned int write();
private:

    bool isArchFile();

    std::string m_archFilePath;
    std::string m_filePath;
    int m_fileDescriptor;
    uint64_t m_seek;
    uint64_t m_fileSize;
};

}

#endif // BLOCKFILEREADER_H
