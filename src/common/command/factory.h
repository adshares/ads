#ifndef FACTORY_H
#define FACTORY_H

#include <memory>
#include "abstraction/interfaces.h"
#include "settings.hpp"

namespace command{

class factory
{
public:
    static std::unique_ptr<IBlockCommand> makeCommand(uint8_t type);

private:
    factory() = default;
};

}

#endif // FACTORY_H
