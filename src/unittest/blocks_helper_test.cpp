#include "gtest/gtest.h"

#include <string.h>
#include <iostream>
#include <fstream>
#include <boost/filesystem/operations.hpp>

#include "../common/helper/blocks.h"
#include "default.h"

int m_timestamp = 1528201216;
uint16_t m_bank = 1;
uint32_t m_msid = 3;

TEST(BlockTest, prepareData) {
    boost::filesystem::path p("blk/5B1/68000/");
    boost::filesystem::path p2(p);
    boost::filesystem::path p3(p);
    std::ofstream file;

    p += "log/";
    p2 += "und/";
    boost::filesystem::create_directories(p);
    boost::filesystem::create_directories(p2);
    boost::filesystem::create_directory("tmp/");

    p3 += "delta.txt";
    file.open(p3.string(), std::ofstream::out | std::ofstream::binary);
    file << "Example text";
    file.close();

    p += "0001_00000003.log";
    file.open(p.string(), std::ofstream::out | std::ofstream::binary);
    file << "Example text";
    file.close();

    p2 += "0001.dat";
    file.open(p2.string(), std::ofstream::out | std::ofstream::binary);
    file << "Example text";
    file.close();
}

TEST(BlockTest, getFileFromBlock) {
    char filename[64] = {0};
    Helper::FileName::getLog(filename, m_timestamp, m_bank, m_msid);
    EXPECT_EQ(strlen(filename), Helper::FileName::kLogNameFixedLength);
    EXPECT_TRUE(Helper::get_file_from_block(filename));

    filename[0] = 0;
    Helper::FileName::getUndo(filename, m_timestamp, m_bank);
    EXPECT_EQ(strlen(filename), Helper::FileName::kUndoNameFixedLength);
    EXPECT_TRUE(Helper::get_file_from_block(filename));
}

TEST(BlockTest, openFileFromDirectoryBlock) {
    char filename[64] = {0};
    Helper::FileName::getUndo(filename, m_timestamp, m_bank);
    int fd = Helper::open_block_file(filename);
    EXPECT_GE(fd, 0);
    if (fd >= 0) {
        close(fd);
    }
}

TEST(BlockTest, tarOldBlock) {
    uint32_t timestamp = m_timestamp + ((BLOCKS_COMPRESSED_SHIFT) * BLOCKSEC);

    char dirpath[64];
    sprintf(dirpath, "blk/%03X/%05X", timestamp>>20, timestamp&0xFFFFF);
    Helper::tar_old_blocks(timestamp);
    EXPECT_FALSE(boost::filesystem::exists(dirpath));
    sprintf(dirpath, "blk/%03X/%05X.tar.gz", m_timestamp>>20, m_timestamp&0xFFFFF);
    EXPECT_TRUE(boost::filesystem::exists(dirpath));
}

TEST(BlockTest, getFileFromArch) {
    char filename[64] = {0};

    // directory shouldnt exists here, removed in tarOldBlocks test
    sprintf(filename, "blk/%03X/%05X", m_timestamp>>20, m_timestamp&0xFFFFF);
    ASSERT_FALSE(boost::filesystem::exists(filename));

    filename[0] = 0;
    Helper::FileName::getLog(filename, m_timestamp, m_bank, m_msid);
    EXPECT_EQ(strlen(filename), Helper::FileName::kLogNameFixedLength);

    std::string newpath(filename);
    newpath.replace(0, 4, TMP_DIR); // replace blk with tmp
    EXPECT_TRUE(Helper::get_file_from_block(filename));
    EXPECT_TRUE(boost::filesystem::exists(newpath));

    filename[0] = 0;
    Helper::FileName::getUndo(filename, m_timestamp, m_bank);
    EXPECT_EQ(strlen(filename), Helper::FileName::kUndoNameFixedLength);

    newpath = filename;
    newpath.replace(0, 4, TMP_DIR); // replace blk with tmp
    EXPECT_TRUE(Helper::get_file_from_block(filename));
    EXPECT_TRUE(boost::filesystem::exists(newpath));
}

TEST(BlockTest, openFileFromArch) {
    char filename[64] = {0};
    // directory shouldnt exists here, removed in tarOldBlocks test
    sprintf(filename, "blk/%03X/%05X", m_timestamp>>20, m_timestamp&0xFFFFF);
    ASSERT_FALSE(boost::filesystem::exists(filename));

    filename[0] = 0;
    Helper::FileName::getUndo(filename, m_timestamp, m_bank);
    int fd = Helper::open_block_file(filename);
    EXPECT_GE(fd, 0);
    if (fd >=0 ) {
        close(fd);
    }
}

TEST(BlockTest, clearData) {
    boost::filesystem::remove_all("blk");
    boost::filesystem::remove_all("tmp");
    boost::filesystem::remove_all("und");
    boost::filesystem::remove_all("log");
}
