#ifndef BLOCKS_H
#define BLOCKS_H

#include <stdint.h>
#include <fcntl.h>
#include <cstdio>
#include <string>

#include "default.hpp"

namespace Helper {

/**
 * @brief Compress old blocks, @see BLOCKS_COMPRESSED_SHIFT .
 * @param currentTime - decimal current block start value
 */
void arch_old_blocks(uint32_t currentTime);

/**
 * @brief Removes block from filesystem.
 * @param blockPath - path to block directory in filesystem
 */
void remove_block(const char* blockPath);

/**
 * @brief Removes files from block path, except directory set as exclude_dir.
 * @param blockPath - path to block directory in filesystem
 * @param excludeDir - path to directory skip during remove
 */
void remove_block_exclude(const char* blockPath, const char* excludeDir);

//! removes file
void remove_file(const char* filename);

/**
 * @brief Check is file exists in not compressed block
 * @param filePath
 * @return true if exists (not compressed yet), otherwise false.
 */
bool is_file_not_compressed(const char *filePath);

//! Check is it directory for snapshot or not.
inline bool is_snapshot_directory(uint32_t blockTime);

uint32_t get_users_count(uint16_t bank);

void db_backup(uint32_t block_path, uint16_t nodes);

namespace FileName {
const int kCommonNameFixedLength = 14; //only const prefix, for full length add strlen(filename)
const int kUndoNameFixedLength = 26;
const int kMsgNameFixedLength = 34;
const int kUndNameFixedLength = 34;
const int kLogNameFixedLength = 35;

//! common function for rest of files.
inline void getName(char* output, size_t size, uint32_t timestamp, const char* filename) {
    snprintf(output, size, "blk/%03X/%05X/%s", timestamp>>20, timestamp&0xFFFFF, filename);
}
inline void getUndo(char* output, size_t size, uint32_t timestamp, uint16_t bank) {
    snprintf(output, size, "blk/%03X/%05X/und/%04X.dat", timestamp>>20, timestamp&0xFFFFF, bank);
}
inline void getMsg(char* output, size_t size, uint32_t timestamp, uint32_t msgType, uint16_t bank, uint32_t msgId) {
    snprintf(output, size, "blk/%03X/%05X/%02x_%04x_%08x.msg", timestamp>>20, timestamp&0xFFFFF, msgType, bank, msgId);
}
inline void getUndFile(char* output, size_t size, uint32_t timestamp, uint32_t msgType, uint16_t bank, uint32_t msgId) {
    snprintf(output, size, "blk/%03X/%05X/%02x_%04x_%08x.und", timestamp>>20, timestamp&0xFFFFF, msgType, bank, msgId);
}
inline void getLog(char* output, size_t size, uint32_t timestamp, uint16_t bank, uint32_t msgId) {
    snprintf(output, size, "blk/%03X/%05X/log/%04X_%08X.log", timestamp>>20, timestamp&0xFFFFF, bank, msgId);
}
inline void getLogTimeBin(char* output, size_t size, uint32_t timestamp) {
    snprintf(output, size, "blk/%03X/%05X/log/time.bin", timestamp>>20, timestamp&0xFFFFF);
}
inline void getUsr(char* output, size_t size, uint16_t bank) {
    snprintf(output, size, "usr/%04X.dat", bank);
}
inline void getBlk(char *output, size_t size, uint32_t timestamp, const char* dir = nullptr) {
    if (!dir) {
        snprintf(output, size, "blk/%03X/%05X/", timestamp>>20, timestamp&0xFFFFF);
    } else {
        snprintf(output, size, "blk/%03X/%05X/%s/", timestamp>>20, timestamp&0xFFFFF, dir);
    }
}

} // namespace Files

} // namespace Helper

#endif // BLOCKS_H
