#pragma once

#include "LoggerLevel.hpp"
#include "handler/base/IHandler.hpp"
#include <fstream>
#include <memory>
#include <ostream>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <iostream>
#include <vector>

class Logger {
public:
    Logger();
    Logger(std::shared_ptr<std::ostream> handler);
    ~Logger();

    bool enable(void) const;
    void enable(bool enabled);

    // Console only: writing files is up to the application, e.g.
    //     Logger::logger().registerHandler(std::make_shared<std::ofstream>("latest.log", std::ios::app));
    static Logger &logger(void)
    {
        // Function-local static: initialized exactly once, even with several threads logging.
        static const std::shared_ptr<Logger> logger = [] {
            auto instance = std::make_shared<Logger>();
            instance->registerHandler(std::make_shared<std::ostream>(std::cout.rdbuf()));
            instance->setLevel(LoggerLevel::DEBUG);
            return instance;
        }();

        return *logger;
    }

    void debug(std::string_view format);
    ////////////////////////////////////////////////////////////////////////////
    /**
     * @brief         Print a "debug" level message on the current stream
     *
     * @param format  The format of the string to write
     * @param args    The arguments to give to the "format" string
     * @return        This method returns nothing
     */
    ////////////////////////////////////////////////////////////////////////////
    template<typename ...Args>
    void debug(std::string_view format, Args&&... args)
    {
        this->log(LoggerLevel::DEBUG, format, std::forward<Args>(args)...);
    }
    ////////////////////////////////////////////////////////////////////////////


    void info(std::string_view format);
    ////////////////////////////////////////////////////////////////////////////
    /**
     * @brief         Print a "info" level message on the current stream
     *
     * @param format  The format of the string to write
     * @param args    The arguments to give to the "format" string
     * @return        This method returns nothing
     */
    ////////////////////////////////////////////////////////////////////////////
    template<typename ...Args>
    void info(std::string_view format, Args&&... args)
    {
        this->log(LoggerLevel::INFO, format, std::forward<Args>(args)...);
    }
    ////////////////////////////////////////////////////////////////////////////


    void ok(std::string_view format);
    ////////////////////////////////////////////////////////////////////////////
    /**
     * @brief         Print a "ok" level message on the current stream
     *
     * @param format  The format of the string to write
     * @param args    The arguments to give to the "format" string
     * @return        This method returns nothing
     */
    ////////////////////////////////////////////////////////////////////////////
    template<typename ...Args>
    void ok(std::string_view format, Args&&... args)
    {
        this->log(LoggerLevel::SUCCESS, format, std::forward<Args>(args)...);
    }
    ////////////////////////////////////////////////////////////////////////////


    void warn(std::string_view format);
    ////////////////////////////////////////////////////////////////////////////
    /**
     * @brief         Print a "warning" level message on the current stream
     *
     * @param format  The format of the string to write
     * @param args    The arguments to give to the "format" string
     * @return        This method returns nothing
     */
    ////////////////////////////////////////////////////////////////////////////
    template<typename ...Args>
    void warn(std::string_view format, Args&&... args)
    {
        this->log(LoggerLevel::WARN, format, std::forward<Args>(args)...);
    }
    ////////////////////////////////////////////////////////////////////////////


    void error(std::string_view format);
    ////////////////////////////////////////////////////////////////////////////
    /**
     * @brief         Print a "error" level message on the current stream
     *
     * @param format  The format of the string to write
     * @param args    The arguments to give to the "format" string
     * @return        This method returns nothing
     */
    ////////////////////////////////////////////////////////////////////////////
    template<typename ...Args>
    void error(std::string_view format, Args&&... args)
    {
        this->log(LoggerLevel::ERROR, format, std::forward<Args>(args)...);
    }
    ////////////////////////////////////////////////////////////////////////////


    ////////////////////////////////////////////////////////////////////////////
    /**
     * @brief         Log a message on the currents streams
     *
     * @param level   The level of logging (warning, ok, info...)
     * @param format  The format of the string to write
     * @param args    The arguments to give to the "format" string
     * @return        This method returns nothing
     */
    ////////////////////////////////////////////////////////////////////////////
    template<typename ...Args>
    void log(LoggerLevel level, std::string_view format, Args&&... args)
    {
        if (level < this->m_currentLevel || !this->enable()) return;
        const std::string formatted = std::vformat(format, std::make_format_args(args...));

        for (auto& handler : this->m_handlers) {
            handler->log(level, formatted);
        }
    }
    ////////////////////////////////////////////////////////////////////////////


    ////////////////////////////////////////////////////////////////////////////
    /**
     * @brief           Register an handler to log in
     *
     * @param handler  The handler to register
     * @return         This method returns nothing
     */
    ////////////////////////////////////////////////////////////////////////////
    void registerHandler(std::shared_ptr<std::ostream> handler);
    ////////////////////////////////////////////////////////////////////////////


    ////////////////////////////////////////////////////////////////////////////
    /**
     * @brief           Register an handler to log in
     *
     * @param handler  The handler to register
     * @return         This method returns nothing
     */
    ////////////////////////////////////////////////////////////////////////////
    void setLevel(LoggerLevel level);
    ////////////////////////////////////////////////////////////////////////////

private:
    std::vector<std::unique_ptr<ILoggerHandler>> m_handlers;                          //!< The handlers (the streams)
    LoggerLevel                                  m_currentLevel = LoggerLevel::INFO;  //!< The current level of the logger (Debuf, info, ...). All lower level will be ignored
    bool                                         m_enable = true;
};
