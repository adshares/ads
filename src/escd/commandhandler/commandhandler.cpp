#include "commandhandler.h"
#include "command/getaccount.h"
#include "command/setaccountkey.h"
#include "../office.hpp"


CommandHandler::CommandHandler(office& office, boost::asio::ip::tcp::socket& socket)
    : m_offi(office),
      m_socket(socket) {
}

void CommandHandler::execute(std::unique_ptr<IBlockCommand> command, const user_t& usera) {
    m_usera = usera;

    if(command->checkSignature(m_usera.hash, m_usera.pkey)) {
        onInit(std::move(command));

        if(onValidate()) {
            onExecute();
        }
    }
}
