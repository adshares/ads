#include "gtest/gtest.h"

#include <string.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <boost/filesystem/operations.hpp>

#include "../common/helper/libarchive.h"
#include "../common/helper/libarchivereader.h"

namespace LibArchiveSuite {

const char* DIRECTORY = "arch_test";
const char* FILE1_PATH = "arch_test/file1.txt";
const char* FILE2_PATH = "arch_test/sub_dir/file2.dat";
const char* FILE3_PATH = "arch_test/sub_dir/file3";

const char* FILE1_DATA = "File1 test\nSecond line\nThird line\n\n\n\n";
const char* FILE2_DATA = "\x41\x42\x43\x44\x45\x46\x47\x00\x01\x02\n";
const char* FILE3_DATA = "\x05\n\x04\n\x03\n\x02\n\x00\n\x01";

const int FILE1_DATA_LENGTH = strlen(FILE1_DATA);
const int FILE2_DATA_LENGTH = 11;
const int FILE3_DATA_LENGTH = 11;

const char* ARCH_NAME = "esc.arch";

TEST(LibArchiveTest, prepareData) {
    boost::filesystem::create_directories(DIRECTORY);
    boost::filesystem::path subdir(FILE2_PATH);
    boost::filesystem::create_directories(subdir.branch_path());

    std::ofstream file;
    file.open(FILE1_PATH, std::ofstream::out);
    file << FILE1_DATA;
    file.close();

    file.open(FILE2_PATH, std::ofstream::out | std::ofstream::binary);
    file.write(FILE2_DATA, FILE2_DATA_LENGTH);
    file.close();

    file.open(FILE3_PATH, std::ofstream::out | std::ofstream::binary);
    file.write(FILE3_DATA, FILE3_DATA_LENGTH);
    file.close();
}

TEST(LibArchiveTest, createArch) {
    Helper::LibArchive arch(ARCH_NAME);
    EXPECT_TRUE(arch.createArch(DIRECTORY));
}

TEST(LibArchiveTest, checkHeader) {
    std::vector< std::pair<uint32_t, std::string> > indexes;
    indexes.push_back(std::make_pair(0, FILE1_PATH));
    indexes.push_back(std::make_pair(FILE1_DATA_LENGTH, FILE2_PATH));
    indexes.push_back(std::make_pair(FILE1_DATA_LENGTH + FILE2_DATA_LENGTH, FILE3_PATH));
    int header_size = sizeof(uint16_t) +
            sizeof(uint32_t) + strlen(FILE1_PATH) + 1 +
            sizeof(uint32_t) + strlen(FILE2_PATH) + 1 +
            sizeof(uint32_t) + strlen(FILE3_PATH) + 1;

    char buffer_expected[header_size] = {};
    uint16_t ids_size = indexes.size();
    memcpy(buffer_expected, (char*)&ids_size, 2);
    char* buffer_cur = buffer_expected + 2;
    for (auto it = indexes.begin(); it != indexes.end(); ++it) {
        uint32_t offset = (*it).first + header_size;
        memcpy(buffer_cur, (char*)&offset, sizeof(uint32_t)); buffer_cur += sizeof(uint32_t);
        memcpy(buffer_cur, (*it).second.c_str(), (*it).second.length()); buffer_cur += (*it).second.length();
        memcpy(buffer_cur, "\n", 1); buffer_cur += 1;
    }

    char buffer[header_size];
    std::ifstream archfile(ARCH_NAME, std::ifstream::in | std::ifstream::binary);
    if (archfile.is_open()) {
        archfile.read((char*)buffer, header_size);
        archfile.close();
    }

    EXPECT_EQ(memcmp(buffer, buffer_expected, header_size), 0);
}

TEST(LibArchiveTest, getFileFromArch) {
    int fd;
    uint64_t offset, size;

    Helper::LibArchive lib(ARCH_NAME);
    lib.getFileHandle(FILE3_PATH, &fd, &offset, &size);
    EXPECT_GE(fd, 0);
    EXPECT_EQ(offset, 134);
    EXPECT_EQ(size, FILE3_DATA_LENGTH);

}

TEST(LibArchiveReaderTest, readFile) {
    Helper::LibArchiveReader reader(ARCH_NAME, FILE3_PATH);
    EXPECT_TRUE(reader.isOpen());

    char buffer[10];
    int shift = 5;
    std::string expected_result = std::string(FILE3_DATA).substr(shift, std::string::npos);
    reader.lseek(shift, SEEK_SET);
    reader.read(buffer, FILE3_DATA_LENGTH - shift);

    EXPECT_STREQ(expected_result.c_str(), buffer);

    EXPECT_TRUE(reader.open(FILE1_PATH));
    EXPECT_EQ(reader.getSize(), FILE1_DATA_LENGTH);

    reader.lseek(FILE1_DATA_LENGTH + 10, SEEK_SET); // move over size, should set to the end of file
    reader.lseek(-1, SEEK_CUR); // set to last character
    char last_ch;
    reader.read((char*)&last_ch, 1);
    EXPECT_EQ(last_ch, FILE1_DATA[FILE1_DATA_LENGTH-1]);
}

TEST(LibArchiveReaderTest, clean) {
    boost::filesystem::remove_all(DIRECTORY);
    boost::filesystem::remove(ARCH_NAME);
}

}
