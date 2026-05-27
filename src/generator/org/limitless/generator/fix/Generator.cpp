//
// Created by Fredrik Dahlberg on 2026-04-22.
//
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <print>
#include <ranges>
#include <unordered_map>
#include <utility>
#include <vector>

#include  "org/limitless/fix/decoder/Dictionary.hpp"

#include "pugixml.hpp"

namespace org::limitless::generator::fix {

namespace decoder = limitless::fix::decoder;

using decoder::Category;
using decoder::Parent;

struct Type
{
    std::string m_name;
    int32_t m_size;
    int32_t m_length;
    Category m_type;

    Type(std::string name, const int32_t size, const int32_t length, const Category type = Category::Null)
        : m_name(std::move(name)), m_size(size), m_length(length), m_type{type}
    {
    }
};

struct Field
{
    int32_t m_tag{};
    std::string m_name;
    std::string m_type;
    int32_t m_length{};
    Category m_category{decoder::Category::Null};
    Parent m_parentCategory{};
    decoder::Presence m_presence{decoder::Presence::Required};

    Field() = default;
    Field(const int32_t tag, std::string  name, std::string type,
          const int32_t length, const decoder::Presence presence, Category category, Parent parent) :
        m_tag(tag), m_name(std::move(name)), m_type(std::move(type)), m_length(length),
        m_category(category), m_parentCategory(parent), m_presence{presence}
    {
    }

    Field(const Field& other) = default;
    Field(Field&& other) noexcept = default;
    Field& operator=(const Field& other) = default;
    Field& operator=(Field&& other) noexcept = default;
};

struct Record
{
    std::string m_name{};
    std::string m_id{};
    Parent m_category{};
    std::vector<Field> m_fields{};
    std::vector<Field> m_structs{};

    Record() = default;

    Record(std::string name, std::string id, const Parent type, const std::vector<Field>& fields)
        : m_name(std::move(name)), m_id(std::move(id)), m_category(type), m_fields{fields}
    {
    }

    Record(const Record& other) = default;
    Record(Record&& other) noexcept = default;
    Record& operator=(const Record& other) = default;
    Record& operator=(Record&& other) noexcept = default;
};

struct Generator
{
    std::unordered_map<std::string, Type> m_types{};
    std::unordered_map<std::string, Record> m_recordsByName{};
    std::vector<Record> m_records{};
    std::vector<Record> m_grammar{};

    static std::string camelCase(const std::string& value)
    {
        std::string result{value};
        if (!value.empty())
        {
            result[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(result[0])));
        }
        return result;
    }

    void processTypes(const pugi::xml_object_range<pugi::xml_node_iterator>& types)
    {
        m_types.try_emplace("char",   "char",   1, 1, Category::String);
        m_types.try_emplace("uint8",  "uint8",  1, 1, Category::Int32);
        m_types.try_emplace("int32",  "int32",  4, 1, Category::Int32);
        m_types.try_emplace("uint32", "uint32", 4, 1, Category::Int32);

        for (auto typeNode : types)
        {
            const std::string_view name = typeNode.attribute("name").as_string();
            const std::string_view primitive = typeNode.attribute("primitiveType").as_string();
            const std::string_view type = typeNode.attribute("type").as_string();
            auto primitiveType = m_types.find(std::string{primitive});
            auto refType = m_types.find(std::string{type});

            if (!primitive.empty() && primitiveType == m_types.end())
            {
                std::println("Error: type '{}' has an invalid primitive type '{}'", name, primitive);
            }
            else if (!type.empty() && refType == m_types.end())
            {
                std::println("Error: type '{}' has an invalid derived '{}'", name, type);
            }
            else
            {
                auto* ref = refType != m_types.end() ? &refType->second : &primitiveType->second;
                auto length = std::max(typeNode.attribute("length").as_int(), ref->m_length);
                m_types.try_emplace(std::string{name}, std::string{name}, ref->m_size, length, ref->m_type);
            }
        }
    }

