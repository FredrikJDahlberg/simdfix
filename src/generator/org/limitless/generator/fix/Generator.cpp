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

namespace decoder = org::limitless::fix::decoder;
using decoder::TypeName;

struct Type
{
    std::string m_name;
    int32_t m_size;
    int32_t m_length;
    TypeName m_type;

    Type(std::string name, const int32_t size, const int32_t length, const TypeName type = TypeName::Null)
        : m_name(std::move(name)), m_size(size), m_length(length), m_type{type}
    {
    }
};

struct Field
{
    int32_t m_tag{};
    std::string m_name;
    int32_t m_length{};
    TypeName m_type{};
    decoder::Presence m_presence{decoder::Presence::Required};

    Field() = default;

    Field(const int32_t tag, std::string  name, TypeName type, const int32_t length, const decoder::Presence presence) :
        m_tag(tag), m_name(std::move(name)), m_length(length), m_type(type), m_presence{presence}
    {
    }

    Field(const Field& other) = default;
    Field(Field&& other) noexcept = default;
    Field& operator=(const Field& other) = default;
    Field& operator=(Field&& other) noexcept = default;
};

// message or component
struct Struct
{
    std::string m_name{};
    std::string m_id{};
    TypeName m_type{};
    std::vector<Field> m_fields{};

    Struct() = default;

    Struct(std::string name, std::string id, const TypeName type, const std::vector<Field>& fields)
        : m_name(std::move(name)), m_id(std::move(id)), m_type(type), m_fields{fields}
    {
    }

    Struct(const Struct& other) = default;
    Struct(Struct&& other) noexcept = default;
    Struct& operator=(const Struct& other) = default;
    Struct& operator=(Struct&& other) noexcept = default;
};

struct Generator
{
    std::unordered_map<std::string, Type> m_types{};
    std::unordered_map<std::string, Struct> m_records;
    std::unordered_map<std::string, Struct> m_messages;

    void process(const pugi::xml_document& doc)
    {
        const pugi::xml_node protocol = doc.child("protocol");
        processTypes(protocol.child("types").children());
        processComponents(protocol.select_nodes(".//component|.//group"));
        processMessages(protocol.children("message"));
    }

