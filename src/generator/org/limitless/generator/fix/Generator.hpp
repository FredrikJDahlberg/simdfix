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

    static std::string uncap(const std::string& value)
    {
        std::string result{value};
        if (!value.empty())
        {
            result[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(result[0])));
        }
        return result;
    }

    void generateGrammar(const std::string& grammarFile, const std::vector<Record>& grammar) const
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
        for (const auto& record : grammar)
        {
            auto sorted = record.m_fields;
            std::ranges::sort(sorted, {}, &Field::m_tag);
            out << std::format("struct {} {{\n", record.m_name);
            out << "    static constexpr uint16_t Tags[] = {\n";
            for (auto& field : sorted)
            {
                out << std::format("        {},\n", field.m_tag);
            }
            out << "    };\n\n";
            out << "    static constexpr Dictionary Grammar[] = {\n";
            for (auto& field : sorted)
            {
                out << std::format("        {{ {}, {}, Presence::{} }},\n",
                                   field.m_tag, field.m_length, field.m_presence.name());
            }
            out << "    };\n";
            out << "};\n\n";
        }
        out << "}\n\n";
        out << "#endif //SIMD_FIX_GRAMMAR_HPP\n";
        out.close();
    }

    static void generateDefinition(std::ostream& out, const Record& record)
    {
        out << std::format("struct {}Decoder : ", record.m_name);
        switch (record.m_parent.m_value)
        {
            case Parent::Message:
                out << std::format("MessageDecoder<protocols::{}>\n", record.m_name);
                break;
            case Parent::Component:
                out << "StructDecoder\n";
                break;
            case Parent::Group:
                out << "GroupDecoder\n";
                break;
            default:
                out << "Decoder\n";
                break;
        }
        out << "{\n";
        if (record.m_parent == Parent::Message)
        {
            out << "    using Decoder = MessageDecoder;\n\n";
        }
        out << std::format("    {}Decoder() = default;\n\n", record.m_name);
    }

    static void generateWrap(std::ostream& out, const Record& record)
    {

        out << std::format("    {}Decoder& wrap(", record.m_name);

        if (record.m_parent == Parent::Message)
        {
            out << "const std::span<const uint8_t> data,\n";
            out << "                        const std::span<Token> tokens,\n";
            out << "                        const std::span<uint16_t> tags,\n";
            out << "                        const uint32_t count)\n";
            out << "    {\n";
            out << "        Decoder::wrap(data, tokens, tags, count);\n";
        }
        else
        {
            out << "Tokens* decoder";
            switch (record.m_parent.m_value)
            {
                case Parent::Group:
                    out << ", uint32_t tag)\n";
                    out << "    {\n";
                    out << "        GroupDecoder::wrap(decoder, tag);\n";
                    break;
                case Parent::Component:
                    out << ")\n";
                    out << "    {\n";
                    out << "        StructDecoder::wrap(decoder);\n";
                    break;
                default:
                    out << ")\n";
                    out << "    {\n";
                    break;
            }
        }

        auto arg = record.m_parent == Parent::Message ? "&m_tokens" : "decoder";
        for (auto& field : record.m_records)
        {
            out << std::format("        m_{}.wrap({}{});\n",
                               uncap(field.m_name), arg,
                               field.m_tag != 0 ? std::format(", {}", field.m_tag) : std::string{});
        }
        out << "        return *this;\n";
        out << "    }\n\n";
    }

    static void generateGetters(std::ostream& out, const Record& record)
    {
        auto arg = record.m_parent != Parent::Message ? "this->m_decoder->" : "m_tokens.";
        for (auto& field : record.m_fields)
        {
            auto methodName = field.m_name;
            methodName[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(methodName[0])));
            std::string_view mandatory = field.m_presence.m_value == decoder::Presence::Required ? "true" : "false";
            if (field.m_category == Category::String)
            {
                out << std::format("    [[nodiscard]] std::expected<std::span<const uint8_t>, Result::Values> {}() const\n", methodName);
                out << "    {\n";
                out << std::format("        return {}getString<{}, {}>();\n", arg, field.m_tag, mandatory);
                out << "    }\n\n";
            }
            else if (field.m_category == Category::Int32)
            {
                out << std::format("    [[nodiscard]] std::expected<uint32_t, Result::Values> {}() const\n", methodName);
                out << "    {\n";
                out << std::format("        return {}getUint32<{}, {}>();\n", arg, field.m_tag, mandatory);
                out << "    }\n\n";
            }
        }
    }

    static void generateFields(std::ostream& out, const Record& record)
    {
        if (record.m_records.size() >= 1)
        {
            out << "private:\n";
            for (auto& comp : record.m_records)
            {
                out << std::format("    {}Decoder m_{}{{}};\n\n", comp.m_type, uncap(comp.m_name));
            }
        }
    }

    static void generateStructGetters(std::ostream& out, const Record& record)
    {
        for (const auto& comp : record.m_records)
        {
            auto fieldName = uncap(comp.m_name);
            out << std::format("    {}Decoder& {}()\n    {{\n", comp.m_type, fieldName);
            out << std::format("        return m_{};\n", fieldName);
            out << std::format("    }}\n\n");
        }
    }

    static void generateRecord(std::ostream& out, const Record& record)
    {
        generateDefinition(out, record);
        generateFields(out, record);
        if (record.m_records.size() >= 1)
        {
            out << "public:\n";
        }
        if (record.m_parent == Parent::Message)
        {
            out << std::format("    static constexpr uint16_t MessageId = '{}';\n\n", record.m_id);
        }
        generateWrap(out, record);
        generateStructGetters(out, record);
        generateGetters(out, record);
        out << "};\n\n";
    }

    void generateMessageDecoders(const std::string& fileName, const std::vector<Record>& records) const
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
        for (auto& record : records)
        {
            generateRecord(out, record);
        }
        out << "} // namespace org::limitless::fix::generated\n\n";
        out << "#endif //SIMD_FIX_MESSAGE_DECODERS_HPP\n";

        out.close();
    }

    void generateMessageHandler(const std::string& fileName, const std::vector<Record>& records) const
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
        out << "                          const uint32_t count)\n";
        out << "    {\n";
        out << "        const auto messageType = data[tokens[2].position];\n";
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
};

}
#endif // SIMD_FIX_GENERATOR
