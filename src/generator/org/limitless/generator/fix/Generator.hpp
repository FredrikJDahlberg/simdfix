//
// Created by Fredrik Dahlberg on 2026-04-22.
//
#ifndef SIMD_FIX_GENERATOR
#define SIMD_FIX_GENERATOR

#include <fstream>
#include <filesystem>
#include <iostream>
#include <print>

#include "org/limitless/generator/fix/DataModel.hpp"
#include "org/limitless/generator/fix/Processor.hpp"

namespace org::limitless::generator::fix {

namespace decoder = limitless::fix::decoder;

struct Generator
{
    static void generateGrammar(const std::string& grammarFile, const std::vector<Record>& messages)
    {
        std::ofstream out(grammarFile);
        if (!out)
        {
            std::println("Error: could not open '{}' for writing", grammarFile);
            return;
        }
        out << "#ifndef SIMD_FIX_GRAMMAR_HPP\n";
        out << "#define SIMD_FIX_GRAMMAR_HPP\n\n";
        out << "#include \"org/limitless/fix/decoder/Dictionary.hpp\"\n\n";
        out << "namespace org::limitless::fix::protocols {\n\n";
        out << "using namespace org::limitless::fix::decoder;\n\n";
        for (const auto& message : messages)
        {
            auto sorted = message.m_fields;
            std::ranges::sort(sorted, {}, &Field::m_tag);
            out << std::format("struct {} {{\n", message.m_name);
            out << "    static constexpr uint16_t Tags[] = {\n";
            for (auto& field : sorted)
            {
                out << std::format("        {},\n", field.m_tag);
            }
            out << "    };\n\n";
            out << "    static constexpr Dictionary Grammar[] = {\n";
            for (auto& field : sorted)
            {
                out << std::format("        {{ {}, {}, Presence::{}, Category::{} }},\n",
                                   field.m_tag, field.m_length, field.m_presence.name(), field.m_parent.name());
            }
            out << "    };\n";
            out << "};\n\n";
        }
        out << "}\n\n";
        out << "#endif //SIMD_FIX_GRAMMAR_HPP\n";
        out.close();
    }

    static void generateDecoders(const std::string& fileName,
                                 const std::vector<Record>& records,
                                 const std::vector<Record>& enums)
    {
        std::ofstream out(fileName);
        if (!out)
        {
            std::println("Error: could not open '{}' for writing", fileName);
            return;
        }

        out << "#ifndef SIMD_FIX_MESSAGE_DECODERS_HPP\n";
        out << "#define SIMD_FIX_MESSAGE_DECODERS_HPP\n\n";
        out << "#include <expected>\n\n";
        out << "#include \"org/limitless/fix/decoder/GroupDecoder.hpp\"\n";
        out << "#include \"org/limitless/fix/decoder/StructDecoder.hpp\"\n";
        out << "#include \"org/limitless/fix/decoder/MessageDecoder.hpp\"\n";
        out << "#include \"org/limitless/fix/messages/Grammar.hpp\"\n\n";
        out << "namespace org::limitless::fix::generated {\n\n";
        out << "using namespace org::limitless::fix::decoder;\n\n";
        out << "using String = std::span<const uint8_t>;  // FIXME\n\n";
        // FIXME: add constants
        for (const auto& value : enums)
        {
            generateEnum(out, value);
        }
        for (auto& record : records)
        {
            generateRecord(out, record);
        }
        out << "} // namespace org::limitless::fix::generated\n\n";
        out << "#endif //SIMD_FIX_MESSAGE_DECODERS_HPP\n";

        out.close();
    }