    void processTypes(const pugi::xml_object_range<pugi::xml_node_iterator>& types)
    {
        m_types.try_emplace("char",   "char",   1, 1, TypeName::String);
        m_types.try_emplace("uint8",  "uint8",  1, 1, TypeName::Int32);
        m_types.try_emplace("int32",  "int32",  4, 1, TypeName::Int32);
        m_types.try_emplace("uint32", "uint32", 4, 1, TypeName::Int32);

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

    void processComponents(const pugi::xpath_node_set& components)
    {
        for (const auto& xpathNode : components)
        {
            const auto node = xpathNode.node();
            std::string_view name = node.attribute("name").as_string();
            std::vector<Field> fields{};
            processFields(node.children(), fields);
            m_records.try_emplace(std::string{name}, std::string{name}, std::string{}, TypeName::Component, fields);
        }
    }

    void processMessages(const pugi::xml_object_range<pugi::xml_named_node_iterator>& messages)
    {
        for (auto& message : messages)
        {
            std::string_view name = message.attribute("name").as_string();
            std::string_view id = message.attribute("id").as_string();
            std::vector<Field> fields{};
            processFields(message.children(), fields);
            m_messages.try_emplace(std::string{name}, std::string{name}, std::string{id}, TypeName::Message, fields);
        }
    }

    void processFields(const pugi::xml_object_range<pugi::xml_node_iterator>& nodes,
                       std::vector<Field>& fields,
                       const bool inGroup = false)
    {
        for (const auto& field : nodes)
        {
            const std::string_view nodeType = field.name();
            const std::string_view name = field.attribute("name").as_string();
            const int32_t tag = field.attribute("tag").as_int();
            const std::string_view type = field.attribute("type").as_string();
            const std::string_view primitive = field.attribute("primitiveType").as_string();
            const std::string resolvedType = std::string{type.empty() ? primitive : type};
            const std::string_view presenceAttr = field.attribute("presence").as_string();
            decoder::Presence presence{presenceAttr};
            auto refType = m_types.find(resolvedType);
            if (nodeType == "group")
            {
                const auto counterName = field.attribute("counter").as_string();
                Field counter{tag, std::string{counterName}, TypeName::Group, 0, presence};
                fields.emplace_back(std::move(counter));
                // processFields(field.children(), fields, true);
            }
            else
            {
                if (refType == m_types.end())
                {
                    auto compType = m_records.find(resolvedType);
                    if (compType != m_records.end())
                    {
                        fields.append_range(compType->second.m_fields);
                    }
                    else
                    {
                        std::println("component not found {}", name);
                    }
                }
                else
                {
                    const int32_t length = std::max(field.attribute("length").as_int(), refType->second.m_length);
                    const TypeName typeName = inGroup ? TypeName::GroupMember : refType->second.m_type;
                    fields.emplace_back(tag, std::string{name}, typeName, length, presence);
                }
            }
        }
    }

    void print()
    {
        for (auto& val: m_types | std::views::values)
        {
            auto& type = val;
            std::println("Type{{name={}, align={}, length={}}}", type.m_name, type.m_size, type.m_length);
        }
        for (const auto& message : m_messages)
        {
            std::println("{}", message.second.m_name);
            for (auto& field : message.second.m_fields)
            {
                std::println("    {}, {}", field.m_tag, field.m_name);
            }
        }
    }

    void generateStruct(std::ostream& out, const Struct& component)
    {
        auto sorted = component.m_fields;
        std::ranges::sort(sorted, {}, &Field::m_tag);
        out << std::vformat("struct {} {{\n", std::make_format_args(component.m_name));
        out << "    static constexpr uint16_t Tags[] = {\n";
        for (auto& field : sorted)
        {
            out << std::vformat("        {},\n", std::make_format_args(field.m_tag));
        }
        out << "    };\n\n";
        out << "    static constexpr Dictionary Grammar[] = {\n";
        for (auto& field : sorted)
        {
            auto presence = field.m_presence.name();
            out << std::vformat("        {{ {}, {}, Presence::{} }},\n", std::make_format_args(field.m_tag, field.m_length, presence));
        }
        out << "    };\n";
        out << "};\n\n";
    }

    void generateGrammar(std::ostream& out)
    {
        out << "#ifndef SIMD_FIX_GRAMMAR_HPP\n";
        out << "#define SIMD_FIX_GRAMMAR_HPP\n\n";
        out << "#include \"org/limitless/fix/decoder/Dictionary.hpp\"\n\n";
        out << "namespace org::limitless::fix::protocols {\n";

        for (auto& record : m_records)
        {
            generateStruct(out, record.second);
        }

        for (auto& message: m_messages)
        {
            generateStruct(out, message.second);
        }
        out << "}\n\n";
        out << "#endif //SIMD_FIX_GRAMMAR_HPP\n";
    }

    // FIXME: does not support groups
    void generateMessages(std::ostream& out)
    {
        out << "#ifndef SIMD_FIX_MESSAGES_HPP\n";
        out << "#define SIMD_FIX_MESSAGES_HPP\n\n";
        out << "#include <expected>\n\n";
        out << "#include \"HeaderDecoder.hpp\"\n";
        out << "#include \"org/limitless/fix/decoder/MessageDecoder.hpp\"\n";
        out << "#include \"org/limitless/fix/decoder/DecoderStatus.hpp\"\n";
        out << "#include \"org/limitless/fix/messages/Grammar.hpp\"\n\n";
        out << "namespace org::limitless::fix::generated {\n\n";
        out << "using namespace org::limitless::fix;\n\n";

        for (auto& message : m_messages | std::views::values)
        {
            auto& name = message.m_name;
            auto& id   = message.m_id;

            // collect groups present in this message
            std::vector<const Field*> groups;
            for (auto& field : message.m_fields)
                if (field.m_type == TypeName::Group)
                    groups.push_back(&field);

            out << std::vformat("struct {}Decoder : decoder::MessageDecoder<protocols::{}>\n{{\n", std::make_format_args(name, name));
            out << "    using Message = MessageDecoder;\n";
            out << "    using Header  = messages::HeaderDecoder<MessageDecoder>;\n";
            for (auto* g : groups)
                out << std::vformat("    using {}Decoder = messages::GroupDecoder<MessageDecoder>;\n", std::make_format_args(g->m_name));
            out << "    using String  = std::span<const uint8_t>;\n\n";
            out << std::vformat("    static constexpr uint16_t MessageId = '{}';\n\n", std::make_format_args(id));
            out << "    Header m_header{this};\n";
            for (auto* g : groups)
            {
                auto memberName = g->m_name;
                memberName[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(memberName[0])));
                out << std::vformat("    {}Decoder m_{}{{this}};\n", std::make_format_args(g->m_name, memberName));
            }
            out << "\n";
            out << std::vformat("    {}Decoder() = default;\n\n", std::make_format_args(name));
            out << std::vformat("    {}Decoder& wrap(const std::span<const uint8_t> data,\n", std::make_format_args(name));
            out << "                        const std::span<Token> tokens,\n";
            out << "                        const std::span<uint16_t> tags,\n";
            out << "                        const uint32_t count)\n";
            out << "    {\n";
            out << "        Message::wrap(data, tokens, tags, count);\n";
            out << "        return *this;\n";
            out << "    }\n\n";

            for (auto& field : message.m_fields)
            {
                if (field.m_type == TypeName::Group || field.m_type == TypeName::GroupMember || field.m_tag == 0)
                    continue;

                auto methodName = field.m_name;
                methodName[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(methodName[0])));
                std::string_view mandatory = (field.m_presence.m_value == decoder::Presence::Required) ? "true" : "false";

                if (field.m_type == TypeName::String)
                {
                    out << std::vformat("    [[nodiscard]] std::expected<String, decoder::DecoderStatus> {}() const\n", std::make_format_args(methodName));
                    out << "    {\n";
                    out << std::vformat("        return this->getString<{}>({});\n", std::make_format_args(field.m_tag, mandatory));
                    out << "    }\n\n";
                }
                else
                {
                    out << std::vformat("    [[nodiscard]] std::expected<uint32_t, decoder::DecoderStatus> {}() const\n", std::make_format_args(methodName));
                    out << "    {\n";
                    out << std::vformat("        return this->getUnsigned<{}>({});\n", std::make_format_args(field.m_tag, mandatory));
                    out << "    }\n\n";
                }
            }

            out << "    Header& header() { return m_header; }\n";
            for (auto* g : groups)
            {
                auto memberName = g->m_name;
                memberName[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(memberName[0])));
                out << std::vformat("    {}Decoder& {}() {{ return m_{}; }}\n", std::make_format_args(g->m_name, memberName, memberName));
            }
            out << "};\n\n";
        }

        out << "} // namespace org::limitless::fix::generated\n\n";
        out << "#endif //SIMD_FIX_MESSAGES_HPP\n";
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
    //generator.print();
    generator.generateGrammar(std::cout);
    //generator.generateMessages(std::cout);
    return 0;
}
