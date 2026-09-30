/*
** KRONK CORP, 2026
** KRONK3D
** File description:
** Value-or-error return type
*/
#pragma once

#include <optional>
#include <string>
#include <utility>

namespace k3
{

    struct Error
    {
        std::string message;
    };

    // Same surface as std::expected<T, std::string> (bool test, *, ->, error()), which cannot be
    // used yet: Clang 18 with libstdc++ 13 (Ubuntu 24.04) does not provide it.
    template<typename T>
    class Result
    {
        public:
            Result(T value) : m_value(std::move(value)) {}
            Result(Error error) : m_error(std::move(error.message)) {}

            [[nodiscard]] bool has_value() const noexcept { return m_value.has_value(); }
            explicit operator bool() const noexcept { return has_value(); }

            [[nodiscard]] T& value() & { return *m_value; }
            [[nodiscard]] const T& value() const & { return *m_value; }
            [[nodiscard]] T&& value() && { return std::move(*m_value); }

            T& operator*() & { return *m_value; }
            const T& operator*() const & { return *m_value; }
            T&& operator*() && { return std::move(*m_value); }
            T* operator->() { return &*m_value; }
            const T* operator->() const { return &*m_value; }

            [[nodiscard]] const std::string& error() const noexcept { return m_error; }

        private:
            std::optional<T> m_value{};
            std::string      m_error{};
    };

}
