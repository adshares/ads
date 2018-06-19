#include "gtest/gtest.h"

#include <iostream>
#include <fstream>
#include <unistd.h>
#include <fcntl.h>
#include <boost/filesystem/operations.hpp>

#include "../common/helper/tarcompressor.h"

const char* EXAMPLE_TEXT = "\nExample text string\n";
const char* ARCH_DIR_FILE_NAME = "arch.tar";
const char* DIRECTORY_PATH = "arch_dir";
const char* FILE1_DATA = "Example test string\n";
const char* FILE2_DATA = "Example\nTest\nString\n";
const char* FILE1_NAME = "/file1.txt";
const char* FILE2_NAME = "/file2.txt";
const char* DEEP_TREE = "blk/5B0/EA280";

using namespace Helper;

class CompressionTest : public ::testing::Test {
public:
    CompressionTest() {
    }

    void SetUp() {
    }

    void TearDown() {
    }

    ~CompressionTest() {
    }
};


TEST_F (CompressionTest, prepareData) {
    boost::filesystem::create_directories(DEEP_TREE);
    std::string filename(DIRECTORY_PATH);
    filename += FILE1_NAME;
    boost::filesystem::create_directory(DIRECTORY_PATH);
    std::ofstream file;
    file.open(filename.c_str(), std::ofstream::out);
    file << FILE1_DATA;
    file.close();
    filename = DIRECTORY_PATH + std::string(FILE2_NAME);
    file.open(filename, std::ofstream::out);
    file << FILE2_DATA;
    file.close();

    filename = DEEP_TREE + std::string(FILE2_NAME);
    file.open(filename, std::ofstream::out);
    file << FILE2_DATA;
    file.close();
}

TEST_F (CompressionTest, CompressDirectory) {
    TarCompressor tar(ARCH_DIR_FILE_NAME);
    EXPECT_TRUE( tar.compressDirectory(DIRECTORY_PATH) );
}

TEST_F (CompressionTest, DecompressDirectory) {
    const char* newDirectory = "test";
    boost::filesystem::create_directory(newDirectory);

    TarCompressor tar(ARCH_DIR_FILE_NAME);
    EXPECT_TRUE(tar.decompressDirectory(newDirectory));

    std::stringstream filepath{};
    filepath << newDirectory << "/" <<DIRECTORY_PATH << "/" << FILE1_NAME;
    std::stringstream ss{};

    EXPECT_TRUE(boost::filesystem::exists(filepath.str()));
    std::ifstream input(filepath.str(), std::ifstream::in);
    ss << input.rdbuf();
    EXPECT_STREQ(ss.str().c_str(), FILE1_DATA);
    input.close();

    filepath.str("");
    filepath << newDirectory << "/" <<DIRECTORY_PATH << "/" << FILE2_NAME;
    ss.str("");

    EXPECT_TRUE(boost::filesystem::exists(filepath.str()));
    input.open(filepath.str(), std::ifstream::in);
    ss << input.rdbuf();
    EXPECT_STREQ(ss.str().c_str(), FILE2_DATA);
    input.close();

    boost::filesystem::remove_all(newDirectory);
}

TEST_F (CompressionTest, DirectoriesTree) {
    const std::string tarName("5B0EA280.tar");
    {
        TarCompressor tar(tarName);
        EXPECT_TRUE(tar.compressDirectory(".", DEEP_TREE));
    }

    {
        TarCompressor tar(tarName);
        EXPECT_TRUE(tar.decompressDirectory("result"));
    }
    boost::filesystem::path path("result");
    path += FILE2_NAME;

    EXPECT_TRUE(boost::filesystem::exists(path));

    boost::filesystem::remove_all("result");
    boost::filesystem::remove(tarName);
}

TEST_F (CompressionTest, ExtractOneFile) {
    boost::filesystem::remove_all(DIRECTORY_PATH);

    TarCompressor tar(ARCH_DIR_FILE_NAME);
    std::stringstream filepath{};
    filepath << DIRECTORY_PATH << FILE2_NAME;
    EXPECT_TRUE(tar.extractFileFromArch(filepath.str().c_str()));

    filepath.str("");
    filepath << DIRECTORY_PATH << FILE2_NAME;
    std::ifstream file(filepath.str(), std::ifstream::in);
    std::stringstream ss{};
    ss << file.rdbuf();
    EXPECT_STREQ(ss.str().c_str(), FILE2_DATA);
    file.close();
}

TEST_F (CompressionTest, ExtractNotExistingFile) {
    TarCompressor tar(ARCH_DIR_FILE_NAME);
    EXPECT_FALSE(tar.extractFileFromArch("fake"));

}

TEST_F (CompressionTest, clearData) {
    boost::filesystem::remove_all(DIRECTORY_PATH);
    boost::filesystem::remove_all("blk/");
    boost::filesystem::remove(ARCH_DIR_FILE_NAME);
}

TEST(SparseFileCompression, prepare) {
    boost::filesystem::create_directory("sparse_dir");
    boost::filesystem::create_directories("sparse_result");

    int fd;
    int one_mb = 1000000;
    int one_gb = one_mb * 1000;
    struct stat st;


    for (int i=1; i<5; ++i) {
        std::string filename("sparse_dir/sparsefile_");
        filename += std::to_string(i);
        fd = open(filename.c_str(), O_WRONLY|O_CREAT, 0666);
        if (fd > 0) {
            write(fd, "begin", 5);
            lseek(fd, one_gb, SEEK_SET);
            write(fd, "end", 3);
            fstat(fd, &st);
            close(fd);
        }
    }
}

TEST(SparseFileCompression, compressBigData) {
    TarCompressor tar("sparse.tar");
    EXPECT_TRUE(tar.compressDirectory("sparse_dir"));
}

TEST(SparseFileCompression, decompressBigArch) {
    TarCompressor tar("sparse.tar");
    EXPECT_TRUE(tar.decompressDirectory("sparse_result"));
}

TEST(SparseFileCompression, extractFileFromBigArch) {
    TarCompressor tar("sparse.tar");
    EXPECT_TRUE(tar.extractFileFromArch("sparse_dir/sparsefile_4", "sparse_result/sparsefile_x"));
    EXPECT_TRUE(boost::filesystem::exists("sparse_result/sparsefile_x"));
}

TEST(SparseFileCompression, clean) {
    boost::filesystem::remove_all("sparse_dir");
    boost::filesystem::remove_all("sparse_result");
    boost::filesystem::remove("sparse.tar");
}
