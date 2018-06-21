#include "sendmanyhandler.h"
#include "command/sendone.h"
#include "../office.hpp"
#include "helper/hash.h"

SendManyHandler::SendManyHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : CommandHandler(office, socket) {
}

void SendManyHandler::onInit(std::unique_ptr<IBlockCommand> command) {
    m_command = init<SendMany>(std::move(command));
    assert(m_command);
    m_command->initTransactionVector();
}

void SendManyHandler::onExecute() {
    auto errorCode = ErrorCodes::Code::eNone;

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

    uint32_t msid, mpos;
    if (!errorCode) {
        auto res = commitChanges(*m_command);
        errorCode = res.errorCode;
        msid = res.msid;
        mpos = res.mpos;

        if (!errorCode) {
            log_t tlog;
            tlog.time   = time(NULL);
            tlog.type   = m_command->getType();
            tlog.node   = m_command->getBankId();
            tlog.user   = m_command->getUserId();
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

ErrorCodes::Code SendManyHandler::onValidate() {
    return m_command->checkForDuplicates();
}

