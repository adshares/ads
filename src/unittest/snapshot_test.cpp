#include "gtest/gtest.h"

#include <fstream>
#include <boost/filesystem.hpp>
#include <sys/stat.h>
#include <fcntl.h>
#include <algorithm>
#include <string>
#include <iterator>

#include "default.hpp"
#include "helper/blocks.h"
#include "helper/servers.h"

const int kNodeId = 1;
const std::string kMainDirectory = "blk";
const std::string kDBDirectory = "usr/";
const std::string kSnapshotDirectory = "blk/5AB/79000/und/";
const int kStartBlockDec = 1521979360;
const int kSnapshotBlock = kStartBlockDec + BLOCKSEC;

/*
 *
 * |    | |    |  |
 * |  1 | |  2 |  | 3
 * |____| |____|  |__x   <-- interrupt point
 *           ^
 *           |____ snapshot directory
 *
 *             und/0001.dat files
 * | block|  user   |  msid   |  weight |
 * |------|---------|---------|---------|
 * |  1   |   0     |   11    |   10    |
 * |      |   1     |   1     |   1     |
 * --------------------------------------
 * |  2   |   0     |   12    |   10    |
 * |      |   -     |   -     |   -     | <-- sparse file, later replaced by snapshot
 * |      |   2     |   1     |   1     |
 * --------------------------------------
 * |  3   |   0     |   13    |   3     |
 * |      |   1     |   2     |   5     |
 * |      |   2     |   2     |   4     |
 * --------------------------------------
 *
 *              usr/0001.dat
 * |      |         |         |         |
 * |      |    0    |   14    |    4    |
 * |      |    1    |    3    |    4    |
 * |      |    2    |    3    |    4    |
 * |      |    3    |    1    |    1    |
 * --------------------------------------
 *
 *          snapshot file
 * |      |         |         |         |
 * |      |    0    |    12   |    10   |
 * |      |    1    |    2    |    5    |
 * |      |    2    |    1    |    1    |
 * |      |         |         |         |
 * --------------------------------------
 */

user_t snapshot_usrs[3];

void clean()
{
    boost::filesystem::remove_all("blk");
    boost::filesystem::remove_all("usr");
}

TEST(SnapshotTest, prepareData)
{

    clean();

    snapshot_usrs[0].msid = 12;
    snapshot_usrs[0].weight = 10;
    snapshot_usrs[1].msid = 2;
    snapshot_usrs[1].weight = 5;
    snapshot_usrs[2].msid = 1;
    snapshot_usrs[2].weight = 1;

    char filename[64];

    boost::filesystem::create_directories("usr");

    uint32_t block_time = kStartBlockDec;
    Helper::FileName::getBlk(filename, sizeof(filename), block_time, "und");
    boost::filesystem::create_directories(filename);
    block_time += BLOCKSEC;
    Helper::FileName::getBlk(filename, sizeof(filename), block_time, "und");
    boost::filesystem::create_directories(filename);
    block_time += BLOCKSEC;
    Helper::FileName::getBlk(filename, sizeof(filename), block_time, "und");
    boost::filesystem::create_directories(filename);

    block_time = kStartBlockDec;
    std::ofstream file;
    user_t usr;

    // 1st block
    Helper::FileName::getUndo(filename, sizeof(filename), block_time, kNodeId);
    file.open(filename, std::ofstream::out | std::ofstream::binary);
    usr = {};
    usr.msid = 11;
    usr.weight = 10;
    file.write((char*)&usr, sizeof(user_t));
    usr.msid = 1;
    usr.weight = 1;
    file.write((char*)&usr, sizeof(user_t));
    file.close();

    // 2nd block
    block_time += BLOCKSEC;
    Helper::FileName::getUndo(filename, sizeof(filename), block_time, kNodeId);
    file.open(filename, std::ofstream::out | std::ofstream::binary);
    usr = {};
    usr.msid = 12;
    usr.weight = 10;
    file.write((char*)&usr, sizeof(user_t));
    file.seekp(2 * sizeof(user_t));
    usr.msid = 1;
    usr.weight = 1;
    file.write((char*)&usr, sizeof(user_t));
    file.close();

    // 3rd block
    block_time += BLOCKSEC;
    Helper::FileName::getUndo(filename, sizeof(filename), block_time, kNodeId);
    file.open(filename, std::ofstream::out | std::ofstream::binary);
    usr = {};
    usr.msid = 13;
    usr.weight = 3;
    file.write((char*)&usr, sizeof(user_t));
    usr.msid = 2;
    usr.weight = 5;
    file.write((char*)&usr, sizeof(user_t));
    usr.msid = 2;
    usr.weight = 4;
    file.write((char*)&usr, sizeof(user_t));
    file.close();

    // create usr file
    Helper::FileName::getUsr(filename, sizeof(filename), kNodeId);
    file.open(filename, std::ofstream::out | std::ofstream::binary);
    usr = {};
    usr.msid = 14;
    usr.weight = 4;
    file.write((char*)&usr, sizeof(user_t));
    usr = {};
    usr.msid = 3;
    usr.weight = 4;
    file.write((char*)&usr, sizeof(user_t));
    usr = {};
    usr.msid = 3;
    usr.weight = 4;
    file.write((char*)&usr, sizeof(user_t));
    usr = {};
    usr.msid = 1;
    usr.weight = 1;
    file.write((char*)&usr, sizeof(user_t));
    file.close();

    // servers.srv in snapshot directory
    Helper::FileName::getName(filename, sizeof(filename), kSnapshotBlock, "servers.srv");
    Helper::ServersHeader header{};
    header.nodesCount = 2;
    Helper::ServersNode node{};
    node.accountCount = 3;
    file.open(filename, std::ofstream::out | std::ofstream::binary);
    file.write((char*)&header, sizeof(header));
    file.write((char*)&node, sizeof(node));
    file.write((char*)&node, sizeof(node));
    file.close();
}

