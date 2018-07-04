#include "libarchive.h"

#include <vector>
#include <string>
#include <ctype.h>
#include <iostream>
#include <fstream>
#include <map>
#include <sstream>
#include <sys/stat.h>
#include <fcntl.h>
#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>
#include <algorithm>

#include "serversheader.h"
#include "parser/msglistparser.h"

namespace Helper {

const int SIZE_OF_JUMP_FIELD = sizeof(uint32_t);

LibArchive::LibArchive(const char* archPath) : m_filePath(archPath)
{

}

bool LibArchive::createArch(const char* directoryPath)
{
    if (!m_filePath) return false;

    std::ofstream outputFile;
    outputFile.exceptions(std::ofstream::failbit | std::ofstream::badbit);
    outputFile.open(m_filePath, std::ofstream::out | std::ofstream::binary);
    if (!outputFile.is_open()) return false;

    boost::filesystem::path dirPath(directoryPath);
    dirPath += "/";
    boost::filesystem::directory_iterator directory(dirPath), end;

    std::vector< std::pair<std::string, uint32_t> > def_file_index = {
        {"header.hdr", 0},
        {"servers.srv", 0},
        {"signatures.ok", 0},
        {"signatures.no", 0},
        {"hlog.hlg", 0},
        {"msglist.dat",0}
    };

    std::map<std::string, uint32_t> msg_file_index;

    // preapre header
    struct stat st = {};
    for (auto it = def_file_index.begin(); it != def_file_index.end(); ++it)
    {
        boost::filesystem::path filepath(dirPath);
        filepath += it->first;
        if (stat(filepath.string().c_str(), &st) == 0)
        {
            it->second = st.st_size;
        }

    }

    // get .msg files info
    while (directory != end)
    {
        if (boost::filesystem::is_regular_file(*directory) && directory->path().extension() == ".msg")
        {
            stat(directory->path().string().c_str(), &st);
            if (st.st_size > 0)
            {
                msg_file_index[directory->path().filename().string()] = st.st_size;
            }
        }
        ++directory;
    }

    uint32_t offset = def_file_index[0].second;
    offset += (def_file_index.size()-1 + msg_file_index.size()) * SIZE_OF_JUMP_FIELD;

    std::ifstream entry;
    entry.exceptions(std::ifstream::failbit | std::ifstream::badbit | std::ifstream::eofbit);
    try
    {
        // write header
        boost::filesystem::path fileFullPath(dirPath);
        fileFullPath += "header.hdr";
        entry.open(fileFullPath.string(), std::ifstream::in | std::ifstream::binary);
        outputFile << entry.rdbuf();
        entry.close();

        for (auto it = def_file_index.begin() + 1; it != def_file_index.end(); ++it)
        {
            uint32_t position = (it->second > 0) ? offset : 0;
            outputFile.write((char*)&position, SIZE_OF_JUMP_FIELD);
            offset += it->second;
        }

        for (auto it : msg_file_index)
        {
            uint32_t position = offset;
            outputFile.write((char*)&position, SIZE_OF_JUMP_FIELD);
            offset += it.second;
        }

        // put defined files
        for (auto it = def_file_index.begin() + 1; it != def_file_index.end(); ++it)
        {
            if (it->second > 0)
            {
                boost::filesystem::path filepath(dirPath);
                filepath += it->first;
                entry.open(filepath.string(), std::ifstream::in | std::ifstream::binary);
                outputFile << entry.rdbuf();
                entry.close();
            }
        }

        // put msg files
        for (auto it : msg_file_index)
        {
            boost::filesystem::path filepath(dirPath);
            filepath += it.first;
            entry.open(filepath.string(), std::ifstream::in | std::ifstream::binary);
            outputFile << entry.rdbuf();
            entry.close();
        }

        outputFile.close();

    } catch (std::exception& e)
    {
        std::cerr << "Exception: "<<e.what()<<std::endl;
        outputFile.close();
        boost::filesystem::remove(m_filePath);
        return false;
    }

    return true;
}

bool LibArchive::getFileHandle(const char* filepath, int *file_descriptor, uint64_t *offset, uint64_t *file_size)
{
    std::vector< std::pair<std::string, uint32_t> > def_file_index = {
        {"header.hdr", 0},
        {"servers.srv", 0},
        {"signatures.ok", 0},
        {"signatures.no", 0},
        {"hlog.hlg", 0},
        {"msglist.dat", 0}
    };

    struct stat st = {};
    stat(m_filePath, &st);

    bool found = false;
    boost::filesystem::path filePath(filepath);
    std::ifstream arch(m_filePath, std::iostream::in | std::iostream::binary);
    if (arch.is_open())
    {
        Helper::ServersHeader serversHeader;
        arch.read((char*)&serversHeader, sizeof(Helper::ServersHeader));

        for(auto it=def_file_index.begin()+1; it != def_file_index.end(); ++it)
        {
            uint32_t jumpCursor;
            arch.read((char*)&jumpCursor, sizeof(jumpCursor));
            it->second = jumpCursor;
        }

        if (filePath.extension() == ".msg")
        {
            if (def_file_index.back().second <= 0) return false;

            uint32_t node_id, hash, msg_id;
            sscanf(filepath, "%2X_%4X_%8X", &hash, &node_id, &msg_id);

            MessageListHeader msgListHeader = {};
            uint32_t cursor_to_first_msg_header = arch.tellg();

            arch.seekg(def_file_index.back().second, std::ios::beg);
            arch.read((char*)&msgListHeader, sizeof(msgListHeader));

            for (unsigned int i=0; i<msgListHeader.num_of_msg; ++i)
            {
                MessageRecord item;
                arch.read((char*)&item, sizeof(item));
                // check is msg exists in this package
                if (item.node_id == node_id && item.node_msid == msg_id)
                {
                    // read offset
                    uint32_t jumpToMsg = cursor_to_first_msg_header + sizeof(uint32_t)*i;
                    arch.seekg(jumpToMsg, std::ios::beg);
                    arch.read((char*)offset, SIZE_OF_JUMP_FIELD);


                    // read msg size
                    uint32_t next_item_pos = st.st_size;
                    if (i < msgListHeader.num_of_msg - 1)
                    {
                        arch.read((char*)&next_item_pos, SIZE_OF_JUMP_FIELD);
                    }

                    *file_size = next_item_pos - *offset;
                    found = true;
                    break;
                }
            }

        } else if (filePath.filename() == "msglist.dat")
        {
            if (def_file_index.back().second <= 0) return false;

            *offset = def_file_index.back().second;
            if (serversHeader.messageCount == 0)
            {
                *file_size = st.st_size - def_file_index.back().second;
            } else
            {
                uint32_t firstMsgJumpCursor;
                arch.read((char*)&firstMsgJumpCursor, sizeof(firstMsgJumpCursor));
                *file_size = firstMsgJumpCursor - def_file_index.back().second;
            }
            found = true;

        } else
        {
            for (unsigned int i=0; i<def_file_index.size()-1; ++i)
            {
                if (def_file_index[i].first.compare(filepath) == 0)
                {
                    if (i>0 && def_file_index[i].second <=0) return false;
                    found = true;
                    *offset = def_file_index[i].second;

                    uint32_t next_item_pos = st.st_size;
                    for (++i; i<def_file_index.size(); ++i)
                    {
                        if (def_file_index[i].second > 0)
                        {
                            next_item_pos = def_file_index[i].second;
                            break;
                        }
                    }
                    *file_size = next_item_pos - *offset;
                    break;
                }
            }
        }

        arch.close();

    }

    if (found)
    {
        int fd = open(m_filePath, O_RDONLY);
        if (fd <= 0) return false;
        *file_descriptor = fd;
        return true;
    }

    return false;
}

}
