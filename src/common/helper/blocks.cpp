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
#include "servers.h"

namespace Helper {


boost::mutex blocklock;

void arch_old_blocks(uint32_t currentTime) {
#ifdef BLOCKS_COMPRESSED_SHIFT
    unsigned int blocksComprShift = (BLOCKS_COMPRESSED_SHIFT <= BLOCKDIV) ? BLOCKDIV+1 : BLOCKS_COMPRESSED_SHIFT;
    currentTime -= ((blocksComprShift-1) * BLOCKSEC);
    char dirpath[16];
    char filepath[32];
    for (int i=0; i<50; ++i) {
        currentTime -= BLOCKSEC;
        Helper::FileName::getBlk(dirpath, sizeof(dirpath), currentTime);
        char header_file_path[64];
        Helper::FileName::getName(header_file_path, sizeof(header_file_path), currentTime, "header.hdr");
        if (boost::filesystem::exists(dirpath) && boost::filesystem::exists(header_file_path)) {
            // create archive from blk directory
            snprintf(filepath, sizeof(filepath), "blk/%03X/%05X%s", currentTime>>20, currentTime&0xFFFFF, Helper::ARCH_EXTENSION);
            Helper::LibArchive arch(filepath);
            if (arch.createArch(dirpath)) {
                if (is_snapshot_directory(currentTime)) {
                    Helper::remove_block_exclude(dirpath, "und");
                } else {
                    Helper::remove_block(dirpath);
                }
            } else {
                std::cerr << "Error directory compressing "<<dirpath<<std::endl;
            }
        } else {
            return;
        }
    }
#endif
}

void remove_block(const char* blockPath)
{
    boost::lock_guard<boost::mutex> lock(blocklock);
    boost::filesystem::remove_all(blockPath);
}

void remove_block_exclude(const char* blockPath, const char* excludeDir) {
    boost::lock_guard<boost::mutex> lock(blocklock);
    boost::filesystem::path blockpath(blockPath);
    boost::filesystem::path exclude_dir{blockpath / excludeDir};
    boost::filesystem::directory_iterator end_dir, dir(blockpath);

    while (dir != end_dir)
    {
        if (dir->path() != exclude_dir)
        {
            boost::filesystem::remove_all(dir->path());
        }
        ++dir;
    }
}

void remove_file(const char* filename) {
    boost::filesystem::remove(filename);
}

bool is_file_not_compressed(const char* filePath) {
    return boost::filesystem::exists(boost::filesystem::path(filePath));
}

inline bool is_snapshot_directory(uint32_t blockTime) {
    if ((blockTime/BLOCKSEC)%BLOCKDIV == 0) {
        return true;
    }
    return false;
}

uint32_t get_users_count(uint16_t bank) {
    char usrPath[64];
    Helper::FileName::getUsr(usrPath, sizeof(usrPath), bank);
    struct stat st;
    stat(usrPath, &st);
    return (st.st_size/sizeof(user_t));
}

void db_backup(uint32_t block_path, uint16_t nodes)
{
    // used to temporary name for snapshot, resolve names colision between sparse local und/* file and currently creating db snapshot.
    const char* const snapshot_postfix = "tmp";
    char backupFilePath[64];

    if (!is_snapshot_directory(block_path)) {
        return;
    }

    Helper::FileName::getName(backupFilePath, sizeof(backupFilePath), block_path, "servers.srv");
    Helper::Servers servers(backupFilePath);
    servers.load();

    for (uint16_t bank = 1; bank < nodes; ++bank)
    {

        // open snapshot file
        Helper::FileName::getUndo(backupFilePath, sizeof(backupFilePath), block_path, bank);
        strncat(backupFilePath, snapshot_postfix, sizeof(backupFilePath) - strlen(backupFilePath) - 1);
        Helper::FileWrapper snapshotFile;
        if (!snapshotFile.open(backupFilePath, O_WRONLY | O_CREAT, 0644))
        {
            WLOG("Can't open snapshot file: %s\n" , backupFilePath);
            return;
        }

        // open bank file
        char filePath[64];
        Helper::FileName::getUsr(filePath, sizeof(filePath), bank);
        Helper::FileWrapper bankFile;
        if (!bankFile.open(filePath))
        {
            WLOG("Can't open bank file: %s\n", filePath);
            return;
        }

        // open undos from now to block_path
        uint32_t current_block = time(NULL);
        current_block -= current_block%BLOCKSEC;
        std::vector<std::shared_ptr<Helper::FileWrapper>> undoFiles;
        std::shared_ptr<Helper::FileWrapper> undo_file;
        for (uint32_t block = block_path; block <= current_block; block+=BLOCKSEC) {
            char undoPath[64];
            Helper::FileName::getBlk(undoPath, sizeof(undoPath), block);
            if (!boost::filesystem::exists(undoPath))
            {
                WLOG("Gap in block chain: %d %s\n", block, undoPath);
                break;
            }

            Helper::FileName::getUndo(undoPath, sizeof(undoPath), block, bank);

            undo_file.reset(new FileWrapper(std::string(undoPath), O_RDONLY | O_CREAT, 0644));
            if (undo_file->isOpen()) {
                undoFiles.push_back(undo_file);
            }
        }

        // for each user
        uint32_t users = servers.getNode(bank).accountCount;
        for (uint32_t user = 0; user < users; ++user)
        {
            user_t usr = {};
            // read undos and find user (user->msid != 0)
            for (auto it=undoFiles.begin(); it != undoFiles.end(); ++it)
            {
                (*it)->seek(user * sizeof(user_t), SEEK_SET);
                (*it)->read((char*)&usr, sizeof(user_t));
                if (usr.msid != 0)
                {
                    // user found
                    bankFile.seek(sizeof(user_t), SEEK_CUR);
                    break;
                }
            } // end of undo

            if (usr.msid == 0)
            {
                // if not found read user from bank file
                bankFile.read((char*)&usr, sizeof(user_t));

                if (!undoFiles.empty())
                {
                    user_t latest_record = {};
                    auto latest_undo = undoFiles.back();
                    latest_undo->seek(user * sizeof(user_t), SEEK_SET);
                    latest_undo->read((char*)&latest_record, sizeof(user_t));
                    if (latest_record.msid != 0)
                    {
                        memcpy((char*)&usr, &latest_record, sizeof(user_t));
                    }
                }
            }

            // write user to snapshot file
            if (!snapshotFile.write((char*)&usr, sizeof(user_t)))
            {
                WLOG("Can't write to bank file\n");
                return;
            }

        } // end of for each user
    } // end of for each node

    // change snapshot temp file name to correct one
    for (uint16_t bank = 1; bank < nodes; ++bank)
    {
        Helper::FileName::getUndo(backupFilePath, sizeof(backupFilePath), block_path, bank);
        strncat(backupFilePath, snapshot_postfix, sizeof(backupFilePath) - strlen(backupFilePath) - 1);
        std::rename(backupFilePath,
                    std::string(backupFilePath).substr(0, Helper::FileName::kUndoNameFixedLength).c_str());
    }
}

}
