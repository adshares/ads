#include "gtest/gtest.h"

#include <string.h>
#include <iostream>
#include <fstream>
#include <boost/filesystem/operations.hpp>

#include "../common/helper/blocks.h"
#include "../common/helper/blockfilereader.h"
#include "../common/helper/libarchive.h"
#include "default.h"

int m_timestamp = 1528201216;
uint16_t m_bank = 1;
uint32_t m_msid = 3;
const char* m_userid = "0001/";

TEST(BlockTest, prepareData) {
    boost::filesystem::path p("blk/5B1/68000/");
    boost::filesystem::path p2(p);
    boost::filesystem::path p3(p);
    std::ofstream file;

    p += "log/";
    p2 += "und/";
    boost::filesystem::create_directories(p);
    boost::filesystem::create_directories(p2);

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

TEST(BlockTest, openFileFromDirectoryBlock) {
    char filename[64] = {0};
    Helper::FileName::getUndo(filename, m_timestamp, m_bank);
    Helper::BlockFileReader reader(filename);
    EXPECT_TRUE(reader.isOpen());
    EXPECT_EQ(reader.getSize(), strlen("Example text"));
}

TEST(BlockTest, compressOldBlocks) {
    uint32_t timestamp = m_timestamp + ((BLOCKS_COMPRESSED_SHIFT) * BLOCKSEC);

    char dirpath[64];
    sprintf(dirpath, "blk/%03X/%05X", timestamp>>20, timestamp&0xFFFFF);
    Helper::arch_old_blocks(timestamp);
    EXPECT_FALSE(boost::filesystem::exists(dirpath));
    sprintf(dirpath, "blk/%03X/%05X%s", m_timestamp>>20, m_timestamp&0xFFFFF, Helper::ARCH_EXTENSION);
    EXPECT_TRUE(boost::filesystem::exists(dirpath));
}

TEST(BlockTest, filePathNotInBlock) {
    char filename[64] = {0};
    sprintf(filename, "servers.srv");
    Helper::BlockFileReader reader(filename);
    EXPECT_FALSE(reader.isOpen());
}

TEST(BlockTest, clearData) {
    boost::filesystem::remove_all("blk");
    boost::filesystem::remove_all("und");
    boost::filesystem::remove_all("log");
}
