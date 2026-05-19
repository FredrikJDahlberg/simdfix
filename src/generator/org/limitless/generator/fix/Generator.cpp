//
// Created by Fredrik Dahlberg on 2026-04-22.
//
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <print>
#include <ranges>
#include <unordered_map>
#include <utility>
#include <vector>

#include "pugixml.hpp"

enum class TypeName
{
    Null,
    Int32,
    String,
    Message,
    Component,
    Group
};

struct Presence
{
    enum Values { Null, Constant, Optional, Required };

    static constexpr std::string_view Names[] = { "??", "Constant", "Optional", "Required" };

    Presence() : m_value{Null} {}

    Presence(const Values value) : m_value{value} {}

    Presence(const std::string_view name) : m_value{Null}
    {
        m_value = Required;
        if (name.empty())
        {
            return;
        }
        for (int i = 1; i < 4; ++i)
        {
            if (Names[i] == name)
            {
                m_value = static_cast<Values>(i);
                return;
            }
        }
    }

    [[nodiscard]] std::string_view name() const
    {
        return Names[m_value];
    }

    Values m_value;
};

struct Type
{
    std::string m_name;
    int32_t m_size;
    int32_t m_length;
    TypeName m_type;

    Type(std::string  name, const int32_t size, const int32_t length)
        : m_name(std::move(name)), m_size(size), m_length(length), m_type{TypeName::Null}
    {
    }
};

struct Field
{
    int32_t m_tag{};
    std::string m_name;
    int32_t m_length{};
    TypeName m_type;
    Presence m_presence;

    Field(const int32_t tag, std::string  name, /*std::string type*/TypeName type, const int32_t length, const Presence presence) :
        m_tag(tag), m_name(std::move(name)), m_length(length), m_type(/*std::move(type)*/type), m_presence{presence} // , m_value{"null"}
    {
    }

    Field() = default;
    Field(const Field& other) = default;
    Field(Field&& other) noexcept = default;
    Field& operator=(const Field& other) = default;
    Field& operator=(Field&& other) noexcept = default;
};

// message or component
struct Struct
{
    std::string m_name;
    TypeName m_type;
    std::vector<Field> m_fields;

    Struct() = default;

    Struct(std::string  name, const TypeName type, const std::vector<Field>& fields)
        : m_name(std::move(name)), m_type(type), m_fields{fields}
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
        processComponents(protocol.children("component"));
        processMessages(protocol.children("message"));
    }

    void processTypes(const pugi::xml_object_range<pugi::xml_node_iterator>& types)
    {
        // FIXME: all supported primitive types
        m_types.try_emplace("char", "char", 1, 1);
        m_types.try_emplace("uint8", "uint8", 1, 1);
        m_types.try_emplace("char", "char", 1, 1);
        m_types.try_emplace("int32", "int32", 4, 1);
        m_types.try_emplace("uint32", "uint32", 4, 1);

        for (auto typeNode : types)
        {
            const std::string_view name = typeNode.attribute("name").as_string();
            const std::string_view primitive = typeNode.attribute("primitiveType").as_string();
            const std::string_view type = typeNode.attribute("type").as_string();
            auto primitiveType = m_types.find(std::string{primitive});
            auto refType = m_types.find(std::string{type});

            // resolve types
            // assume that document is checked by grammar
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
                m_types.try_emplace(std::string{name}, std::string{name}, ref->m_size, length);
            }
        }
    }

    void processComponents(const pugi::xml_object_range<pugi::xml_named_node_iterator>& components)
    {
        for (const auto& component : components)
        {
            std::string_view name = component.attribute("name").as_string();
            std::vector<Field> fields{};
            processFields(component.children(), fields);
            m_records.try_emplace(std::string{name}, std::string{name}, TypeName::Component, fields);
        }
    }

    void processMessages(const pugi::xml_object_range<pugi::xml_named_node_iterator>& messages)
    {
        for (auto& message : messages)
        {
            std::string_view name = message.attribute("name").as_string();
            std::vector<Field> fields{};
            processFields(message.children(), fields);
            m_messages.try_emplace(std::string{name}, std::string{name}, TypeName::Message, fields);
        }
    }

    void processFields(const pugi::xml_object_range<pugi::xml_node_iterator>& nodes, std::vector<Field>& fields)
    {
        for (const auto& field : nodes)
        {
            const std::string_view nodeType = field.name();
            const std::string_view name = field.attribute("name").as_string();
            const int32_t tag = field.attribute("tag").as_int();
            const std::string_view type = field.attribute("type").as_string();
            const std::string_view primitive = field.attribute("primitiveType").as_string();
            const std::string resolvedType = std::string{type.empty() ? primitive : type};
            const int32_t length = field.attribute("length").as_int();
            const std::string_view presenceAttr = field.attribute("presence").as_string();
            if (!presenceAttr.empty())
            {
            }
            Presence presence{presenceAttr};
            auto typeName = std::string{name};
            auto refType = m_types.find(resolvedType);
            if (nodeType == "group")
            {
                std::println("Group found {}", name);
                // auto counter = m_types.find("int32");
                const auto counterName = field.attribute("counter").as_string();
                Field counter{tag, std::string{counterName}, TypeName::Group, length, presence};
                fields.emplace_back(std::move(counter));
                processFields(field.children(), fields);
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
                    std::println("Field '{}' type '{}' found", name, type);

                    fields.emplace_back(tag, std::string{name}, refType->second.m_type, length, presence);
                }
            }
            std::println("field: {}, {}, {}, {}", name, tag, primitive, length);
        }
    }

    void resolveType(const Field& field, Struct& record)
    {
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

    void generateGrammar()
    {
        std::println("#ifndef SIMD_FIX_GRAMMAR_HPP");
        std::println("#define SIMD_FIX_GRAMMAR_HPP\n");
        std::println("#include \"org/limitless/fix/decoder/Dictionary.hpp\"\n");
        std::println("namespace org::limitless::fix::protocols {{");

        for (auto& message: m_messages | std::views::values)
        {
            auto sorted = message.m_fields;
            std::ranges::sort(sorted, {}, &Field::m_tag);
            std::println("struct {} {{", message.m_name);
            std::println("    static constexpr uint16_t Tags[] = {{", message.m_name);
            for (auto& field : sorted)
            {
                std::println("        {},", field.m_tag);
            }
            std::println("    }};\n");
            std::println("    static constexpr Dictionary Grammar[] = {{", message.m_name);
            for (auto& field : sorted)
            {
                std::println("        {{ {}, Presence::{}, {} }}, ", field.m_tag, field.m_presence.name(), field.m_name);
            }
            std::println("    }};");
            std::println("}};\n");
        }
        std::println("}}\n");
        std::println("#endif //SIMD_FIX_GRAMMAR_HPP");
    }
};

int main(int argc, char** argv)
{
    pugi::xml_document doc;
    const auto result = doc.load_file(argv[1]);
    if (!result)
    {
        std::println("XML error: {}, dir = {}, file = {}",
            result.description(), std::filesystem::current_path().c_str(), argv[1]);
        return 1;
    }

    Presence pre = Presence::Constant;

    Generator generator{};
    generator.process(doc);
    generator.print();
    generator.generateGrammar();
    return 0;
}

