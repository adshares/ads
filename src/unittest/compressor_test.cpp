#include "gtest/gtest.h"

#include "../common/helper/tarcompressor.h"

#include <iostream>
#include <fstream>
#include <boost/filesystem/operations.hpp>

const char* EXAMPLE_TEXT = "\nExample text string\n";
const char* ARCH_FILE_NAME = "arch.gz";
const char* ARCH_DIR_FILE_NAME = "arch.tar.gz";
const char* DIRECTORY_PATH = "arch_dir";
const char* FILE1_DATA = "Example test string\n";
const char* FILE2_DATA = "Example\nTest\nString\n";
const char* FILE1_NAME = "/file1.txt";
const char* FILE2_NAME = "/file2.txt";
const char* DEEP_TREE = "blk/5B0/EA280";

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


void prepareData() {
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

void clearData() {
    boost::filesystem::remove_all(DIRECTORY_PATH);
    boost::filesystem::remove_all("blk/");
    boost::filesystem::remove(ARCH_FILE_NAME);
    boost::filesystem::remove(ARCH_DIR_FILE_NAME);
}

//TEST_F (CompressionTest, CompressStringToFile) {
//    TarCompressor tar(ARCH_FILE_NAME);
//    EXPECT_TRUE (tar.compress(EXAMPLE_TEXT));
//}

//TEST_F (CompressionTest, DecompressStringFromArch) {
//    std::string tmpResultFile = "string_from_arch.txt";
//    TarCompressor tar(ARCH_FILE_NAME);
//    EXPECT_TRUE (tar.decompress(tmpResultFile.c_str()));

//    std::ifstream file(tmpResultFile, std::ifstream::in);
//    std::stringstream ss{};
//    ss << file.rdbuf();
//    EXPECT_STREQ(ss.str().c_str(), EXAMPLE_TEXT);
//    boost::filesystem::remove(tmpResultFile);
//}

//TEST_F (CompressionTest, TryDecompressNotExistingFile) {
//    const char* fake_path = "fake_path_to_file";
//    if (!boost::filesystem::exists(fake_path)) {
//        TarCompressor tar(fake_path);
//        EXPECT_FALSE (tar.decompress("fake"));
//    } else {
//        std::cerr << "Can't perform test, "<<fake_path<<" exists, remove it first."<<std::endl;
//        EXPECT_FALSE(true);
//    }
//}

TEST_F (CompressionTest, CompressDirectory) {
    prepareData();
    TarCompressor tar(ARCH_DIR_FILE_NAME);
    EXPECT_TRUE( tar.compressDirectory(DIRECTORY_PATH) );
}

TEST_F (CompressionTest, DecompressDirectory) {
    const char* newDirectory = "test";
    boost::filesystem::create_directory(newDirectory);

    TarCompressor tar(ARCH_DIR_FILE_NAME);
    EXPECT_TRUE(tar.decompressDirectory(newDirectory));

    std::stringstream filepath{};
    filepath << newDirectory << FILE1_NAME;
    std::stringstream ss{};

    EXPECT_TRUE(boost::filesystem::exists(filepath.str()));
    std::ifstream input(filepath.str(), std::ifstream::in);
    ss << input.rdbuf();
    EXPECT_STREQ(ss.str().c_str(), FILE1_DATA);
    input.close();

    filepath.str("");
    filepath << newDirectory << FILE2_NAME;
    ss.str("");

    EXPECT_TRUE(boost::filesystem::exists(filepath.str()));
    input.open(filepath.str(), std::ifstream::in);
    ss << input.rdbuf();
    EXPECT_STREQ(ss.str().c_str(), FILE2_DATA);
    input.close();

    boost::filesystem::remove_all(newDirectory);
}

TEST_F (CompressionTest, DirectoriesTree) {
    const std::string tarName("5B0EA280.tar.gz");
    {
        TarCompressor tar(tarName, CompressionType::eGZIP);
        EXPECT_TRUE(tar.compressDirectory(DEEP_TREE));
    }

    {
        TarCompressor tar(tarName, CompressionType::eGZIP);
        EXPECT_TRUE(tar.decompressDirectory("result"));
    }
    boost::filesystem::path path("result");
    path += FILE2_NAME;

    EXPECT_TRUE(boost::filesystem::exists(path));

    boost::filesystem::remove_all("result");
    boost::filesystem::remove(tarName);
}

TEST_F (CompressionTest, ExtractFile) {
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
    clearData();
}

//TEST_F (CompressionTest, ExtractNotExistingFile) {
//    TarCompressor tar(ARCH_DIR_FILE_NAME);
//    EXPECT_FALSE(tar.extractFileFromArch("fake"));

//}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
