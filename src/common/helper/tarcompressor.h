#ifndef TARCOMPRESSOR_H
#define TARCOMPRESSOR_H

#include <boost/iostreams/filtering_stream.hpp>
#include <string>
#include <sstream>

enum CompressionType {
    eNone = 0,
    eGZIP,
    eBZIP2,
    eAutoDetection
};

/** class to compress and decompress data or directories using tar and one of compression filters (gzip, bzip2).
 */
class TarCompressor
{
public:
    /**
     * @brief TarCompressor constructor, sets members
     * @param archiveFilePath - path to tar or tar.gz file.
     * For compress - result file, for decompress - input file.
     * @param type - compression type @see CompressionType
     */
    TarCompressor(const std::string& archiveFilePath, CompressionType type = eAutoDetection);

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

    // nweDirectory must exists
    bool extractFileFromArch(const char* filename, const char *fileNewDirectory = nullptr);

private:
    //! when use autodetection it set compression type based on file extension.
    void setType();

    //! set ostream filter compression type
    void getCompressionType(boost::iostreams::filtering_ostream& stream_buf);

    //! set streambuf filter decompression type
    void getDecompressionType(boost::iostreams::filtering_streambuf<boost::iostreams::input>& stream_buf);

    /**
     * @brief createTar - create tar file from directory.
     * @param tarName - result tar file
     * @param directoryPath - input directory path
     * @param relativePath - path where go to before compression
     * @return true if success, otherwise false.
     */
    bool createTar(const char* tarName, const char* directoryPath, const char* relativePath = nullptr);

    /**
     * @brief unpackTar - untar file and put files to directory path.
     * @param tarName - input tar file name.
     * @param newDirectoryPath - output directory path.
     * User "." to untar without creating new directory.
     * @return true if success, otherwise false.
     */
    bool unpackTar(const char* tarName, const char* newDirectoryPath);

    std::string m_archiveFilePath;
    CompressionType m_Type;
};

#endif // TARCOMPRESSOR_H
