#ifndef BLOCKS_H
#define BLOCKS_H

#include <stdint.h>
#include <fcntl.h>
#include <cstdio>
#include <string>

#define TMP_DIR "tmp/"

namespace Helper {

/**
 * @brief Compress old blocks, @see BLOCKS_COMPRESSED_SHIFT .
 * @param currentTime - decimal current block start value
 */
void tar_old_blocks(uint32_t currentTime);


/**
 * @brief Removes block from filesystem.
 * @param blockPath - path to block directory in filesystem
 */
void remove_block(const char* blockPath);

void remove_file(const char* filename);
bool remove_file_if_temporary(const char* filename);

/**
 * @brief Extracts file from arch if needed.
 * @param filePath - [in|out] full path to file eg. staring from blk/.../0001.dat.
 * Note that filePath will be changed if file decompressed to temporary place.
 * @return true if file successful extracted and can be open, false if file not exists.
 */
bool get_file_from_block(char *filePath);

/**
 * @brief Open file from block or tar, depends on location.
 * @param filename - file path.
 * @param type - O_RDONLY, O_WRONLY etc.
 * @return file descriptor
 */
int open_block_file(char *filename, int type = O_RDONLY);

bool is_file_not_compressed(const char *filePath);

//! chech is file exists in temporary directory
bool is_temporary_file(const char *filePath);
bool is_temporary_file(const std::string& filePath);

namespace FileName {
const int kCommonNameFixedLength = 14; //only const prefix, for full length add strlen(filename)
const int kUndoNameFixedLength = 26;
const int kMsgNameFixedLength = 34;
const int kUndNameFixedLength = 34;
const int kLogNameFixedLength = 35;

//! common function for rest of files.
constexpr void getName(char* output, uint32_t timestamp, const char* filename) {
    sprintf(output, "blk/%03X/%05X/%s", timestamp>>20, timestamp&0xFFFFF, filename);
}
constexpr void getUndo(char* output, uint32_t timestamp, uint16_t bank) {
    sprintf(output, "blk/%03X/%05X/und/%04X.dat", timestamp>>20, timestamp&0xFFFFF, bank);
}
constexpr void getMsg(char* output, uint32_t timestamp, uint32_t msgType, uint16_t bank, uint32_t msgId) {
    sprintf(output, "blk/%03X/%05X/%02x_%04x_%08x.msg", timestamp>>20, timestamp&0xFFFFF, msgType, bank, msgId);
}
constexpr void getUndFile(char* output, uint32_t timestamp, uint32_t msgType, uint16_t bank, uint32_t msgId) {
    sprintf(output, "blk/%03X/%05X/%02x_%04x_%08x.und", timestamp>>20, timestamp&0xFFFFF, msgType, bank, msgId);
}
constexpr void getLog(char* output, uint32_t timestamp, uint16_t bank, uint32_t msgId) {
    sprintf(output, "blk/%03X/%05X/log/%04X_%08X.log", timestamp>>20, timestamp&0xFFFFF, bank, msgId);
}
constexpr void getLogTimeBin(char* output, uint32_t timestamp) {
    sprintf(output, "blk/%03X/%05X/log/time.bin", timestamp>>20, timestamp&0xFFFFF);
}

} // namespace Files

} // namespace Helper

#endif // BLOCKS_H
