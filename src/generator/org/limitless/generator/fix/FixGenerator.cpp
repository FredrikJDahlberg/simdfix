//
// Created by Fredrik Dahlberg on 2026-06-06.
//

#include <filesystem>
#include <print>

#include "org/limitless/generator/fix/Processor.hpp"
#include "org/limitless/generator/fix/Generator.hpp"

using namespace org::limitless::generator::fix;

int main(int argc, char** argv)
{
    using namespace org::limitless::fix::decoder;
    using namespace org::limitless::generator::fix;

    pugi::xml_document doc;
    const auto result = doc.load_file(argv[1]);
    if (!result)
    {
        std::println("XML error: {}, dir = {}, file = {}", result.description(),
                     std::filesystem::current_path().c_str(), argv[1]);
        return 1;
    }

    Processor processor{};
    processor.process(doc);

    Generator generator{};
    std::string grammarFile{argv[2]};
    grammarFile.append("/Grammar.hpp");
    generator.generateGrammar(grammarFile, processor.m_grammar);

    std::string decodersFile{argv[2]};
    decodersFile.append("/MessageDecoders.hpp"); // FIXME: MessageDecoders
    generator.generateDecoders(decodersFile, processor.m_records, processor.m_enums);

    std::string handlerFile{argv[2]};
    handlerFile.append("/MessageHandler.hpp");
    generator.generateMessageHandler(handlerFile, processor.m_records);
    return 0;
}
