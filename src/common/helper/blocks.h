#ifndef BLOCKS_H
#define BLOCKS_H

#include <stdint.h>
#include <fcntl.h>
#include <cstdio>
#include <string>

#include "default.hpp"

#define TMP_DIR "/run/shm/esc/"

namespace Helper {

/**
 * @brief Set user id to create personal temp directory for files extracted files from arch.
 * @param user_id - user id.
 */
void set_user(uint32_t user_id);

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

//! removes file
void remove_file(const char* filename);

/**
 * @brief removes file if it has a temporary path
 * @param filename
 * @return true if file is temporary and was removed, otherwise (incl. not temp file) return false.
 */
bool remove_file_if_temporary(const char* filename);

//! cleanup temp directory for current user
void cleanup_temp_directory();

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
int open_block_file(char *filename, int type = O_RDONLY, int mode = -1);

/**
 * @brief Check is file exists in not compressed block
 * @param filePath
 * @return true if exists (not compressed yet), otherwise false.
 */
bool is_file_not_compressed(const char *filePath);

//! chech is file exists in temporary directory
bool is_temporary_file(const char *filePath);
bool is_temporary_file(const std::string& filePath);

uint32_t get_users_count(uint16_t bank);

void db_backup(uint32_t block_path, uint16_t nodes);

namespace FileName {
const int kCommonNameFixedLength = 14; //only const prefix, for full length add strlen(filename)
const int kUndoNameFixedLength = 26;
const int kMsgNameFixedLength = 34;
const int kUndNameFixedLength = 34;
const int kLogNameFixedLength = 35;

//! common function for rest of files.
inline void getName(char* output, uint32_t timestamp, const char* filename) {
    sprintf(output, "blk/%03X/%05X/%s", timestamp>>20, timestamp&0xFFFFF, filename);
}
inline void getUndo(char* output, uint32_t timestamp, uint16_t bank) {
    sprintf(output, "blk/%03X/%05X/und/%04X.dat", timestamp>>20, timestamp&0xFFFFF, bank);
}
inline void getMsg(char* output, uint32_t timestamp, uint32_t msgType, uint16_t bank, uint32_t msgId) {
    sprintf(output, "blk/%03X/%05X/%02x_%04x_%08x.msg", timestamp>>20, timestamp&0xFFFFF, msgType, bank, msgId);
}
inline void getUndFile(char* output, uint32_t timestamp, uint32_t msgType, uint16_t bank, uint32_t msgId) {
    sprintf(output, "blk/%03X/%05X/%02x_%04x_%08x.und", timestamp>>20, timestamp&0xFFFFF, msgType, bank, msgId);
}
inline void getLog(char* output, uint32_t timestamp, uint16_t bank, uint32_t msgId) {
    sprintf(output, "blk/%03X/%05X/log/%04X_%08X.log", timestamp>>20, timestamp&0xFFFFF, bank, msgId);
}
inline void getLogTimeBin(char* output, uint32_t timestamp) {
    sprintf(output, "blk/%03X/%05X/log/time.bin", timestamp>>20, timestamp&0xFFFFF);
}
inline void getUsr(char* output, uint16_t bank) {
    sprintf(output, "usr/%04X.dat", bank);
}
inline void getBlk(char *output, uint32_t timestamp, const char* dir = nullptr) {
    if (!dir) {
        sprintf(output, "blk/%03X/%05X/", timestamp>>20, timestamp&0xFFFFF);
    } else {
        sprintf(output, "blk/%03X/%05X/%s/", timestamp>>20, timestamp&0xFFFFF, dir);
    }
}

} // namespace Files

} // namespace Helper

#endif // BLOCKS_H
