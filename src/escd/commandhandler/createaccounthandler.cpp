#include "createaccounthandler.h"
#include "command/createaccount.h"
#include "../office.hpp"
#include "helper/hash.h"
#include "command/errorcodes.h"

CreateAccountHandler::CreateAccountHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}

void CreateAccountHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    try {
        m_command = std::unique_ptr<CreateAccount>(dynamic_cast<CreateAccount*>(command.release()));
    } catch (std::bad_cast& bc) {
        DLOG("CreateAccount bad_cast caught: %s", bc.what());
        return;
    }
}

void CreateAccountHandler::onExecute() {
    assert(m_command);

    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;
    auto        startedTime     = time(NULL);
    uint32_t    lpath           = startedTime-startedTime%BLOCKSEC;
    int64_t     fee{0};
    int64_t     deduct{0};
    uint32_t    msid;
    uint32_t    mpos;

    uint32_t newUser = 0;
    if (m_command->getDestBankId() == m_offi.svid) {
        newUser = m_offi.add_user(m_command->getBankId(), m_usera.pkey, m_command->getTime(), m_command->getUserId());
        if(!newUser) {
            errorCode = ErrorCodes::Code::eCreateAccountFail;
            DLOG("ERROR: failed to create account\n");
        } else {
            m_command->setNewUser(newUser, m_usera.pkey);
        }
    }

    if (!errorCode) {
        deduct = m_command->getDeduct();
        fee = m_command->getFee();

        //commit changes
        m_usera.msid++;
        m_usera.time=m_command->getTime();
        m_usera.lpath=lpath;
        //m_usera.node = 0;


        Helper::create256signhash(m_command->getSignature(), m_command->getSignatureSize(), m_usera.hash, m_usera.hash);

        if(!m_offi.add_msg(*m_command.get(), msid, mpos)) {
            DLOG("ERROR: message submission failed (%08X:%08X)\n",msid, mpos);
            errorCode = ErrorCodes::Code::eMessageSubmitFail;
        } else {

            m_offi.set_user(m_command->getUserId(), m_usera, deduct+fee);

            log_t tlog;
            tlog.time   = time(NULL);
            tlog.type   = m_command->getType();
            tlog.node   = m_command->getBankId();
            tlog.user   = newUser;
            tlog.umid   = m_command->getUserMessageId();
            tlog.nmid   = msid;
            tlog.mpos   = mpos;
            tlog.weight = -deduct;

            tInfo info;
            info.weight = m_usera.weight;
            info.deduct = deduct;
            info.fee = fee;
            info.stat = m_usera.stat;
            memcpy(info.pkey, m_usera.pkey, sizeof(info.pkey));
            memcpy(tlog.info, &info, sizeof(tInfo));

            m_offi.put_ulog(m_command->getUserId(), tlog);

            if (m_command->getBankId() == m_command->getDestBankId()) {
                tlog.type|=0x8000; //incoming
                tlog.node = m_command->getBankId();
                tlog.user = m_command->getUserId();
                tlog.weight = deduct;
                m_offi.put_ulog(newUser, tlog);
            }
        }
    }

    m_usera.user = newUser;

    try {
        boost::asio::write(m_socket, boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));
        if(!errorCode) {
            commandresponse response{m_usera, msid, mpos};
            boost::asio::write(m_socket, boost::asio::buffer(&response, sizeof(response)));
        }
    } catch (std::exception& e) {
        DLOG("Responding to client %08X error: %s\n", m_usera.user, e.what());
    }
}

ErrorCodes::Code CreateAccountHandler::onValidate() {
    if(m_command->getDestBankId() != m_offi.svid) {
        uint32_t now = time(NULL);
        if(now%BLOCKSEC>BLOCKSEC/2) {
            DLOG("ERROR: bad timing for remote account request, try after %d seconds\n",
                 BLOCKSEC-now%BLOCKSEC);
            return ErrorCodes::Code::eCreateAccountBadTiming;
        }
    }
    return ErrorCodes::Code::eNone;
}