    static void generateMessageHandler(const std::string& fileName, const std::vector<Record>& records)
    {
        std::ofstream out(fileName);
        if (!out)
        {
            std::println("Error: could not open '{}' for writing", fileName);
            return;
        }

        std::vector<Record> messages{};
        for (const auto& record : records)
        {
            if (!record.m_id.empty())
            {
                messages.push_back(record);
            }
        }
        out << "#ifndef SIMD_FIX_MESSAGE_HANDLER_HPP\n";
        out << "#define SIMD_FIX_MESSAGE_HANDLER_HPP\n\n";
        out << "#include \"org/limitless/fix/decoder/Result.hpp\"\n";
        out << "#include \"org/limitless/fix/messages/MessageDecoders.hpp\"\n\n";
        out << "namespace org::limitless::fix::generated {\n\n";
        out << "using decoder::Result;\n\n";
        out << "template <typename Handler>\n";
        out << "class MessageHandler\n{\n";
        for (auto& message : messages)
        {
            out << std::format("    {}Decoder m_{};\n", message.m_name, uncap(message.m_name));
        }
        out << "\npublic:\n";
        out << "    template <typename Event>\n";
        out << "    Result::Values receive(Event&& event)\n";
        out << "    {\n";
        out << "        return static_cast<Handler*>(this)->handle(std::forward<Event>(event));\n";
        out << "    }\n\n";
        out << "    Result::Values handle(const std::span<const uint8_t> data,\n";
        out << "                          const std::span<Token> tokens,\n";
        out << "                          const std::span<uint16_t> tags,\n";
        out << "                          const uint32_t count,\n";
        out << "                          const uint8_t messageType)\n";
        out << "    {\n";
        out << "        auto status = Result::InvalidMessageType;\n";
        out << "        switch (messageType)\n";
        out << "        {\n";
        for (auto& message : messages)
        {
            auto memberName = uncap(message.m_name);
            out << std::format("            case {}Decoder::MessageId:\n", message.m_name);
            out << std::format("                m_{}.wrap(data, tokens, tags, count);\n", memberName);
            out << std::format("                status = m_{}.checkRequired();\n", memberName);
            out << "                if (status == Result::Success)\n";
            out << "                {\n";
            out << std::format("                    status = receive(m_{});\n", memberName);
            out << "                }\n";
            out << "                break;\n";
        }
        out << "            default:\n";
        out << "                break;\n";
        out << "        }\n";
        out << "        return status;\n";
        out << "    }\n\n";
        out << "protected:\n";
        for (auto& message : messages)
        {
            out << std::format("    Result::Values handle({}Decoder&) {{ return Result::Success; }}\n", message.m_name);
        }
        out << "};\n\n";
        out << "} // namespace org::limitless::fix::generated\n\n";
        out << "#endif // SIMD_FIX_MESSAGE_HANDLER_HPP\n";
    }

private:

    static std::string uncap(const std::string& value)
    {
        std::string result{value};
        if (!value.empty())
        {
            result[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(result[0])));
        }
        return result;
    }

    static void generateDefinition(std::ostream& out, const Record& record)
    {
        out << std::format("struct {}Decoder : ", record.m_name);
        switch (record.m_parent.m_value)
        {
            case ParentType::Message:
                out << std::format("MessageDecoder<protocols::{}>\n", record.m_name);
                out << "{\n";
                break;
            case ParentType::Component:
                out << "StructDecoder\n{\n";
                break;
            case ParentType::Group:
                out << "GroupDecoder\n{\n";
                break;
            default:
                break;
        }
    }

    static void generateConstructors(std::ostream& out, const Record& record)
    {
        if (record.m_records.size() >= 1)
        {
            out << "public:\n";
        }
        std::string arg{};
        switch (record.m_parent.m_value)
        {
            case ParentType::Component:
                arg = "StructDecoder";
                break;
            case ParentType::Group:
                arg = "GroupDecoder";
                break;
            default:
                break;
        }
        const auto isMessage = record.m_parent == ParentType::Message;
        if (isMessage)
        {
            out << std::format("    {}Decoder() : \n", record.m_name);
        }
        else
        {
            out << std::format("    explicit {}Decoder(FieldDecoder& decoder) : \n", record.m_name);
        }
        if (!arg.empty())
        {
            out << std::format("        {}{{decoder}}{}\n", arg, record.m_records.empty() ? "" : ",");
        }
        for (auto const& field : record.m_records)
        {
            out << std::format("        m_{}{{{}decoder}}{}\n",
                               uncap(field.m_name), isMessage ? "m_" : "",
                               field.m_name != record.m_records.back().m_name ? "," : "");
        }
        out << "    {\n    }\n\n";
    }

