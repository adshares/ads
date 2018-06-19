#ifndef LIBARCHIVEREADER_H
#define LIBARCHIVEREADER_H


namespace Helper {

/**
 * @brief Class to read file directly from archive without extracting.
 */
class LibArchiveReader
{
public:
    //! set path to archive file, do not open.
    LibArchiveReader(const char* archPath);

    /**
     * @brief Set paths and OPEN file.
     * @param archPath - path to archive
     * @param filepath - path to file you're looking for.
     */
    LibArchiveReader(const char* archPath, const char* filepath);

    //! dstr close file descriptor if exists.
    ~LibArchiveReader();

    /**
     * @brief Search for file in archive and get handle to it.
     * Function successful invoke allows to use manipulation functions like: open, read, lseek.
     * @param filepath - path to file you're looking for.
     * @return true if success, otherwise false.
     */
    bool open(const char* filepath);

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
    const char* m_archFilePath;
    const char* m_filePath;
    int m_fileDescriptor;
    unsigned int m_seek;
    unsigned int m_fileSize;
};

}

#endif // LIBARCHIVEREADER_H
