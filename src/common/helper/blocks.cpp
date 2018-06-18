#include "blocks.h"

#include <iostream>
#include <fstream>
#include <cstdio>
#include <iomanip>
#include <sys/stat.h>
#include <helper/filewrapper.h>
#include <boost/filesystem.hpp>
#include <boost/chrono.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <memory>
#include "tarcompressor.h"
#include "default.h"

namespace Helper {

boost::mutex blocklock;

const int TMP_DIR_NAME_LENGTH = strlen(TMP_DIR);
static uint32_t current_user = 0;

void set_user(uint32_t user_id) {
    current_user = user_id;
}

const std::string get_user_dir() {
    std::stringstream ss{};
    ss.fill('0');
    ss << std::setw(4) << std::hex << current_user;
    ss << "/";
    return ss.str();
}

void tar_old_blocks(uint32_t currentTime) {
#ifdef BLOCKS_COMPRESSED_SHIFT
    unsigned int blocksComprShift = (BLOCKS_COMPRESSED_SHIFT < BLOCKDIV) ? BLOCKDIV+1 : BLOCKS_COMPRESSED_SHIFT;
    currentTime -= ((blocksComprShift-1) * BLOCKSEC);
    char dirpath[16];
    char filepath[32];
    for (int i=0; i<50; ++i) {
        currentTime -= BLOCKSEC;
        Helper::FileName::getBlk(dirpath, currentTime);
        if (boost::filesystem::exists(dirpath)) {
            boost::filesystem::path und_dir(dirpath);
            und_dir += "/und";
            boost::filesystem::remove_all(und_dir); // do not compress und/* files
            sprintf(filepath, "blk/%03X/%05X.tar", currentTime>>20, currentTime&0xFFFFF);
            TarCompressor tar(filepath);
            if (!tar.compressDirectory(".", dirpath)) {
                std::cerr<< "Error directory compressing" <<dirpath<<std::endl;
                return;
            }
            Helper::remove_block(dirpath);
        } else {
            return;
        }
    }
#endif
}

void remove_block(const char* blockPath) {
    boost::lock_guard<boost::mutex> lock(blocklock);
    boost::filesystem::remove_all(blockPath);
}

void remove_file(const char* filename) {
    boost::filesystem::remove(filename);
}

void cleanup_temp_directory() {
    boost::filesystem::path path(TMP_DIR);
    path += get_user_dir();
    boost::filesystem::remove_all(path);
}

bool remove_file_if_temporary(const char* filename) {
    if (Helper::is_temporary_file(filename)) {
        remove_file(filename);
        return true;
    }
    return false;
}

bool get_file_from_block(char *filePath) {
    boost::lock_guard<boost::mutex> lock(blocklock);

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
    blockpath += Helper::ARCH_EXTENSION;

    std::string filepath_in_block(".");
    for ( ; it != path.end(); ++it) {
        filepath_in_block += "/";
        filepath_in_block += it->string();
    }

    std::string newpath(filePath);
    newpath.replace(0, TMP_DIR_NAME_LENGTH, TMP_DIR); // replace blk with tmp
    newpath.insert(TMP_DIR_NAME_LENGTH, get_user_dir());

    TarCompressor tar(blockpath);
    bool res = tar.extractFileFromArch(filepath_in_block.c_str(), newpath.c_str());
    if (res) {
        strcpy(filePath, newpath.c_str());
    }
    return res;
}

int open_block_file(char* filename, int type, int mode) {
    if (!boost::filesystem::exists(filename)) {
        if (!get_file_from_block(filename)) {
            return -1;
        }
    }

    if (mode != -1) {
        return open(filename, type, mode);
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

uint32_t get_users_count(uint16_t bank) {
    char usrPath[64];
    Helper::FileName::getUsr(usrPath, bank);
    struct stat st;
    stat(usrPath, &st);
    return (st.st_size/sizeof(user_t));
}

void db_backup(uint32_t block_path, uint16_t nodes) {
    // used to temporary name for snapshot, resolve names colision between sparse local und/* file and currently creating db snapshot.
    const char* const snapshot_postfix = "tmp";

    if ((block_path/BLOCKSEC)%BLOCKDIV) {
        return;
    }

    char previousSnapshotPath[64];
    char backupFilePath[64];
    unsigned int usert_size = sizeof(user_t);

    // for each node
    for (uint16_t bank = 1; bank < nodes; ++bank)
    {
        // get undo files, youngest at begin
        std::vector<std::shared_ptr<Helper::FileWrapper>> undoFiles;
        std::shared_ptr<Helper::FileWrapper> undo_file;
        for (int i=0; i<BLOCKDIV; ++i) {
            uint32_t block = block_path - (i*BLOCKSEC);
            char undoPath[64];
            Helper::FileName::getUndo(undoPath, block, bank);

            undo_file.reset(new FileWrapper(std::string(undoPath), O_RDONLY, true));
            if (undo_file->isOpen()) {
                undoFiles.push_back(undo_file);
            }
        }

        uint32_t users = get_users_count(bank);
        Helper::FileName::getUndo(previousSnapshotPath, (block_path - (BLOCKDIV*BLOCKSEC)), bank);
        if (!boost::filesystem::exists(previousSnapshotPath)) {
            Helper::FileName::getUsr(previousSnapshotPath, bank);
        }

        Helper::FileWrapper last_snapshot(previousSnapshotPath, O_RDONLY, false);
        if (!last_snapshot.isOpen()) {
            return;
        }

        user_t u;
        Helper::FileName::getUndo(backupFilePath, block_path, bank);
        strcat(backupFilePath, snapshot_postfix);

        int current_snapshot = open(backupFilePath, O_WRONLY | O_CREAT, 0644);
        if (current_snapshot < 0) {
            return;
        }

        // for each user
        for (uint32_t user = 0; user < users; ++user)
        {
            u = {};
            for (auto it=undoFiles.begin(); it != undoFiles.end(); ++it)
            {
                (*it)->seek(user * usert_size, SEEK_SET);
                (*it)->read((char*)&u, usert_size);
                if (u.msid != 0)  // user found
                {
                    last_snapshot.seek(usert_size, SEEK_CUR);
                    write(current_snapshot, (char*)&u, usert_size);
                    break;
                }
            } // end of undo
            if (u.msid != 0) continue; // user already found
            // not found in undo, get from last snapshot
            last_snapshot.read((char*)&u, usert_size);
            write(current_snapshot, (char*)&u, usert_size);
        } // foreach user

        close(current_snapshot);
    } // foreach bank

    // change snapshot temp file name to correct one
    for (uint16_t bank = 1; bank < nodes; ++bank)
    {
        Helper::FileName::getUndo(backupFilePath, block_path, bank);
        strcat(backupFilePath, snapshot_postfix);
        std::rename(backupFilePath,
                    std::string(backupFilePath).substr(0, Helper::FileName::kUndoNameFixedLength).c_str());
    }

    return;
}

}
