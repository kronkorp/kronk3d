#pragma once

#include "kronk3d/utils/BaseException.hpp"
#include <cstddef>
#include <fstream>
#include <istream>
#include <sstream>
#include <string>
#include <string_view>

namespace k3
{

    class FileParserException : public BaseException
    {
        protected:
            template<class... Args>
            FileParserException(char const* const  format, Args&&... args) noexcept : BaseException(format, args...) {}

            template<class... Args>
            FileParserException(const std::string &format, Args&&... args) noexcept : BaseException(format, args...) {}
    };

    class FileParserCantOpen : public FileParserException
    {
        public:
            FileParserCantOpen(std::string_view file) : FileParserException("Cannot open file \"{}\"", file) {}
    };

    class FileParserReachedEof : public FileParserException
    {
        public:
            FileParserReachedEof() : FileParserException("File parser has reached EOF") {}
    };

    class FileParserCantExtract : public FileParserException
    {
        public:
            FileParserCantExtract(std::string_view line) : FileParserException("Cannot extract from line \"{}\"", line) {}
    };

    class IFileParser
    {
        public:
            virtual ~IFileParser() = default;

            virtual const std::string& line()   const noexcept = 0;
            virtual size_t             lineNo() const noexcept = 0;

            virtual bool               next() = 0;
    };

    class FileParser : public IFileParser
    {
        public:
            FileParser(const std::string& filename) : m_file(filename), m_filename(filename), m_iStream(m_file)
            {
                if (m_file.fail() || m_file.bad() || !m_file.is_open() ||
                    m_iStream.fail() || m_iStream.bad()) {
                    throw FileParserCantOpen(filename);
                }
            }

            template<typename T>
            T extract()
            {
                T temp;
                if (m_ss.eof()) {
                    throw FileParserReachedEof();
                }
                if (!(m_ss >> temp) || m_ss.fail()) {
                    throw FileParserCantExtract(m_line);
                }
                return temp;
            }

            virtual const std::string& line() const noexcept override
            {
                return m_line;
            }

            virtual size_t lineNo() const noexcept override
            {
                return m_lineNo;
            }

            virtual bool next() override
            {
                std::getline(this->m_iStream, this->m_line);
                if (this->m_iStream.eof()) {
                    return false;
                } else if (m_iStream.fail() || m_iStream.bad()) {
                    return false;
                }
                m_ss.clear();
                m_ss.str(m_line);
                ++m_lineNo;
                return true;
            }

        protected:
            std::ifstream     m_file;
            std::istream&     m_iStream;
            size_t            m_lineNo = 0;
            std::string       m_line;
            std::string       m_filename;
            std::stringstream m_ss;
    };

}
