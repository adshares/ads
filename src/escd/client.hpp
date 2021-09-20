#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <chrono>
#include "helper/socket.h"
#include "command/factory.h"
#include "commandhandler/commandservice.h"
#include "../common/helper/blocks.h"

#define NETSRV_SOCK_TIMEOUT 5
#define NETSRV_SOCK_IDLE    5
#define NETSRV_SOCK_MAXTRY  3

#if BOOST_VERSION >= 107000
#define BUFFER_SIZE(s) ((s).size())
#else
#define BUFFER_SIZE(s) (boost::asio::detail::buffer_size_helper(s))
#endif

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
          m_timeout(io_service),
          m_commandService(m_offi, *this) {
#ifdef DEBUG
        DLOG("OFFICER ready %04X\n",m_offi.svid);
#endif
    }

    ~client() {
        m_timeout.cancel();
#ifdef DEBUG
        DLOG("Client left %s:%s\n",m_addr.c_str(),m_port.c_str());
#endif
    }

    boost::asio::ip::tcp::socket& socket() {
        return m_socket;
    }

    void set_timeout()
    {
       m_timeout.expires_from_now(boost::posix_time::seconds(NETSRV_SOCK_TIMEOUT));
       m_timeout.async_wait([&](boost::system::error_code const &ec) {
           if (ec == boost::asio::error::operation_aborted)
               return;

           m_offi.leave(shared_from_this());
           DLOG("CLIENT: timeout %s:%s\n",m_addr.c_str(),m_port.c_str());
           m_socket.cancel();
       });
    }

    void start() { //TODO consider providing a local user file pointer

#ifdef DEBUG
        m_addr  = m_socket.remote_endpoint().address().to_string();
        m_port  = std::to_string(m_socket.remote_endpoint().port());
        DLOG("Client entered %s:%s\n",m_addr.c_str(),m_port.c_str());
#endif

        Helper::setSocketTimeout(m_socket, NETSRV_SOCK_TIMEOUT, NETSRV_SOCK_IDLE, NETSRV_SOCK_MAXTRY);

        set_timeout();
        boost::asio::async_read(m_socket,boost::asio::buffer(&m_version,sizeof(m_version)),
                                boost::bind(&client::handle_read_version, shared_from_this(), boost::asio::placeholders::error));
    }

    void handle_read_version(const boost::system::error_code& error) {
        if(error) {
            DLOG("ERROR: read version error\n");
            m_offi.leave(shared_from_this());
            return;
        }

        int32_t version = CLIENT_PROTOCOL_VERSION;
        uint8_t version_error;
#ifdef DEBUG
        version = -version;
        version_error = m_version >= 0;
#else
        version_error = m_version <= 0;
#endif
        m_version = abs(m_version);
        // logic deciding which versions are compatible should be put here in the future
        version_error = version_error || (m_version < CLIENT_MIN_ACCEPTED_VERSION || m_version > CLIENT_PROTOCOL_VERSION);

        boost::asio::write(m_socket, boost::asio::buffer(&version, sizeof(version)));
        boost::asio::write(m_socket, boost::asio::buffer(&version_error, sizeof(version_error)));
        if(version_error) {
            DLOG("ERROR: incorrect client version %d\n", (version < 0 ? -1 : 1)*m_version);
            m_offi.leave(shared_from_this());
            return;
        }

        DLOG("CLIENT: version %d\n", (version < 0 ? -1 : 1)*m_version);

        if(m_version == 2) {
            boost::asio::async_read(m_socket,boost::asio::buffer(&m_size,sizeof(m_size)),
                                                    boost::bind(&client::handle_read_size, shared_from_this(), boost::asio::placeholders::error));
        } else {
            m_readsize +=1;
            boost::asio::async_read(m_socket,boost::asio::buffer(&m_type,1),
                                        boost::bind(&client::handle_read_txstype, shared_from_this(), boost::asio::placeholders::error));
        }
    }

    void handle_read_size(const boost::system::error_code& error) {
        if(error) {
            m_offi.leave(shared_from_this());
            return;
        }
        m_readsize =1;
        boost::asio::async_read(m_socket,boost::asio::buffer(&m_type,1),
                                        boost::bind(&client::handle_read_txstype, shared_from_this(), boost::asio::placeholders::error));

        // redirect
    }

    void handle_read_txstype(const boost::system::error_code& error) {
        if(error) {
//            DLOG("ERROR: read txstype error\n");
            return m_offi.leave(shared_from_this());
            return;
        }

        if(m_type >= TXSTYPE_MAX) {
            DLOG("ERROR: read txstype failed\n");
            m_offi.leave(shared_from_this());
            return;
        }

        m_command = command::factory::makeCommand(m_type);
        m_command->setClientVersion(m_version);

        if(m_command && m_command->getDataSize() > 1 && m_command->getData() != nullptr) {
            int data_size = m_command->getDataSize()-1;
            m_readsize += data_size;
            boost::asio::async_read(m_socket,boost::asio::buffer(m_command->getData()+1, data_size),
                                    boost::bind(&client::handle_read_extended_txs, shared_from_this(), boost::asio::placeholders::error));
        }
        else {
            DLOG("ERROR: Could not create command");
            m_offi.leave(shared_from_this());
            return;
        }

    }

    //if command doesn't have additional data then 0 bytes will be read.
    void handle_read_extended_txs(const boost::system::error_code& error) {
        if (error) {
            DLOG("ERROR reading txs data: %s\n", error.message().c_str());
            return m_offi.leave(shared_from_this());
        }


        m_readsize += m_command->getAdditionalDataSize();
        boost::asio::async_read(m_socket,boost::asio::buffer(m_command->getAdditionalData(), m_command->getAdditionalDataSize()),
                                boost::bind(&client::handle_read_signature_txs, shared_from_this(), boost::asio::placeholders::error));
    }

    void handle_read_signature_txs(const boost::system::error_code& error) {
        if (error) {
            DLOG("ERROR reading txs extended data: %s\n", error.message().c_str());
            return m_offi.leave(shared_from_this());
        }

        m_readsize += m_command->getSignatureSize();



            boost::asio::async_read(m_socket,boost::asio::buffer(m_command->getSignature(), m_command->getSignatureSize()),
                    boost::bind(m_size > m_readsize ? &client::handle_read_txs_extra : &client::handle_read_txs_complete, shared_from_this(), boost::asio::placeholders::error));

    }

    void handle_read_txs_extra(const boost::system::error_code& error)
    {
        if(error) {
            DLOG("ERROR reading signature txs: %s\n", error.message().c_str());
            return m_offi.leave(shared_from_this());
        }
        m_extra_data_size = m_size - m_readsize;
        if(m_extra_data_size > sizeof(m_extra_data)) {
            DLOG("ERROR txs extra data too long\n");
            return m_offi.leave(shared_from_this());
        }
        boost::asio::async_read(m_socket,boost::asio::buffer(m_extra_data, m_extra_data_size),
                 boost::bind( &client::handle_read_txs_complete, shared_from_this(), boost::asio::placeholders::error));
    }

    void handle_read_txs_complete(const boost::system::error_code& error)
    {
        if(error) {
            DLOG("ERROR reading signature txs: %s\n", error.message().c_str());
            return m_offi.leave(shared_from_this());
        }

        if(m_command)
        {
            if(m_extra_data_size > 0) {
                // extra data ignored
            }
            if(m_command->getCommandType() == CommandType::eReadingOnly && m_offi.redirect_read.size() > 0) {
                if(std::find(m_offi.redirect_read_exclude.begin(), m_offi.redirect_read_exclude.end(), m_socket.remote_endpoint().address()) == m_offi.redirect_read_exclude.end()) {
                    TLOG("CLIENT: redirect %s\n", m_socket.remote_endpoint().address().to_string().c_str());
                    return this->sendRedirect(CommandType::eReadingOnly);
                }
            }

            if(m_command->getCommandType() == CommandType::eModifying && m_offi.redirect_write.size() > 0) {
                if(std::find(m_offi.redirect_write_exclude.begin(), m_offi.redirect_write_exclude.end(), m_socket.remote_endpoint().address()) == m_offi.redirect_write_exclude.end()) {
                    TLOG("CLIENT: redirect %s\n", m_socket.remote_endpoint().address().to_string().c_str());
                    return this->sendRedirect(CommandType::eModifying);
                }
            }

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
                    catch (...)
                    {
                        ELOG("UNKNOWN ERROR exception in onExecute \n");
                    }

                    m_offi.unlock_user(lockUserId);
                }
                else{
                    ErrorCodes::Code code = ErrorCodes::Code::eLockUserFailed;
                    ELOG("ERROR: %s\n", ErrorCodes().getErrorMsg(code));
                    ELOG("ERROR: USER: %d\n", lockUserId);
                    boost::asio::write(m_socket, boost::asio::buffer(&code, ERROR_CODE_LENGTH));
                }
            }
            catch (std::exception& e)
            {
                ELOG("ERROR exception in handle_read_txs_complete %s\n", e.what());
            }
            //@TODO: lock , unlock in RAII object.
        }

        set_timeout();
        if(m_version == 2) {
            boost::asio::async_read(m_socket,boost::asio::buffer(&m_size,sizeof(m_size)),
                                                    boost::bind(&client::handle_read_size, shared_from_this(), boost::asio::placeholders::error));
        } else {
            boost::asio::async_read(m_socket,boost::asio::buffer(&m_type,1),
                                        boost::bind(&client::handle_read_txstype, shared_from_this(), boost::asio::placeholders::error));
        }
    }

    void sendError(const ErrorCodes::Code error, boost::asio::const_buffer error_info) {
        try {
            if(m_version == 2) {
                uint32_t size = ERROR_CODE_LENGTH + BUFFER_SIZE(error_info);
                boost::asio::write(m_socket, boost::asio::buffer(&size, sizeof(size)));
            }
            boost::asio::write(m_socket, boost::asio::buffer(&error, ERROR_CODE_LENGTH));
            if(m_version == 2) {
                boost::asio::write(m_socket, boost::asio::buffer(error_info));
            }
        }
        catch(std::exception& e) {
            DLOG("Responding to client %08X error: %s\n", m_addr + ":" + m_port, e.what());
        }
    }

    void sendError(const ErrorCodes::Code error, std::vector<boost::asio::const_buffer> error_info, int info_size) {
        try {
            if(m_version == 2) {
                uint32_t size = ERROR_CODE_LENGTH + info_size;
                boost::asio::write(m_socket, boost::asio::buffer(&size, sizeof(size)));
            }
            boost::asio::write(m_socket, boost::asio::buffer(&error, ERROR_CODE_LENGTH));
            if(m_version == 2) {
                boost::asio::write(m_socket, error_info);
            }
        }
        catch(std::exception& e) {
            DLOG("Responding to client %08X error: %s\n", m_addr + ":" + m_port, e.what());
        }
    }

    void sendError(const ErrorCodes::Code error, std::string error_info) {
         sendError(error, boost::asio::buffer(&error_info[0], error_info.length()));
     }

    void sendError(const ErrorCodes::Code error) {
        sendError(error, boost::asio::buffer((void *)nullptr, 0));
    }

    void sendRedirect(CommandType cmd_type) {
        std::vector<boost::asio::ip::tcp::endpoint> targets;
        if(cmd_type == CommandType::eModifying) {
            targets = m_offi.redirect_write;
        } else if(cmd_type == CommandType::eReadingOnly) {
            targets = m_offi.redirect_read;
        }
        if(targets.size() == 0) {
            sendError(ErrorCodes::eUnknownError);
            m_offi.leave(shared_from_this());
            return;
        }
        std::vector<boost::asio::const_buffer> response;

        struct timeval tp;
        gettimeofday(&tp, NULL);
        long int ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;

        auto endpoint = targets[ ms % targets.size()];
        boost::asio::ip::address_v4 ip = endpoint.address().to_v4();
        auto port = endpoint.port();
        response.emplace_back(boost::asio::buffer(&ip, sizeof(ip)));
        response.emplace_back(boost::asio::buffer(&port, sizeof(port)));
        sendError(ErrorCodes::eRedirect, response, sizeof(ip)+sizeof(port));
        m_offi.leave(shared_from_this());
    }

    void sendResponse(std::vector<boost::asio::const_buffer> data) {
        if(m_version == 2) {
            uint32_t size = 0;//
            for(auto& i : data) {
                size += boost::asio::buffer_size(i);
            }
            boost::asio::write(m_socket, boost::asio::buffer(&size, sizeof(size)));
        }
        boost::asio::write(m_socket, data);
    }

private:
    boost::asio::ip::tcp::socket      m_socket;
    office&                           m_offi;
    std::string                       m_addr;
    std::string                       m_port;

    char                              m_type{TXSTYPE_NON};
    uint32_t                          m_size{0};
    uint32_t                          m_readsize{0};
    int32_t                           m_version{0};
    boost::asio::deadline_timer       m_timeout;
    CommandService                    m_commandService;
    std::unique_ptr<IBlockCommand>    m_command;
    char                              m_extra_data[MAX_EXTRADATA_LENGTH];
    uint32_t                          m_extra_data_size{0};
};

#endif // CLIENT_HPP
