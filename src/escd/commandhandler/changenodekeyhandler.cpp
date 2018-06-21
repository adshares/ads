#include "changenodekeyhandler.h"
#include "../office.hpp"
#include "helper/hash.h"

ChangeNodeKeyHandler::ChangeNodeKeyHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}

void ChangeNodeKeyHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    m_command = init<ChangeNodeKey>(std::move(command));
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
        std::vector<boost::asio::const_buffer> response;
        response.emplace_back(boost::asio::buffer(&errorCode, ERROR_CODE_LENGTH));

        if(!errorCode) {
            commandresponse cresponse{m_usera, msid, mpos};
            response.emplace_back(boost::asio::buffer(&cresponse, sizeof(cresponse)));
        }
        boost::asio::write(m_socket, response);

    } catch (std::exception& e) {
        DLOG("Responding to client %08X error: %s\n", m_usera.user, e.what());
    }
}

ErrorCodes::Code ChangeNodeKeyHandler::onValidate() {
    hash_t secretKey;
    if (!m_command->getDestBankId() && !m_offi.find_key(m_command->getKey(), secretKey)) {
        return ErrorCodes::Code::eMatchSecretKeyNotFound;
    }

    if (m_command->getUserId()) {
        DLOG("ERROR: bad user %04X for bank key changes\n", m_command->getUserId());
        return ErrorCodes::Code::eBadUser;
    }

    return ErrorCodes::Code::eNone;
}

