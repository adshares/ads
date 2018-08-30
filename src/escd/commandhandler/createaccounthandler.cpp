#include "createaccounthandler.h"
#include "command/createaccount.h"
#include "../office.hpp"
#include "helper/hash.h"
#include "command/errorcodes.h"

CreateAccountHandler::CreateAccountHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}

void CreateAccountHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    m_command = init<CreateAccount>(std::move(command));
}

void CreateAccountHandler::onExecute() {
    assert(m_command);

    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;

    uint32_t newUser = 0;
    if (m_command->getDestBankId() == m_offi.svid) {
        newUser = m_offi.add_user(m_command->getBankId(), m_command->getPublicKey(), m_command->getTime());
        if(!newUser) {
            errorCode = ErrorCodes::Code::eCreateAccountFail;
            DLOG("ERROR: failed to create account\n");
        } else {
            m_command->setNewUser(newUser, m_command->getPublicKey());
        }
    }

    uint32_t msid, mpos;
    if (!errorCode) {
        const auto res = commitChanges(*m_command);
        errorCode = res.errorCode;
        msid = res.msid;
        mpos = res.mpos;

        if(!errorCode) {
            log_t tlog;
            tlog.time   = time(NULL);
            tlog.type   = m_command->getType();
            tlog.node   = m_command->getDestBankId();
            tlog.user   = newUser;
            tlog.umid   = m_command->getUserMessageId();
            tlog.nmid   = msid;
            tlog.mpos   = mpos;
            tlog.weight = -m_command->getDeduct();

            tInfo info;
            info.weight = m_usera.weight;
            info.deduct = m_command->getDeduct();
            info.fee = m_command->getFee();
            info.stat = m_usera.stat;
            memcpy(info.pkey, m_usera.pkey, sizeof(info.pkey));
            memcpy(tlog.info, &info, sizeof(tInfo));

            m_offi.put_ulog(m_command->getUserId(), tlog);

            if (m_command->getBankId() == m_command->getDestBankId()) {
                tlog.type|=0x8000; //incoming
                tlog.node = m_command->getBankId();
                tlog.user = m_command->getUserId();
                tlog.weight = m_command->getDeduct();
                m_offi.put_ulog(newUser, tlog);
            }
        }
    }

    try {
        std::vector<boost::asio::const_buffer> response;
        response.emplace_back(boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));
        if(!errorCode) {
            response.emplace_back(boost::asio::buffer(&m_usera, sizeof(m_usera)));
            response.emplace_back(boost::asio::buffer(&msid, sizeof(msid)));
            response.emplace_back(boost::asio::buffer(&mpos, sizeof(mpos)));
            response.emplace_back(boost::asio::buffer(&newUser, sizeof(newUser)));
        }
        boost::asio::write(m_socket, response);
    } catch (std::exception& e) {
        DLOG("Responding to client %08X error: %s\n", m_command->getUserId(), e.what());
    }
}

void CreateAccountHandler::onValidate() {
    if(m_command->getDestBankId() != m_offi.svid) {
        uint32_t now = time(NULL);
        if(now%BLOCKSEC>BLOCKSEC/2) {
            DLOG("ERROR: bad timing for remote account request, try after %d seconds\n",
                 BLOCKSEC-now%BLOCKSEC);
            throw ErrorCodes::Code::eCreateAccountBadTiming;
        }
    }
}

