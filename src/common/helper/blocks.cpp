#include "blocks.h"

#include <iostream>
#include <stdio.h>
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

}
