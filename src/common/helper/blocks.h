#ifndef BLOCKS_H
#define BLOCKS_H

#include <stdint.h>

namespace Helper {

/**
 * @brief tar_old_blocks - compress old blocks, @see BLOCKS_COMPRESSED_SHIFT
 * @param currentTime - decimal current block start value
 */
void tar_old_blocks(uint32_t currentTime);


/**
 * @brief remove_block - removes block from filesystem
 * @param blockPath - path to block directory in filesystem
 */
void remove_block(const char* blockPath);

}

#endif // BLOCKS_H
