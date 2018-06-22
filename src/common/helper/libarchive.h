#ifndef LIBARCHIVE_H
#define LIBARCHIVE_H

#include <stdint.h>

/**
 * Header of archive
 * 2B - no. of files in archive
 * for each file:
 * 4B - seek in archive to begin of file data
 * variable size (null terminated) filepath
 *
 * files write continuously without separators
 */

namespace Helper {

const char* const ARCH_EXTENSION = ".arch";

/**
 * @brief Library to create and read archive with file index in header.
 */
class LibArchive
{
public:
    /**
     * @brief LibArchive - cstr
     * @param archPath - path to archive file
     */
    LibArchive(const char* archPath);

    /**
     * @brief Creates archive from directory.
     * @param directoryPath - directory to compress
     * @param relativePath - path will be ommited from full path when putting to header
     * @return true if sucessfull created, otherwise false.
     */
    bool createArch(const char* directoryPath, bool cutRelativePath = false);

    /**
     * @brief Gets handle to file inside arch withour extracting.
     * Function opens, but not close fd, remember to do this.
     * @param filepath - path to file looking for.
     * @param file_descriptor - [out] file descriptor (fd)
     * @param offset - [out] file begin offset in archive
     * @param file_size - [out] size of file
     * @return true if success otherwise false.
     */
    bool getFileHandle(const char* filepath, int *file_descriptor, uint64_t *offset, uint64_t *file_size);

private:
    const char* m_filePath;

};

}

#endif // LIBARCHIVE_H
