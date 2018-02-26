#ifndef FACTORY_H
#define FACTORY_H

#include <memory>
#include "abstraction/interfaces.h"
#include "settings.hpp"

namespace command{

/*!
 * \brief Class which is creating commad by using type.
 */

class factory
{
public:
    /**
     * \brief Make command function.
     * \param type Type of command
     * @TODO type should be enum type.
     */
    static std::unique_ptr<IBlockCommand> makeCommand(uint8_t type);

private:
    factory() = default;
};

}

#endif // FACTORY_H
