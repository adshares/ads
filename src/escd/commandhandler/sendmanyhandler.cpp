#include "sendmanyhandler.h"
#include "command/sendone.h"
#include "../office.hpp"
#include "helper/hash.h"

SendManyHandler::SendManyHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}


void SendManyHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    try {
        m_command = std::unique_ptr<SendMany>(dynamic_cast<SendMany*>(command.release()));
    } catch (std::bad_cast& bc) {
        DLOG("SendMany bad_cast caught: %s", bc.what());
        return;
    }
    m_command->initTransactionVector();
}

void SendManyHandler::onExecute() {
    assert(m_command);

    auto        startedTime     = time(NULL);
    uint32_t    lpath           = startedTime-startedTime%BLOCKSEC;
    int64_t     fee{0};
    int64_t     deduct{0};
    uint32_t    msid;
    uint32_t    mpos;
    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;

    std::vector<SendAmountTxnRecord> txns = m_command->getTransactionsVector();
    for (auto& it : txns) {
        uint16_t node = it.dest_node;
        uint32_t user = it.dest_user;
        if (!m_offi.check_user(node, user)) {
            DLOG("ERROR: bad target user %04X:%08X\n", node, user);
            errorCode = ErrorCodes::Code::eUserBadTarget;
            break;
        }
    }

    if (!errorCode) {
        deduct = m_command->getDeduct();
        fee = m_command->getFee();
    
        //commit changes
        m_usera.msid++;
        m_usera.time=m_command->getTime();
        m_usera.lpath=lpath;
    
        Helper::create256signhash(m_command->getSignature(), m_command->getSignatureSize(), m_usera.hash, m_usera.hash);
    
    
        if(!m_offi.add_msg(*m_command.get(), msid, mpos)) {
            DLOG("ERROR: message submission failed (%08X:%08X)\n",msid, mpos);
            errorCode = ErrorCodes::Code::eMessageSubmitFail;
        } else {
            m_offi.set_user(m_command->getUserId(), m_usera, deduct+fee);
        }
    }
    
    if (!errorCode) {
        log_t tlog;
        tlog.time   = time(NULL);
        tlog.type   = m_command->getType();
        tlog.node   = m_command->getBankId();
        tlog.user   = m_command->getUserId();
        tlog.umid   = m_command->getUserMessageId();
        tlog.nmid   = msid;
        tlog.mpos   = mpos;
        tlog.weight = -deduct - fee;
        m_offi.put_ulog(m_command->getUserId(),  tlog);
    
        if (txns.size() > 0) {
            std::map<uint64_t, log_t> log;
            for (unsigned int i=0; i<txns.size(); ++i) {
                uint64_t key = ((uint64_t)m_command->getUserId()<<32);
                key|=i;
                tlog.node = txns[i].dest_node;
                tlog.user = txns[i].dest_user;
                tlog.weight = -txns[i].amount;
                tlog.info[31]=(i?0:1);
                log[key]=tlog;
            }
            m_offi.put_ulog(log);
    
            tlog.type|=0x8000; //incoming
            tlog.node=m_command->getBankId();
            tlog.user=m_command->getUserId();
            for (unsigned int i=0; i<txns.size(); ++i) {
                if (txns[i].dest_node == m_offi.svid) {
                    tlog.weight = txns[i].amount;
                    tlog.info[31]=(i?0:1);
                    m_offi.put_ulog(txns[i].dest_user, tlog);
                    if (txns[i].amount >= 0) {
                        m_offi.add_deposit(txns[i].dest_user, txns[i].amount);
                    }
                }
            }
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



bool SendManyHandler::onValidate() {
    int64_t deduct = m_command->getDeduct();
    int64_t fee = m_command->getFee();
    ErrorCodes::Code errorCode = ErrorCodes::Code::eNone;

    if(deduct+fee+(m_usera.user ? USER_MIN_MASS:BANK_MIN_UMASS) > m_usera.weight) {
        DLOG("ERROR: too low balance txs:%016lX+fee:%016lX+min:%016lX>now:%016lX\n",
             deduct, fee, (uint64_t)(m_usera.user ? USER_MIN_MASS:BANK_MIN_UMASS), m_usera.weight);
        errorCode = ErrorCodes::Code::eLowBalance;
    } else {
        // check for duplicate of the same target and amount below 0
        errorCode = m_command->checkForDuplicates();
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
