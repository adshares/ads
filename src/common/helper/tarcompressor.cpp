#include "tarcompressor.h"

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/filesystem.hpp>

TarCompressor::TarCompressor(const std::string &archiveFilePath, CompressionType type)
    : m_archiveFilePath(archiveFilePath) {
    if (type == eAutoDetection) {
        setType();
    } else {
        m_Type = type;
    }
}

void TarCompressor::setType() {
    std::string extension = boost::filesystem::extension(m_archiveFilePath);

    if (extension == ".gz") {
        m_Type = eGZIP;
    } else if (extension == ".bz2") {
        m_Type = eBZIP2;
    } else {
        m_Type = eNone;
    }
}

bool TarCompressor::createTar(const char* tarName, const char* directoryPath, const char *relativePath) {
    std::stringstream ss{};
    ss <<"tar -cf "<< tarName;
    if (relativePath) {
        ss << " -C " << relativePath;
    }
    ss << " " << directoryPath;
    int rv = system(ss.str().c_str());
    if (rv < 0) {
        return false;
    }
    return true;
}

bool TarCompressor::unpackTar(const char* tarName, const char* newDirectoryPath) {
    try {
        if (!boost::filesystem::exists(newDirectoryPath)) {
            boost::filesystem::create_directories(newDirectoryPath);
        } else if (!boost::filesystem::is_directory(newDirectoryPath)) {
            return false;
        }
    } catch (std::exception&) {
        return false;
    }

    std::stringstream ss{};
    ss << "tar -xf "<< tarName << " -C "<<newDirectoryPath;
    int rv = system(ss.str().c_str());
    if (rv < 0) {
        return false;
    }
    return true;
}

bool TarCompressor::compressDirectory(const char *directoryPath, const char* relativePath) {
    const char* tmpTarFile = "tmp.tar";

    if (!boost::filesystem::is_directory(directoryPath)) {
        return false;
    }

    if (!this->createTar(tmpTarFile, directoryPath, relativePath)) {
        return false;
    }

    std::ifstream ifile;
    std::ofstream ofile;
    ifile.exceptions(std::ifstream::failbit | std::ifstream::badbit | std::ifstream::eofbit);
    ofile.exceptions(std::ofstream::failbit | std::ofstream::badbit | std::ofstream::eofbit);

    int rv = true;
    try {
        ifile.open(tmpTarFile, std::ifstream::in | std::ifstream::binary);
        ofile.open(m_archiveFilePath, std::ofstream::out | std::ofstream::binary);

        boost::iostreams::filtering_ostream out;

        this->getCompressionType(out);
        out.push(ofile);
        out << ifile.rdbuf();
        ifile.close();
    } catch (std::exception&) {
        rv = false;
    }

    boost::filesystem::remove(tmpTarFile);

    return rv;
}

bool TarCompressor::decompressDirectory(const char* newDirectoryPath) {
    const char* tmpTarFile = "tmp.tar";

    std::ifstream ifile;
    std::ofstream ofile;
    ifile.exceptions(std::ifstream::failbit | std::ifstream::badbit | std::ifstream::eofbit);
    ofile.exceptions(std::ofstream::failbit | std::ofstream::badbit | std::ofstream::eofbit);

    try {
        ifile.open(m_archiveFilePath, std::ifstream::in | std::ifstream::binary);
        ofile.open(tmpTarFile, std::ofstream::out | std::ofstream::binary);

        boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
        this->getDecompressionType(in);
        in.push(ifile);
        boost::iostreams::copy(in, ofile);
    } catch (std::exception&) {
        return false;
    }

    if(!this->unpackTar(tmpTarFile, newDirectoryPath)) {
        return false;
    }

    boost::filesystem::remove(tmpTarFile);

    return true;
}

bool TarCompressor::compress(const char* data) {
    if (m_archiveFilePath.empty()) {
        return false;
    }

    std::ofstream ofile;
    ofile.exceptions(std::ifstream::failbit | std::ifstream::badbit | std::ifstream::eofbit);

    try {
        ofile.open(m_archiveFilePath, std::ios_base::out | std::ios_base::binary);
        boost::iostreams::filtering_ostream out;
        this->getCompressionType(out);
        out.push(ofile);
        out << data;
    } catch (std::exception&) {
        return false;
    }

    return true;
}

bool TarCompressor::decompress (const char *filepath) {
    if (m_archiveFilePath.empty()) {
        return false;
    }
    std::ifstream ifile;
    std::ofstream ofile;
    ifile.exceptions(std::ifstream::failbit | std::ifstream::badbit | std::ifstream::eofbit);
    ofile.exceptions(std::ofstream::failbit | std::ofstream::badbit | std::ofstream::eofbit);

    try {
        ifile.open(m_archiveFilePath, std::ios_base::in | std::ios_base::binary);
        boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
        this->getDecompressionType(in);
        in.push(ifile);

        ofile.open(filepath, std::ofstream::out | std::ofstream::binary);
        boost::iostreams::copy(in, ofile);

    } catch (std::exception&) {
        return false;
    }

    return true;
}

void TarCompressor::getCompressionType(boost::iostreams::filtering_ostream& stream_buf) {
    switch (m_Type) {
        case eGZIP:
            stream_buf.push(boost::iostreams::gzip_compressor());
            break;
        case eBZIP2:
            stream_buf.push(boost::iostreams::bzip2_compressor());
            break;
        default:
            break;
    }
}

void TarCompressor::getDecompressionType(boost::iostreams::filtering_streambuf<boost::iostreams::input>& stream_buf) {
    switch (m_Type) {
        case eGZIP:
            stream_buf.push(boost::iostreams::gzip_decompressor());
            break;
        case eBZIP2:
            stream_buf.push(boost::iostreams::bzip2_decompressor());
            break;
        default:
            break;
    }
}

bool TarCompressor::extractFileFromArch(const char* filename, const char* fileNewPath) {
    if (m_archiveFilePath.empty() || !boost::filesystem::exists(m_archiveFilePath)) {
        return false;
    }

    std::stringstream command{};
    command << "tar -xf " << m_archiveFilePath << " --no-anchored " << filename ;
    int rv = system(command.str().c_str());
    if (rv < 0) {
        return false;
    }

    if (!boost::filesystem::exists(filename)) {
        return false;
    }

    if (fileNewPath) {
        try {
            boost::filesystem::create_directories(boost::filesystem::path(fileNewPath).parent_path());
            if (boost::filesystem::exists(fileNewPath)) {
                boost::filesystem::remove(fileNewPath);
            }
            boost::filesystem::copy_file(filename, fileNewPath);
            boost::filesystem::remove(filename);
        } catch (std::exception& e) {
            std::cerr<<"Exception: "<<e.what()<<std::endl;
            return false;
        }
    }

    return true;
}
