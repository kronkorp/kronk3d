#pragma once

#include <exception>
#include <string>
#include <format>
#include <stacktrace>

#pragma once

namespace k3
{

    class BaseException : public std::exception
    {
        protected:
            template<class... Args>
            BaseException(char const* const  format, Args&&... args) noexcept : m_message(std::vformat(format, std::make_format_args(args...))), m_trace(std::stacktrace::current()) {}

            template<class... Args>
            BaseException(const std::string &format, Args&&... args) noexcept : m_message(std::vformat(format, std::make_format_args(args...))), m_trace(std::stacktrace::current()) {}

        public:
            virtual const char *what(void) const noexcept override
            {
                return this->m_message.c_str();
            }

            const std::stacktrace &trace(void) const noexcept
            {
                return this->m_trace;
            }
        private:
            std::string m_message;
            std::stacktrace m_trace;
    };

}