    void processFields(const pugi::xml_object_range<pugi::xml_node_iterator>& nodes,
                       const Parent parent, std::vector<Field>& fields)
    {
        for (const auto& field : nodes)
        {
            const std::string_view nodeType = field.name();
            const std::string_view name = field.attribute("name").as_string();
            const int32_t tag = field.attribute("tag").as_int();
            const std::string_view type = field.attribute("type").as_string();
            const std::string_view primitive = field.attribute("primitiveType").as_string();
            const auto resolvedType = std::string{type.empty() ? primitive : type};
            const std::string_view presenceAttr = field.attribute("presence").as_string();
            decoder::Presence presence{presenceAttr};
            auto refType = m_types.find(resolvedType);
            if (nodeType == "group")
            {
                // const auto counterName = field.attribute("counter").as_string();
                const auto groupName = field.attribute("name").as_string();
                fields.emplace_back(0, std::string{groupName}, std::string{groupName}, 0, presence,
                                    Category::Struct, parent);
                // fields.emplace_back(tag, std::string{counterName}, std::string{groupName}, 0, presence,
                //                    Category::Counter, parent);
            }
            else
            {
                if (refType == m_types.end())
                {
                    auto record = m_recordsByName.find(resolvedType);
                    if (record != m_recordsByName.end())
                    {
                        fields.emplace_back(0, std::string{name}, std::string{type}, 0, presence, Category::Struct, parent);
                    }
                    else
                    {
                        std::println("component not found {}", name);
                    }
                }
                else
                {
                    const auto& ref = refType->second;
                    const int32_t length = std::max(field.attribute("length").as_int(), ref.m_length);
                    fields.emplace_back(tag, std::string{name}, std::string{type}, length, presence, ref.m_type, parent);
                }
            }
        }
    }

    void processRecords(const pugi::xpath_node_set& components, const Parent parent)
    {
        for (const auto& xpathNode : components)
        {
            const auto node = xpathNode.node();
            std::string_view name = node.attribute("name").as_string();
            std::vector<Field> fields{};
            processFields(node.children(), parent, fields);

            Record record{std::string{name}, std::string{}, parent, fields};
            if (parent == Parent::Message)
            {
                record.m_id = node.attribute("id").as_string();
            }

            //std::vector<const Field*> structs{};
            for (const auto& field : fields)
            {
                if (field.m_category == Category::Struct)
                {
                    record.m_structs.push_back(field);
                }
            }

            m_recordsByName.emplace(std::string{name}, record);
            m_records.emplace_back(record);
            m_grammar.emplace_back(record);
        }
    }

    void process(const pugi::xml_document& doc)
    {
        const pugi::xml_node protocol = doc.child("protocol");
        processTypes(protocol.child("types").children());
        processRecords(protocol.select_nodes(".//group"), Parent::Group);
        processRecords(protocol.select_nodes(".//component"), Parent::Component);
        processRecords(protocol.select_nodes(".//message"), Parent::Message);
    }

