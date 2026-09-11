#pragma once

#include "kronk3d/utils/BaseException.hpp"
#include <string_view>

namespace k3
{

    class ImageException : public BaseException
    {
        protected:
            template<class... Args>
            ImageException(char const* const  format, Args&&... args) noexcept : BaseException(format, args...) {}

            template<class... Args>
            ImageException(const std::string &format, Args&&... args) noexcept : BaseException(format, args...) {}
    };

    class FailedToOpenFile : public ImageException
    {
        public:
            FailedToOpenFile(std::string_view filename) noexcept : ImageException("Failed to open file {}", filename) {}
    };

}
