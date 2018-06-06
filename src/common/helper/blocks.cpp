#include "blocks.h"

#include <iostream>
#include <cstdio>
#include <boost/filesystem.hpp>

#include "tarcompressor.h"
#include "default.h"

namespace Helper {

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

bool get_file_from_block(const char* filePath, const char* fileNewPath) {
    boost::filesystem::path path(filePath);
    if (boost::filesystem::exists(path)) {
        // file exists in uncompressed directory
        return true;
    }

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

    TarCompressor tar(blockpath, CompressionType::eGZIP);
    return tar.extractFileFromArch(filepath_in_block.c_str(), fileNewPath);
}

int open_block_file(const char* filename, int type) {
    if (boost::filesystem::exists(filename)) {
        return open(filename, type);
    }

    std::string newPath(filename);
    newPath.replace(0, 3, "tmp");
    if (!get_file_from_block(filename, newPath.c_str())) {
        return -1;
    }

    return open(newPath.c_str(), type);
}

}
