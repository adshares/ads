#include "getaccountshandler.h"
#include "command/getaccounts.h"
#include "../office.hpp"

enum ErrorCode {
    eNone = 0,
    eBadPath = 1,
    eBadNode = 2,
    eBankNotFound = 3,
    eUndoNotFound = 4
};

GetAccountsHandler::GetAccountsHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}

void GetAccountsHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    try {
        m_command = std::unique_ptr<GetAccounts>(dynamic_cast<GetAccounts*>(command.release()));
    } catch (std::bad_cast& bc) {
        DLOG("OnGetAccounts bad_cast caught: %s", bc.what());
        return;
    }
}

void GetAccountsHandler::onExecute() {
    assert(m_command);

    ErrorCode errorCode = ErrorCode::eNone;
    uint32_t destBank = m_command->getDestBankId();
    uint32_t path=m_offi.last_path();
    char filename[64];
    int fd, ud;

    sprintf(filename,"usr/%04X.dat", destBank);
    fd = open(filename,O_RDONLY);
    if (fd < 0) {
        errorCode = ErrorCode::eBankNotFound;
    } else {
        sprintf(filename,"blk/%03X/%05X/und/%04X.dat",path>>20,path&0xFFFFF, destBank);
        ud = open(filename,O_RDONLY);
        if(ud < 0) {
            errorCode = ErrorCode::eUndoNotFound;
            close(fd);
        }
    }

    if (errorCode) {
        boost::asio::write(m_socket,boost::asio::buffer((uint8_t*)&errorCode,4));
        return;
    }

    uint32_t users = m_offi.last_users(destBank);
    uint32_t len=users*sizeof(user_t);
    uint8_t *data=(uint8_t*)malloc(4+len);
    uint8_t *dp=data+4;
    for(uint32_t user=0; user<users; user++,dp+=sizeof(user_t)) {
        user_t u;
        u.msid=0;
        read(fd,dp,sizeof(user_t));
        read(ud,&u,sizeof(user_t));
        if(u.msid!=0) {
            memcpy(dp,&u,sizeof(user_t));
        }
    }
    close(ud);
    close(fd);
    memcpy(data,&len,4);
    boost::asio::write(m_socket,boost::asio::buffer(data,4+len));
    free(data);
}

bool GetAccountsHandler::onValidate() {
    ErrorCode errorCode = ErrorCode::eNone;
    uint32_t path=m_offi.last_path();

    if (m_command->getBlockId() != path) {
        errorCode = ErrorCode::eBadPath;
    }
    else if (m_command->getDestBankId() > m_offi.last_nodes()) {
        errorCode = ErrorCode::eBadNode;
    }

    if (errorCode) {
        boost::asio::write(m_socket,boost::asio::buffer((uint8_t*)&errorCode,4));
        return false;
    }

    return true;
}