    static void generateFields(std::ostream& out, const Record& record)
    {
        if (record.m_records.size() >= 1)
        {
            out << "private:\n";
            for (auto& comp : record.m_records)
            {
                out << std::format("    {}Decoder m_{};\n\n", comp.m_type, uncap(comp.m_name));
            }
        }
    }

    static void generateWrap(std::ostream& out, const Record& record)
    {
        if (record.m_parent == ParentType::Message)
        {
            out << std::format("    {}Decoder& wrap(", record.m_name);
            out << "const std::span<const uint8_t> data,\n";
            out << "            const std::span<Token> tokens,\n";
            out << "            const std::span<uint16_t> tags,\n";
            out << "            const uint32_t count)\n";
            out << "    {\n";
            out << "        m_decoder.wrap(data, tokens, tags, count);\n";
            out << "        return *this;\n";
            out << "    }\n\n";
        }
        else if (record.m_parent == ParentType::Group)
        {
            out << std::format("    {}Decoder& wrap()\n", record.m_name);
            out << "    {\n";
            out << std::format("        GroupDecoder::wrap({});\n", record.m_tag);
            out << "        return *this;\n";
            out << "    }\n\n";
        }
    }

    static void generateGetters(std::ostream& out, const Record& record)
    {
        auto parent = record.m_parent.name();
        for (auto& field : record.m_fields)
        {
            auto methodName = field.m_name;
            methodName[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(methodName[0])));
            std::string_view mandatory = field.m_presence.m_value == Presence::Required ? "true" : "false";
            auto categoryName = field.m_category.name();
            auto categoryType = field.m_category.type();
            if (field.m_category != Category::Counter && field.m_category != Category::Struct)
            {
                auto isEnum = field.m_category == Category::Enum;
                out << std::format("    [[nodiscard]] std::expected<{}, Result::Values> {}() const\n",
                                   isEnum? field.m_type : categoryType, methodName);
                out << "    {\n";
                std::string arg = isEnum ? std::format(", {}", field.m_type) : "";
                out << std::format("        return m_decoder.get{}<{}, {}{}, ParentType::{}>();\n",
                                   categoryName, field.m_tag, mandatory, arg, parent);
                out << "    }\n\n";
            }
        }
        for (const auto& comp : record.m_records)
        {
            auto fieldName = uncap(comp.m_name);
            out << std::format("    {}Decoder& {}()\n    {{\n", comp.m_type, fieldName);
            out << std::format("        return m_{}", fieldName);
            if (comp.m_tag != 0)
            {
                out << std::format(".wrap()");
            }
            out << ";\n";
            out << std::format("    }}\n\n");
        }
    }

    static void generateRecord(std::ostream& out, const Record& record)
    {
        generateDefinition(out, record);
        generateFields(out, record);
        generateConstructors(out, record);
        if (record.m_parent == ParentType::Message)
        {
            out << std::format("    static constexpr uint16_t MessageId = '{}';\n\n", record.m_id);
        }
        generateWrap(out, record);
        generateGetters(out, record);
        out << "};\n\n";
    }

    static void generateEnum(std::ostream& out, const Record& record)
    {
        out << std::format("struct {}\n", record.m_name);
        out << "{\n";
        const auto end = record.m_fields.end();
        out << "    enum Values\n    {\n";
        for (auto value = record.m_fields.begin(); value != end; ++value)
        {
            out << std::format("        {}{}\n", value->m_name, value != end - 1? "," : "");
        }
        out << "    };\n";
        auto size = record.m_fields.size();
        out << std::format("    static constexpr uint8_t Codes[{}]  =\n    {{\n", size);
        for (auto value = record.m_fields.begin(); value != end; ++value)
        {
            out << std::format("        '{}'{}\n", value->m_type[0], value != end - 1? "," : "");
        }
        out <<"    };\n";
        out << std::format("    {}() : m_value{{Null}} {{}}\n", record.m_name);
        out << "    Values m_value;\n";
        out << "};\n\n";
    }
};

}
#endif // SIMD_FIX_GENERATOR
