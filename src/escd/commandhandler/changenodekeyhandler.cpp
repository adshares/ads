#include "changenodekeyhandler.h"
#include "../office.hpp"
#include "helper/hash.h"

ChangeNodeKeyHandler::ChangeNodeKeyHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}

void ChangeNodeKeyHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    try {
        m_command = std::unique_ptr<ChangeNodeKey>(dynamic_cast<ChangeNodeKey*>(command.release()));
    } catch (std::bad_cast& bc) {
        DLOG("ChangeNodeKey bad_cast caught: %s", bc.what());
        return;
    }
}

void ChangeNodeKeyHandler::onExecute() {
    assert(m_command);

    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;
    auto        startedTime    = time(NULL);
    uint32_t    lpath          = startedTime-startedTime%BLOCKSEC;

    auto deduct = m_command->getDeduct();
    auto fee    = m_command->getFee();

    if (m_command->getDestBankId()) {
        uint8_t* key = m_offi.node_pkey(m_command->getDestBankId());
        if (!key) {
            errorCode = ErrorCodes::Code::eSetKeyRemoteBankFail;
        } else {
            m_command->setOldPublicKey(key);
        }        

    } else {
        m_command->setOldPublicKey(m_offi.pkey);
        char *key = (char*)m_command->getKey();
        std::copy(key, key + 32, m_offi.pkey);        
    }

    uint32_t msid, mpos;

    if (!errorCode) {
        //commit changes
        m_usera.msid++;
        m_usera.time=m_command->getTime();
        m_usera.lpath=lpath;
        m_usera.node = m_command->getBankId();
        m_usera.user = m_command->getUserId();

        Helper::create256signhash(m_command->getSignature(), m_command->getSignatureSize(), m_usera.hash, m_usera.hash);

        if(!m_offi.add_msg(m_command->getBlockMessage(), m_command->getBlockMessageSize(), msid, mpos)) {
            DLOG("ERROR: message submission failed (%08X:%08X)\n",msid, mpos);
            errorCode = ErrorCodes::Code::eMessageSubmitFail;
        } else {
            m_offi.set_user(m_command->getUserId(), m_usera, deduct+fee);

            log_t tlog;
            tlog.time   = time(NULL);
            tlog.type   = m_command->getType();
            tlog.node   = 0;
//            tlog.user   = m_command->getKey();
            tlog.umid   = m_command->getUserMessageId();
            tlog.nmid   = msid;
            tlog.mpos   = mpos;
            tlog.weight = -deduct;

            tInfo info;
            info.weight = m_usera.weight;
            info.deduct = m_command->getDeduct();
            info.fee = m_command->getFee();
            info.stat = m_usera.stat;
            memcpy(info.pkey, m_usera.pkey, sizeof(info.pkey));
            memcpy(tlog.info, &info, sizeof(tInfo));

            m_offi.put_ulog(m_command->getUserId(), tlog);
        }
    }

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



bool ChangeNodeKeyHandler::onValidate()
{
    ErrorCodes::Code errorCode  = ErrorCodes::Code::eNone;
    int64_t         deduct      = m_command->getDeduct();
    int64_t         fee         = m_command->getFee();

    hash_t secretKey;
    if (!m_command->getDestBankId() && !m_offi.find_key(m_command->getKey(), secretKey)) {
        errorCode = ErrorCodes::Code::eMatchSecretKeyNotFound;
    }
    else if(m_command->getBankId()!=m_offi.svid) {
        errorCode = ErrorCodes::Code::eBankNotFound;
    }
    else if(!m_offi.svid) {
        errorCode = ErrorCodes::Code::eBankIncorrect;
    }
    else if(m_offi.readonly) { //FIXME, notify user.cpp about errors !!!
        errorCode = ErrorCodes::Code::eReadOnlyMode;
    }
    else if (m_command->getUserId()) {
        DLOG("ERROR: bad user %04X for bank key changes\n", m_command->getUserId());
        errorCode = ErrorCodes::Code::eBadUser;
    }
    else if(deduct+fee+(m_usera.user ? USER_MIN_MASS:BANK_MIN_UMASS) > m_usera.weight) {
        DLOG("ERROR: too low balance txs:%016lX+fee:%016lX+min:%016lX>now:%016lX\n",
             deduct, fee, (uint64_t)(m_usera.user ? USER_MIN_MASS:BANK_MIN_UMASS), m_usera.weight);
        errorCode = ErrorCodes::Code::eLowBalance;
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
