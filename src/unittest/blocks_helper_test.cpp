#include "gtest/gtest.h"

#include <string.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <boost/filesystem/operations.hpp>

#include "../common/helper/blocks.h"
#include "../common/helper/blockfilereader.h"
#include "../common/helper/libarchive.h"
#include "../common/helper/serversheader.h"
#include "../common/parser/msglistparser.h"
#include "default.h"

int m_timestamp = 1528201216;
uint16_t m_bank = 1;
uint32_t m_msid = 3;
const char* m_userid = "0001/";

TEST(BlockTest, prepareData) {
    boost::filesystem::path p("blk/5B1/68000");
    std::ofstream file;

    boost::filesystem::create_directories(p);

    p += "/header.hdr";
    Helper::ServersHeader hdr = {};
    memset((char*)&hdr, 1, sizeof(hdr));
    hdr.messageCount = 2;
    file.open(p.string(), std::ofstream::out | std::ofstream::binary);
    file.write((char*)&hdr, sizeof(Helper::ServersHeader));
    file.close();

    p.remove_filename();
    p += "/signatures.ok";
    file.open(p.string(), std::ofstream::out | std::ofstream::binary);
    char buffer[66];
    file.write(buffer, sizeof(buffer));
    file.close();

    p.remove_filename();
    p += "/msglist.dat";
    file.open(p.string(), std::ofstream::out | std::ofstream::binary);
    MessageListHeader header = {};
    header.num_of_msg = 2;
    file.write((char*)&header, sizeof(header));
    for (unsigned int i=0; i<2; ++i) {
        MessageRecord item = {};
        item.node_id = m_bank;
        item.node_msid = m_msid + i;
        file.write((char*)&item, sizeof(MessageRecord));
    }
    file.close();


    p.remove_filename();
    p += "/03_0001_00000003.msg";
    file.open(p.string(), std::ofstream::out | std::ofstream::binary);
    uint8_t type = 1;
    uint32_t size = 32;
    memcpy(buffer, &type, 1);
    memcpy(buffer+1, &size, 3);
    file.write(buffer, 32);
    file.close();

    p.remove_filename();
    p += "/03_0001_00000004.msg";
    file.open(p.string(), std::ofstream::out | std::ofstream::binary);
    file.write(buffer, 32);
    file.close();


    p.remove_filename();
    p += "/fake_file.txt";
    file.open(p.string(), std::ofstream::out);
    file.write(buffer, 32);
    file.close();
}

TEST(BlockTest, openFileFromDirectoryBlock) {
    char filename[64] = {0};
    Helper::FileName::getMsg(filename, sizeof(filename), m_timestamp, 3, m_bank, m_msid);
    Helper::BlockFileReader reader(filename);
    EXPECT_TRUE(reader.isOpen());
}

TEST(BlockTest, compressOldBlocks) {
#ifdef BLOCKS_COMRESSED_SHIFT

    int expectedFileSize =
            sizeof(Helper::ServersHeader) +
            (7*4) + // jump for 7 files (5 + 2 msgs)
            66 + //signatures.ok
            112 + //msglist.dat
            (2*32); // 2 messages

    uint32_t timestamp = m_timestamp + ((BLOCKS_COMPRESSED_SHIFT) * BLOCKSEC);

    char dirpath[64];
    snprintf(dirpath, sizeof(dirpath), "blk/%03X/%05X", timestamp>>20, timestamp&0xFFFFF);
    Helper::arch_old_blocks(timestamp);
    EXPECT_FALSE(boost::filesystem::exists(dirpath));
    snprintf(dirpath, sizeof(dirpath), "blk/%03X/%05X%s", m_timestamp>>20, m_timestamp&0xFFFFF, Helper::ARCH_EXTENSION);
    EXPECT_TRUE(boost::filesystem::exists(dirpath));

    struct stat st = {};
    stat(dirpath, &st);
    EXPECT_EQ(expectedFileSize, st.st_size);
#endif
}

TEST(BlockTest, excludedFile) {
    // excluded file exists in directory but shouldnt be in arch, only fixed list of file types.
    boost::filesystem::path p("blk/5B1/68000/fake_file.txt");
    Helper::BlockFileReader reader(p.c_str());
    EXPECT_FALSE(reader.isOpen());
}

TEST(BlockTest, filePathNotInBlock) {
    boost::filesystem::path p("blk/5B1/68000/servers.srv");
    Helper::BlockFileReader reader(p.c_str());
    EXPECT_FALSE(reader.isOpen());
}

TEST(BlockTest, fileFromArch) {
    {
        boost::filesystem::path p("blk/5B1/68000/signatures.ok");
        Helper::BlockFileReader reader(p.c_str());
        EXPECT_TRUE(reader.isOpen());
    }

    {
        boost::filesystem::path p("blk/5B1/68000/msglist.dat");
        Helper::BlockFileReader reader(p.c_str());
        EXPECT_TRUE(reader.isOpen());
    }

    {
        boost::filesystem::path p("blk/5B1/68000/03_0001_00000003.msg");
        Helper::BlockFileReader reader(p.c_str());
        EXPECT_TRUE(reader.isOpen());
    }
    {
        boost::filesystem::path p("blk/5B1/68000/03_0001_00000004.msg");
        Helper::BlockFileReader reader(p.c_str());
        EXPECT_TRUE(reader.isOpen());
    }
}

TEST(BlockTest, msgFileNotExists) {
    boost::filesystem::path p("blk/5B1/68000/03_0001_000000A0.msg");
    Helper::BlockFileReader reader(p.c_str());
    EXPECT_FALSE(reader.isOpen());
}

TEST(BlockTest, clearData) {
    boost::filesystem::remove_all("blk");
    boost::filesystem::remove_all("und");
    boost::filesystem::remove_all("log");
}
