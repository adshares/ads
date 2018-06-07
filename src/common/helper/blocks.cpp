#include "blocks.h"

#include <iostream>
#include <cstdio>
#include <boost/filesystem.hpp>

#include "tarcompressor.h"
#include "default.h"

namespace Helper {

const int TMP_DIR_NAME_LENGTH = strlen(TMP_DIR);

void tar_old_blocks(uint32_t currentTime) {
    currentTime -= ((BLOCKS_COMPRESSED_SHIFT-1) * BLOCKSEC);
    char dirpath[16];
    char filepath[32];
    for (int i=0; i<10; ++i) {
        currentTime -= BLOCKSEC;
        sprintf(dirpath, "blk/%03X/%05X", currentTime>>20, currentTime&0xFFFFF);
        if (boost::filesystem::exists(dirpath)) {
            sprintf(filepath, "blk/%03X/%05X.tar.gz", currentTime>>20, currentTime&0xFFFFF);
            TarCompressor tar(filepath, CompressionType::eGZIP);
            if (!tar.compressDirectory(".", dirpath)) {
                std::cerr<< "Error directory compressing" <<dirpath<<std::endl;
                return;
            }
            Helper::remove_block(dirpath);
        } else {
            return;
        }
    }
}

void remove_block(const char* blockPath) {
    boost::filesystem::remove_all(blockPath);
}

void remove_file(const char* filename) {
    boost::filesystem::remove(filename);
}

bool remove_file_if_temporary(const char* filename) {
    if (Helper::is_temporary_file(filename)) {
        remove_file(filename);
        return true;
    }
    return false;
}

bool get_file_from_block(char *filePath) {
    if (is_file_not_compressed(filePath)) {
        // file exists in uncompressed directory
        return true;
    }

    boost::filesystem::path path(filePath);
    boost::filesystem::path::iterator it = path.begin();
    std::string blockpath(it->string());
    ++it;
    for (unsigned int i=1; i<3 && it!=path.end(); ++it, ++i) {
        blockpath += "/";
        blockpath += it->string();
    }
    blockpath += ".tar.gz";

    std::string filepath_in_block(".");
    for ( ; it != path.end(); ++it) {
        filepath_in_block += "/";
        filepath_in_block += it->string();
    }

    std::string newpath(filePath);
    newpath.replace(0, 4, TMP_DIR); // replace blk with tmp
    TarCompressor tar(blockpath, CompressionType::eGZIP);
    bool res = tar.extractFileFromArch(filepath_in_block.c_str(), newpath.c_str());
    if (res) {
        strncpy(filePath, TMP_DIR, strlen(TMP_DIR));
    }
    return res;
}

int open_block_file(char* filename, int type) {
    if (boost::filesystem::exists(filename)) {
        return open(filename, type);
    }

    if (!get_file_from_block(filename)) {
        return -1;
    }

    return open(filename, type);
}

bool is_file_not_compressed(const char* filePath) {
    return boost::filesystem::exists(boost::filesystem::path(filePath));
}

bool is_temporary_file(const char *filePath) {
    return (strncmp(filePath, TMP_DIR, TMP_DIR_NAME_LENGTH) == 0);
}

bool is_temporary_file(const std::string& filePath) {
    return (filePath.substr(0, TMP_DIR_NAME_LENGTH).find(TMP_DIR) != std::string::npos);
}

}
