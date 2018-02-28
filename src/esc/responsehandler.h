#ifndef RESPONSEHANDLER_H
#define RESPONSEHANDLER_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"


class office;

class ResponseHandler {
  public:
    ResponseHandler(settings& sts);

    void onExecute(std::unique_ptr<IBlockCommand> command);

  private:
    void initLogs(std::unique_ptr<IBlockCommand>& txs);
    void onGetAccoutResponse(std::unique_ptr<IBlockCommand> command);
    void onSetAccountKeyResponse(std::unique_ptr<IBlockCommand> command);

  private:
    settings&                   m_sts;
    boost::property_tree::ptree m_pt;
    boost::property_tree::ptree m_logpt;

};


#endif // RESPONSEHANDLER_H
