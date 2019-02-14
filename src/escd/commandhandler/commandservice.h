#ifndef SERVICEHANDLER_H
#define SERVICEHANDLER_H

#include <memory>
#include <boost/asio.hpp>
#include "abstraction/interfaces.h"

/*!
 * \brief Command service. It execute proper action for commnads.
 */
class CommandService {
  public:
    /** \brief Constructor.
      * \param office  Office object.
      * \param socket  Socket connected with client. */
    CommandService(office& office, client& client);

    /** \brief execute method which invokes command handler. */
    void onExecute(std::unique_ptr<IBlockCommand> command);

  private:
    client& m_client;     ///< reference to scoket, required for sending errors
    office& m_offi;                             ///< reference to office object.
};


#endif // SERVICEHANDLER_H
