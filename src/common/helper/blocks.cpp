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

#include "default.h"
#include "libarchive.h"

namespace Helper {


boost::mutex blocklock;

void arch_old_blocks(uint32_t currentTime) {
#ifdef BLOCKS_COMPRESSED_SHIFT
    unsigned int blocksComprShift = (BLOCKS_COMPRESSED_SHIFT < BLOCKDIV) ? BLOCKDIV+1 : BLOCKS_COMPRESSED_SHIFT;
    currentTime -= ((blocksComprShift-1) * BLOCKSEC);
    char dirpath[16];
    char filepath[32];
    for (int i=0; i<50; ++i) {
        currentTime -= BLOCKSEC;
        Helper::FileName::getBlk(dirpath, currentTime);
        if (boost::filesystem::exists(dirpath)) {
            // do not compress und/* files
            boost::filesystem::path und_dir(dirpath);
            und_dir += "/und";
            boost::filesystem::remove_all(und_dir);

            // create archive from blk directory
            sprintf(filepath, "blk/%03X/%05X%s", currentTime>>20, currentTime&0xFFFFF, Helper::ARCH_EXTENSION);
            Helper::LibArchive arch(filepath);
            if (!arch.createArch(dirpath)) {
                std::cerr << "Error directory compressing "<<dirpath<<std::endl;
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

bool is_file_not_compressed(const char* filePath) {
    return boost::filesystem::exists(boost::filesystem::path(filePath));
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

        // load previous snapshot or usr/* for initial run
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
                if (u.msid != 0)  // user found, get from undo
                {
                    last_snapshot.seek(usert_size, SEEK_CUR);
                    if (!write(current_snapshot, (char*)&u, usert_size)) break;
                    break;
                }
            } // end of undo
            if (u.msid != 0) continue; // user already found
            // not found in undo, get from last snapshot
            last_snapshot.read((char*)&u, usert_size);
            if (!write(current_snapshot, (char*)&u, usert_size)) break;
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
