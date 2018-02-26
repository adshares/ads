#include "responsehandler.h"
#include "command/pods.h"
#include "helper/json.h"
#include "helper/ascii.h"
#include "helper/hash.h"

using namespace Helper;

ResponseHandler::ResponseHandler(settings& sts)
    :m_sts(sts)
{
}

void ResponseHandler::onExecute(std::unique_ptr<IBlockCommand> command)
{            
    initLogs(command);

    switch(command->getType())
    {
        case TXSTYPE_INF:
            onGetAccoutResponse(std::move(command));
            break;
        case TXSTYPE_KEY:
            onSetAccountKeyResponse(std::move(command));
            break;
        default:
            break;
    }
}

void ResponseHandler::initLogs(std::unique_ptr<IBlockCommand>& txs)
{
    uint32_t now = time(NULL);
    now -= now%BLOCKSEC;
    m_pt.put("current_block_time", now);
    m_pt.put("previous_block_time", now - BLOCKSEC);

    std::stringstream tx_data;
    Helper::ed25519_key2text(tx_data, txs->getData(), txs->getDataSize() + txs->getSignatureSize());
    m_pt.put("tx.data",tx_data.str());
    m_logpt.put("tx.data",tx_data.str());

    if(txs->getType() != TXSTYPE_INF)
    {
        m_pt.put("tx.account_msid", m_sts.msid);
        m_logpt.put("tx.account_msid",m_sts.msid);

        std::stringstream tx_user_hashin;
        Helper::ed25519_key2text(tx_user_hashin, m_sts.ha.data(), SHA256_DIGEST_LENGTH);

        m_pt.put("tx.account_hashin", tx_user_hashin.str());
        m_logpt.put("tx.account_hashin", tx_user_hashin.str());

        std::array<uint8_t, SHA256_DIGEST_LENGTH> hashout;
        Helper::create256signhash(txs->getSignature(), txs->getSignatureSize(), m_sts.ha, hashout);

        std::stringstream tx_user_hashout;
        Helper::ed25519_key2text(tx_user_hashout, hashout.data(), SHA256_DIGEST_LENGTH);
        m_pt.put("tx.account_hashout",  std::move(tx_user_hashout.str()));
        //FIXME calculate deduction and fee
        m_pt.put("tx.deduct", print_amount(txs->getDeduct()));
        m_pt.put("tx.fee", print_amount(txs->getFee()));

        if(m_sts.msid == 1)
        {
            std::stringstream tx_user_public_key;
            Helper::ed25519_key2text(tx_user_public_key, m_sts.pk, SHA256_DIGEST_LENGTH);
            m_logpt.put("tx.account_public_key", std::move(tx_user_public_key.str()));
        }
    }
}

void ResponseHandler::onGetAccoutResponse(std::unique_ptr<IBlockCommand> command)
{
    accountresponse t;

    memcpy(&t, command->getResponse(), command->getResponseSize());

    command->saveResponse(m_sts);

    print_user(t.usera, m_pt, true, command->getBankId(), command->getUserId(), m_sts);
    print_user(t.globalusera, m_pt, false, command->getBankId(), command->getUserId(), m_sts);

    boost::property_tree::write_json(std::cout, m_pt, m_sts.nice);
}

void ResponseHandler::onSetAccountKeyResponse(std::unique_ptr<IBlockCommand> command)
{
    command->saveResponse(m_sts);
    std::cerr<<"PKEY changed2\n";
}