bool compareUsr(const user_t& usr1, const user_t& usr2)
{
    if (usr1.msid   != usr2.msid  ) return false;
    if (usr1.time   != usr2.time  ) return false;
    if (usr1.lpath  != usr2.lpath ) return false;
    if (usr1.user   != usr2.user  ) return false;
    if (usr1.node   != usr2.node  ) return false;
    if (usr1.stat   != usr2.stat  ) return false;
    if (usr1.rpath  != usr2.rpath ) return false;
    if (usr1.weight != usr2.weight) return false;

    return true;
}

TEST(SnapshotTest, createSnapshot)
{
    char filename[64];
    Helper::FileName::getUndo(filename, sizeof(filename), kSnapshotBlock, kNodeId);
    std::stringstream src_data{}, dst_data{};
    std::ifstream file(filename, std::ifstream::in | std::ifstream::binary);
    src_data << file.rdbuf();
    file.close();

    Helper::db_backup(kSnapshotBlock, 2); // 2 nodes incl. 0

    file.open(filename, std::ifstream::in | std::ifstream::binary);
    dst_data << file.rdbuf();

    EXPECT_NE(src_data.str(), dst_data.str());

    file.seekg(0, std::ios_base::beg);

    user_t usr{};
    file.read((char*)&usr, sizeof(user_t));
    EXPECT_TRUE(compareUsr(usr, snapshot_usrs[0]));

    usr = {};
    file.read((char*)&usr, sizeof(user_t));
    EXPECT_TRUE(compareUsr(usr, snapshot_usrs[1]));

    usr = {};
    file.read((char*)&usr, sizeof(user_t));
    EXPECT_TRUE(compareUsr(usr, snapshot_usrs[2]));

}

TEST(SnapshotTest, createSnapshotNotDividendBlock)
{
    char filename[64];
    uint32_t block_time = kSnapshotBlock + BLOCKSEC;
    Helper::FileName::getUndo(filename, sizeof(filename), block_time, kNodeId);
    std::stringstream src_data{}, dst_data{};
    std::ifstream file;
    file.open(filename, std::ifstream::in | std::ifstream::binary);
    src_data << file.rdbuf();
    file.close();

    Helper::db_backup(block_time, 2);

    file.open(filename, std::ifstream::in | std::ifstream::binary);
    dst_data << file.rdbuf();
    file.close();

    EXPECT_EQ(src_data.str(), dst_data.str());
}

TEST(SnapshotTest, clean)
{
    clean();
}
