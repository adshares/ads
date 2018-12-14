#ifndef GETBLOCKSHANDLER_H
#define GETBLOCKSHANDLER_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"
#include "commandhandler.h"
#include "command/getblocks.h"
#include "helper/vipkeys.h"
#include "helper/signatures.h"

class office;
namespace Helper {
class Block;
}

class GetBlocksHandler : public CommandHandler {
  public:
    GetBlocksHandler(office&, client&);

    virtual void onInit(std::unique_ptr<IBlockCommand>) override;
    virtual void onExecute() override;
    virtual void onValidate() override;

  private:
    bool prepareFirstVipKeys(uint8_t* vipHash);
    void prepareNewVipKeys(uint8_t* vipHash);
    void loadLastBlockSignatures(uint32_t path);
    int vipSize(uint8_t* viphash);
    uint32_t readStart();
    void readBlockHeaders(
        uint32_t from,
        uint32_t to,
        header_t& header,
        Helper::Block& currentHeader);
    void sendFirstVipKeysIfNeeded(std::vector<boost::asio::const_buffer>&);
    void sendNewVipKeys(std::vector<boost::asio::const_buffer>&);
    void sendBlockHeaders(std::vector<boost::asio::const_buffer>&);
    void sendLastBlockSignatures(std::vector<boost::asio::const_buffer>&);
    VipKeys m_firstVipKeys;
    VipKeys m_newVipKeys;
    Signatures m_signatures;
    std::vector<header_t> m_serversHeaders;
    bool m_newviphash=false;
    ErrorCodes::Code prepareResponse();
    std::unique_ptr<GetBlocks> m_command;
};

#endif
