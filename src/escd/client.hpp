#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "command/factory.h"
#include "commandhandler/commandservice.h"

//this could all go to the office class and we could use just the start() function

/**
 * \brief Main class for esc client.
 *         Brief description continued.
 */

class client : public boost::enable_shared_from_this<client> {
  public:
    client(boost::asio::io_service& io_service,office& offi)
        : m_socket(io_service),
          m_offi(offi),
          m_addr(""),
          m_port(""),
          m_buf(nullptr),
          m_commandService(m_offi, m_socket) {
#ifdef DEBUG
        DLOG("OFFICER ready %04X\n",m_offi.svid);
#endif
    }

    ~client() {
#ifdef DEBUG
        DLOG("Client left %s:%s\n",m_addr.c_str(),m_port.c_str());
#endif
        free(m_buf);
        m_buf=nullptr;
    }

    boost::asio::ip::tcp::socket& socket() {
        return m_socket;
    }

    void start() { //TODO consider providing a local user file pointer
        m_addr  = m_socket.remote_endpoint().address().to_string();
        m_port  = std::to_string(m_socket.remote_endpoint().port());
#ifdef DEBUG
        DLOG("Client entered %s:%s\n",m_addr.c_str(),m_port.c_str());
#endif

        Helper::setSocketTimeout(m_socket);
        m_buf=(char*)std::malloc(txslen[TXSTYPE_MAX]+64+128);
        boost::asio::async_read(m_socket,boost::asio::buffer(m_buf,1),
                                boost::bind(&client::handle_read_txstype, shared_from_this(), boost::asio::placeholders::error));
    }

    void handle_read_txstype(const boost::system::error_code& error) {
        if(error) {
            DLOG("ERROR: read txstype error\n");
            m_offi.leave(shared_from_this());
            return;
        }

        if(*m_buf >= TXSTYPE_MAX) {
            DLOG("ERROR: read txstype failed\n");
            m_offi.leave(shared_from_this());
            return;
        }

        m_command = command::factory::makeCommand(*m_buf);

        if(m_command) {
            boost::asio::async_read(m_socket,boost::asio::buffer(m_command->getData()+1, m_command->getDataSize()-1),
                                    boost::bind(&client::handle_read_extended_txs, shared_from_this(), boost::asio::placeholders::error));
        }
        else {
            DLOG("ERROR: Could not create command");
        }

    }

    //if command doesn't have additional data then 0 bytes will be read.
    void handle_read_extended_txs(const boost::system::error_code& error) {
        if (error) {
            DLOG("ERROR reading txs data: %s\n", error.message().c_str());
            m_offi.leave(shared_from_this());
        }
        boost::asio::async_read(m_socket,boost::asio::buffer(m_command->getAdditionalData(), m_command->getAdditionalDataSize()),
                                boost::bind(&client::handle_read_signature_txs, shared_from_this(), boost::asio::placeholders::error));
    }

    void handle_read_signature_txs(const boost::system::error_code& error) {
        if (error) {
            DLOG("ERROR reading txs extended data: %s\n", error.message().c_str());
            m_offi.leave(shared_from_this());
        }
        boost::asio::async_read(m_socket,boost::asio::buffer(m_command->getSignature(), m_command->getSignatureSize()),
                                boost::bind(&client::handle_read_txs_complete, shared_from_this(), boost::asio::placeholders::error));
    }

    void handle_read_txs_complete(const boost::system::error_code& error)
    {
        if(error) {
            DLOG("ERROR reading signature txs: %s\n", error.message().c_str());                        
            m_offi.leave(shared_from_this());
        }

        if(m_command)
        {
            auto lockUserId = m_command->getUserId();

            try{
                if(m_offi.lock_user(lockUserId))
                {
                    try
                    {
                        m_commandService.onExecute(std::move(m_command));
                    }
                    catch (std::exception& e)
                    {
                        ELOG("ERROR exception in onExecute %s\n", e.what());
                    }
                    m_offi.unlock_user(lockUserId);
                }
                else{
                    ErrorCodes::Code code = ErrorCodes::Code::eLockUserFailed;
                    ELOG("ERROR: %s\n", ErrorCodes().getErrorMsg(code));
                    ELOG("ERROR: %d\n", lockUserId);
                    boost::asio::write(m_socket, boost::asio::buffer(&code, ERROR_CODE_LENGTH));
                }
            }
            catch (std::exception& e)
            {
                ELOG("ERROR exception in handle_read_txs_complete %s\n", e.what());
            }
            //@TODO: lock , unlock in RAII object.
        }

        m_offi.leave(shared_from_this());        
    }

private:
    boost::asio::ip::tcp::socket      m_socket;
    office&                           m_offi;
    std::string                       m_addr;
    std::string                       m_port;
    char*                             m_buf;
    CommandService                    m_commandService;
    std::unique_ptr<IBlockCommand>    m_command;
};

#endif // CLIENT_HPP
