#ifndef SERVICEHANDLER_H
#define SERVICEHANDLER_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"

class office;

/*!
 * \brief Command service. It execute proper action for commnads.
 */
class CommandService {
  public:
    /** \brief Constructor.
      * \param office  Office object.
      * \param socket  Socket connected with client. */
    CommandService(office& office, boost::asio::ip::tcp::socket& socket);

    /** \brief execute method which invokes command handler. */
    void onExecute(std::unique_ptr<IBlockCommand> command);

  private:
    boost::asio::ip::tcp::socket& m_socket;     ///< reference to scoket, required for sending errors
    office& m_offi;                             ///< reference to office object.
};


#endif // SERVICEHANDLER_H
