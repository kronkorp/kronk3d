#include "Logger.hpp"
#include "LoggerLevel.hpp"
#include "handler/impl/FileHandler.hpp"
#include "handler/impl/TtyHandler.hpp"
#include <iostream>
#include <memory>
#include <ostream>
#include <vector>
#include <cstdio>
#include <ctime>

#ifdef _WIN32
    #include <io.h>
#else
    #include <unistd.h>
#endif

static bool isTerminal(std::FILE* stream)
{
#ifdef _WIN32
    return _isatty(_fileno(stream)) != 0;
#else
    return isatty(fileno(stream)) != 0;
#endif
}

Logger::Logger() {}

Logger::~Logger() {}

Logger::Logger(std::shared_ptr<std::ostream> handler)
{
    this->registerHandler(handler);
}

void Logger::registerHandler(std::shared_ptr<std::ostream> handler)
{
    if ((handler->rdbuf() == std::cout.rdbuf() && isTerminal(stdout)) ||
        (handler->rdbuf() == std::cerr.rdbuf() && isTerminal(stderr))) {
            this->m_handlers.push_back(std::make_unique<TtyLoggerHandler>(handler));
    } else {
            this->m_handlers.push_back(std::make_unique<FileLoggerHandler>(handler));
    }
}

void Logger::debug(std::string_view format)
{
    this->log(LoggerLevel::DEBUG, format);
}

void Logger::info(std::string_view format)
{
    this->log(LoggerLevel::INFO, format);
}

void Logger::ok(std::string_view format)
{
    this->log(LoggerLevel::SUCCESS, format);
}

void Logger::warn(std::string_view format)
{
    this->log(LoggerLevel::WARN, format);
}

void Logger::error(std::string_view format)
{
    this->log(LoggerLevel::ERROR, format);
}

void Logger::setLevel(LoggerLevel level)
{
    this->m_currentLevel = level;
}

bool Logger::enable(void) const
{
    return this->m_enable;
}

void Logger::enable(bool enabled)
{
    this->m_enable = enabled;
}