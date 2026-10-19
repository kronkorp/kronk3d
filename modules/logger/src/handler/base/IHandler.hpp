#pragma once
#include <string>
#include "../../LoggerLevel.hpp"

namespace k3::logging
{

class ILoggerHandler
{
public:
    virtual ~ILoggerHandler() = default;

    virtual void log(LoggerLevel level, const std::string& message) = 0;
};

}