    void generateGrammar(std::ostream& out) const
    {
        out << "#ifndef SIMD_FIX_GRAMMAR_HPP\n";
        out << "#define SIMD_FIX_GRAMMAR_HPP\n\n";
        out << "#include \"org/limitless/fix/decoder/Dictionary.hpp\"\n\n";
        out << "namespace org::limitless::fix::protocols {\n";

        for (const auto& record: m_grammar)
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
            out << "    static constexpr decoder::Dictionary Grammar[] = {\n";
            for (auto& field : sorted)
            {
                out << std::format("        {{ {}, {}, decoder::Presence::{} }},\n",
                                   field.m_tag, field.m_length, field.m_presence.name());
            }
            out << "    };\n";
            out << "};\n\n";
        }
        out << "}\n\n";
        out << "#endif //SIMD_FIX_GRAMMAR_HPP\n";
    }

    static void generateStruct(std::ostream& out, const Record& record)
    {
        auto& name = record.m_name;
        auto& id   = record.m_id;
        if (record.m_category != Parent::Message)
        {
            out << "template <typename Message>\n";
        }

        std::string decoder;
        if (record.m_category == Parent::Group)
        {
            decoder = "decoder::GroupDecoder<Message>";
            out << std::format("struct {}Decoder : {}\n{{\n", name, decoder);
        }
        else
        {
            decoder = std::format("decoder::MessageDecoder<protocols::{}>", name);
            out << std::format("struct {}Decoder : {}\n{{\n", name, decoder);
        }
        if (record.m_category == Parent::Message)
        {
            out << "    using Message = " << decoder << ";\n";
        }
        if (!record.m_structs.empty())
        {
            out << "private:\n";
            for (auto& ref : record.m_structs)
            {
                out << std::format("    {}Decoder<Message> m_{};\n\n", ref.m_type, camelCase(ref.m_type));
            }
        }
        out << "public:\n";
        if (record.m_category == Parent::Message)
        {
            out << std::format("    static constexpr uint16_t MessageId = '{}';\n\n", id);
            out << std::format("    {}Decoder() {{}}\n\n", name);
        }
        if (record.m_category != Parent::Group)
        {
            out << std::format("    {}Decoder& wrap(const std::span<const uint8_t> data,\n", name);
            out << "                        const std::span<Token> tokens,\n";
            out << "                        const std::span<uint16_t> tags,\n";
            out << "                        const uint32_t count)\n";
            out << "    {\n";
            out << "        MessageDecoder::wrap(data, tokens, tags, count);\n";
            out << "        return *this;\n";
            out << "    }\n\n";
        }
        else
        {
            out << std::format("    {}Decoder& wrap()\n    {{\n", record.m_name);
            out << std::format("        decoder::GroupDecoder<Message>::wrap(decoder::GroupDecoder<Message>::next(627));\n");
            out << "        return *this;\n";
            out << "    }\n\n";
        }
        for (const auto& ref : record.m_structs)
        {
            auto fieldName = ref.m_type;
            fieldName[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(fieldName[0])));
            out << std::format("    {}Decoder<Message> {}() {{\n", ref.m_type, fieldName);
            out << std::format("        return m_{};\n", fieldName);
            out << std::format("    }}\n\n");
        }

        for (auto& field : record.m_fields)
        {
            auto methodName = field.m_name;
            methodName[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(methodName[0])));
            std::string_view mandatory = field.m_presence.m_value == decoder::Presence::Required ? "true" : "false";
            if (field.m_category == Category::String)
            {
                out << std::format("    [[nodiscard]] std::expected<std::span<const uint8_t>, decoder::DecoderStatus> {}() const\n", methodName);
                out << "    {\n";
                out << std::format("        return this->getString<{}>({});\n", field.m_tag, mandatory);
                out << "    }\n\n";
            }
            else if (field.m_category != Category::Struct)
            {
                out << std::format("    [[nodiscard]] std::expected<uint32_t, decoder::DecoderStatus> {}() const\n", methodName);
                out << "    {\n";
                out << std::format("        return this->getUnsigned<{}>({});\n", field.m_tag, mandatory);
                out << "    }\n\n";
            }
        }
        out << "};\n\n";
    }

    void generateMessages(std::ostream& out) const
    {
        out << "#ifndef SIMD_FIX_MESSAGES_HPP\n";
        out << "#define SIMD_FIX_MESSAGES_HPP\n\n";
        out << "#include <expected>\n\n";
        out << "#include \"org/limitless/fix/decoder/DecoderStatus.hpp\"\n";
        out << "#include \"org/limitless/fix/decoder/GroupDecoder.hpp\"\n";
        out << "#include \"org/limitless/fix/decoder/MessageDecoder.hpp\"\n";
        out << "#include \"org/limitless/fix/messages/Grammar.hpp\"\n\n";
        out << "namespace org::limitless::fix::generated {\n\n";
        out << "using namespace org::limitless::fix;\n\n";

        for (auto& record : m_records)
        {
            generateStruct(out, record);
        }

        out << "} // namespace org::limitless::fix::generated\n\n";
        out << "#endif //SIMD_FIX_MESSAGES_HPP\n";
    }

    void generateMessageHandler(std::ostream& out)
    {
        out << "#ifndef SIMD_FIX_MESSAGE_HANDLER_HPP\n";
        out << "#define SIMD_FIX_MESSAGE_HANDLER_HPP\n\n";
        out << "#include \"org/limitless/fix/decoder/DecoderStatus.hpp\"\n";
        out << "#include \"org/limitless/fix/messages/Messages.hpp\"\n\n";
        out << "namespace org::limitless::fix::generated {\n\n";
        out << "using decoder::DecoderStatus;\n\n";
        out << "template <typename Handler>\n";
        out << "class MessageHandler\n{\n";

        std::vector<Record> messages{};
        for (const auto& record : m_records)
        {
            if (!record.m_id.empty())
            {
                messages.push_back(record);
            }
        }
        for (auto& message : messages)
        {
            auto memberName = message.m_name;
            memberName[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(memberName[0])));
            out << std::format("    {}Decoder m_{}{{}};\n", message.m_name, memberName);
        }

        out << "\npublic:\n";
        out << "    template <typename Event>\n";
        out << "    DecoderStatus receive(Event&& event)\n";
        out << "    {\n";
        out << "        return static_cast<Handler*>(this)->handle(std::forward<Event>(event));\n";
        out << "    }\n\n";
        out << "    DecoderStatus handle(const std::span<const uint8_t> data,\n";
        out << "                        const std::span<Token> tokens,\n";
        out << "                        const std::span<uint16_t> tags,\n";
        out << "                        const uint32_t count)\n";
        out << "    {\n";
        out << "        const auto messageType = data[tokens[2].position];\n";
        out << "        auto status = DecoderStatus::InvalidMessageType;\n";
        out << "        switch (messageType)\n";
        out << "        {\n";
        for (auto& message : messages)
        {
            if (!message.m_id.empty())
            {
                auto memberName = camelCase(message.m_name);
                out << std::format("            case {}Decoder::MessageId:\n", message.m_name);
                out << std::format("                m_{}.wrap(data, tokens, tags, count);\n", memberName);
                out << std::format("                status = m_{}.checkRequired();\n", memberName);
                out << "                if (status == DecoderStatus::Success)\n";
                out << "                {\n";
                out << std::format("                    status = receive(m_{});\n", memberName);
                out << "                }\n";
                out << "                break;\n";
            }
        }
        out << "            default:\n";
        out << "                break;\n";
        out << "        }\n";
        out << "        return status;\n";
        out << "    }\n\n";
        out << "protected:\n";
        for (auto& message : messages)
        {
            out << std::format("    DecoderStatus handle({}Decoder&) {{ return DecoderStatus::Success; }}\n", message.m_name);
        }
        out << "};\n\n";
        out << "} // namespace org::limitless::fix::generated\n\n";
        out << "#endif // SIMD_FIX_MESSAGE_HANDLER_HPP\n";
    }

    void generateDecoders(const char* directory)
    {
        std::string messageFile{directory};
        messageFile.append("/Messages.hpp");
        std::ofstream messagesOut(messageFile);
        if (messagesOut)
        {
            generateMessages(messagesOut);
        }
        else
        {
            std::println("Error: could not open '{}' for writing", directory);
        }
        messagesOut.close();
    }
};

}

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

    Generator generator{};
    generator.process(doc);

    std::string grammarFile{argv[2]};
    grammarFile.append("/Grammar.hpp");
    std::ofstream grammarOut(grammarFile);
    if (!grammarOut)
    {
        std::println("Error: could not open '{}' for writing", argv[2]);
        return 1;
    }
    generator.generateGrammar(grammarOut);
    grammarOut.close();

    generator.generateDecoders(argv[2]);

    std::string handlerFile{argv[2]};
    handlerFile.append("/MessageHandler.hpp");
    std::ofstream handlerOut(handlerFile);
    if (handlerOut)
    {
        generator.generateMessageHandler(handlerOut);
    }
    else
    {
        std::println("Error: could not open '{}' for writing", handlerFile);
    }
    return 0;
}
