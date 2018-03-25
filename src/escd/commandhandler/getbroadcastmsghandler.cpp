#include "getbroadcastmsghandler.h"

#include <fstream>
#include "command/getbroadcastmsg.h"
#include "../office.hpp"
#include "helper/hash.h"


GetBroadcastMsgHandler::GetBroadcastMsgHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}

void GetBroadcastMsgHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    try {
        m_command = std::unique_ptr<GetBroadcastMsg>(dynamic_cast<GetBroadcastMsg*>(command.release()));
    } catch (std::bad_cast& bc) {
        DLOG("GetBroadcastMsg bad_cast caught: %s", bc.what());
        return;
    }
}

void GetBroadcastMsgHandler::onExecute() {
    assert(m_command);

    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;
    uint32_t blockTime = m_command->getBlockTime();
    uint32_t path, lpath, size = 0;

    lpath = m_offi.last_path();
    if(!blockTime) {
        path = lpath;
    } else {
        path = blockTime - blockTime%BLOCKSEC;
        if (path > lpath) {
            errorCode = ErrorCodes::Code::eBroadcastNotReady;
        }
    }

    std::vector<std::string> fileBuffer;
    if (!errorCode) {
        char filename[64];
        sprintf(filename,"blk/%03X/%05X/bro.log",path>>20,path&0xFFFFF);

        std::ifstream broadcastFile(filename, std::ifstream::binary | std::ifstream::in);
        if (!broadcastFile.is_open()) {
            errorCode = ErrorCodes::Code::eNoBroadcastFile;
        } else {
            auto ssBuffer = std::stringstream{};
            ssBuffer << broadcastFile.rdbuf();
            size = ssBuffer.str().length();
            int remainData = (size > MAX_BLG_SIZE) ? MAX_BLG_SIZE : size;
            while (remainData > 0) {
                int sizeOfBlock = (remainData > MESSAGE_CHUNK) ? MESSAGE_CHUNK : remainData;
                char messageBlock[sizeOfBlock];
                ssBuffer.read(messageBlock, sizeOfBlock);
                remainData -= sizeOfBlock;
                fileBuffer.push_back(std::string(messageBlock, sizeOfBlock));
            }
            broadcastFile.close();
        }
    }

    try {
        boost::asio::write(m_socket, boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));
        if(!errorCode) {
            BroadcastFileHeader header{path, lpath, size};
            boost::asio::write(m_socket, boost::asio::buffer(&header, sizeof(header)));
            // message is sending in parts of max size MESSAGE_CHUNK each block
            for (auto &it : fileBuffer) {
                boost::asio::write(m_socket, boost::asio::buffer(it));
            }
        }
    } catch (std::exception& e) {
        DLOG("Responding to client %08X error: %s\n", m_usera.user, e.what());
    }


}

bool GetBroadcastMsgHandler::onValidate() {

    auto        startedTime     = time(NULL);
    int32_t diff = m_command->getBlockTime() - startedTime;
    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;

    if(diff>1) {
        DLOG("ERROR: time in the future (%d>1s)\n", diff);
        errorCode = ErrorCodes::Code::eTimeInFuture;
    }
    else if(m_command->getBankId() != m_offi.svid) {
        errorCode = ErrorCodes::Code::eBankNotFound;
    }

    if (errorCode) {
        try {
            boost::asio::write(m_socket, boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));
        } catch (std::exception& e) {
            DLOG("Responding to client %08X error: %s\n", m_usera.user, e.what());
        }
        return false;
    }


    return true;
}
