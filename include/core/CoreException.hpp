#pragma once

#include "utils/BaseException.hpp"
namespace k3
{

    class CoreException : public BaseException
    {
        protected:
            template<class... Args>
            CoreException(char const* const  format, Args&&... args) noexcept : BaseException(format, args...) {}

            template<class... Args>
            CoreException(const std::string &format, Args&&... args) noexcept : BaseException(format, args...) {}
    };

    class FileNotOpen : public CoreException
    {
        public:
            FileNotOpen() noexcept : CoreException("File not open.") {}
    };

}
