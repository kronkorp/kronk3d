#pragma once

#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace k3
{


    using RegistryKey = size_t;

    template<class T>
    class Registry
    {
        public:
            Registry() {};

            T& operator[](RegistryKey key)
            {
                return *m_values[key];
            }

            T& get(RegistryKey key)
            {
                return *m_values[key];
            }

            bool has(RegistryKey key)
            {
                return m_values.size() > key;
            }

            RegistryKey put(std::unique_ptr<T> value)
            {
                size_t key = m_values.size();
                m_values.push_back(std::move(value));
                return key;
            }

            T& putFetch(std::unique_ptr<T> value)
            {
                return get(put(std::move(value)));
            }

            RegistryKey indexOf(const T& value)
            {
                for (RegistryKey i = 0; i < m_values.size(); ++i) {
                    if (&value == m_values[i].get()) {
                        return i;
                    }
                }
                return -1UL;
            }

        private:
            std::vector<std::unique_ptr<T>> m_values;
    };

}
