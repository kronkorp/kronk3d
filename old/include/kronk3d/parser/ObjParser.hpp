#pragma once

#include "kronk3d/utils/FileParser.hpp"
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace k3
{

    class ObjParser;

    class IObjParserHandler
    {
        public:
            virtual ~IObjParserHandler() = default;

            virtual void handle(ObjParser& parser) = 0;
    };

    class ObjParser : public FileParser
    {
        public:
            ObjParser(std::string_view filename);

        private:
            std::unordered_map<std::string, std::unique_ptr<IObjParserHandler>> m_commands;
    };

}
