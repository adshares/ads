#ifndef TARCOMPRESSOR_H
#define TARCOMPRESSOR_H

#include <string>
#include <sstream>

namespace Helper {

const char* const ARCH_EXTENSION = ".tar";

/** class to compress and decompress data or directories using tar.
 */
class TarCompressor
{
public:
    /**
     * @brief TarCompressor constructor, sets members
     * @param archiveFilePath - path to tar or tar.gz file.
     * For compress - result file, for decompress - input file.
     */
    TarCompressor(const std::string& archiveFilePath);

    /**
     * @brief compress - compress data string.
     * @param data - input data string
     * @return true if success otherwise false. Result file is archiveFilePath.
     */
    bool compress(const char* data);

    /**
     * @brief decompress - decompress data from file
     * @param filepath - result file with decompressed data.
     * @return true if success otherwise false. Input archive as archiveFilePath.
     */
    bool decompress(const char* filepath);

    /**
     * @brief compressDirectory - tar and compress directory.
     * @param directoryPath - input directory path
     * @param relativePath = path where go to before compression
     * @return true if success, otherwise false. Result file is archiveFilePath.
     */
    bool compressDirectory(const char* directoryPath, const char* relativePath = nullptr);

    /**
     * @brief decompressDirectory - untar and decompress directory.
     * @param newDirectoryPath - path to directory where files will be unpack.
     * Use "." to unpack without creating directory.
     * @return true if success. otherwise false.
     */
    bool decompressDirectory(const char* newDirectoryPath);

    /**
     * @brief Extracts one file (#filename) from arch (#archiveFilePath)
     * @param filename - name of file to extract
     * @param fileNewDirectory - optional, new file location
     * @return
     */
    bool extractFileFromArch(const char* filename, const char *fileNewDirectory = nullptr);

private:
    std::string m_archiveFilePath;
};
}

#endif // TARCOMPRESSOR_H
