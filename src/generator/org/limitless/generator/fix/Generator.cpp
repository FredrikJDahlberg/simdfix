//
// Created by Fredrik Dahlberg on 2026-04-22.
//
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <print>
#include <ranges>
#include <unordered_map>
#include <vector>

#include "pugixml.hpp"

enum class PrimitiveType
{
    Null,
    Int32,
    String
};

enum class CompositeType
{
    Message,
    Record,
    Group
};

struct Type
{
    std::string m_name;
    int32_t m_size;
    int32_t m_length;

    Type(const std::string& name, int32_t size, int32_t length)
        : m_name(name), m_size(size), m_length(length)
    {
    }
};

struct Field
{
    int32_t m_tag;
    std::string m_name;
    int32_t m_alignment;
    int32_t m_length;
    PrimitiveType m_type;

    Field(const int32_t tag, const std::string& name, int32_t alignment, int32_t length, PrimitiveType type) :
        m_tag(tag), m_name(name), m_alignment(alignment), m_length(length), m_type(type)
    {
    }

    Field(const Field& other) = default;
    Field(Field&& other) noexcept = default;
    Field& operator=(const Field& other) = default;
    Field& operator=(Field&& other) noexcept = default;
};

struct Struct
{
    std::string m_name;
    Type m_type;
    std::vector<Field> m_fields;

    Struct(const std::string& name, Type type, std::vector<Field> fields)
        : m_name(name), m_type(type), m_fields(std::move(fields))
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

    void process(pugi::xml_document& doc)
    {
        const pugi::xml_node protocol = doc.child("protocol");

        processTypes(protocol.child("types").children());
        for (pugi::xml_node node : protocol.children())
        {
#if 0
            auto nodeName = node.name();
            std::println("{}", nodeName);
            auto type = PrimitiveType::Null;
            if (std::strncmp(nodeName, "record", 6) == 0)
            {
                type = CompositeType::Record;
            }
            else if (std::strncmp(nodeName, "message", 7) == 0)
            {
                type = CompositeType::Message;
            }
            if (type != Type::Null)
            {
                const auto name = node.attribute("name").value();
                std::vector<Field> fields{};
                for (pugi::xml_node field : node.children())
                {
                    auto fieldName = std::string{field.attribute("name").value()};
                    auto fieldType = field.name();
                    auto fieldTag = field.attribute("tag").as_int();
                    if (strncmp(fieldType, "string", 6) == 0)
                    {
                        auto length = field.attribute("length").as_int();
                        fields.emplace_back(fieldTag, fieldName, 1, length, Type::String);
                    }
                    else if (strncmp(fieldType, "int32", 5) == 0)
                    {
                        fields.emplace_back(fieldTag, fieldName, 4, 1, Type::Int32);
                    }
                }
                std::ranges::sort(fields, {}, &Field::m_tag);
                if (type == Type::Message)
                {
                    m_messages.try_emplace(name, std::string{name}, type, std::move(fields));
                }
                else
                {
                    m_records.try_emplace(name, std::string{name}, type, std::move(fields));
                }
            }
#endif
        }
#if 0
        auto header = m_records.find("StandardHeader");
        auto trailer = m_records.find("StandardTrailer");
        for (auto message : m_messages)
        {
            message.second.m_fields.append_range(header->second.m_fields);
            message.second.m_fields.append_range(trailer->second.m_fields);
            std::ranges::sort(message.second.m_fields, {}, &Field::m_tag);
        }
#endif
    }

    void processTypes(pugi::xml_object_range<pugi::xml_node_iterator> types)
    {
        // FIXME: all supported primitive types
        m_types.try_emplace("uint8", "uint8", 1, 1);
        m_types.try_emplace("char", "char", 1, 1);
        m_types.try_emplace("int32", "int32", 4, 1);
        m_types.try_emplace("uint32", "uint32", 4, 1);

        for (auto type : types)
        {
            const std::string_view name = type.attribute("name").as_string();
            const std::string_view primitiveAttr = type.attribute("primitiveType").as_string();
            const std::string_view typeAttr = type.attribute("type").as_string();
            auto primitiveType = m_types.find(std::string{primitiveAttr});
            auto refType = m_types.find(std::string{typeAttr});
            if (primitiveAttr.empty() == typeAttr.empty())
            {
                std::println("Error: one primitiveType or derived type must be defined");
            }
            else if (!primitiveAttr.empty() && primitiveType == m_types.end())
            {
                std::println("Error: type {} has an invalid primitive type {}", name, primitiveAttr);
            }
            else if (!typeAttr.empty() && refType == m_types.end())
            {
                std::println("Error: type {} has an invalid derived {}", name, typeAttr);
            }
            else
            {
                auto* ref = (refType != m_types.end()) ? &refType->second : &primitiveType->second;
                auto length = ref->m_length;
//                auto lengthAttr = type.attribute("length").as_int();
                m_types.try_emplace(std::string{name}, std::string{name}, ref->m_size, ref->m_length);
            }
        }
    }

    void print()
    {
        for (auto& pair : m_types)
        {
            auto& type = pair.second;
            std::println("Type{{name={}, align={}, length={}}}", type.m_name, type.m_size, type.m_length);
        }
#if 0
        for (auto& pair : m_records)
        {
            auto& record = pair.second;
            std::println("{}", record.m_name);
            for (auto& field : record.m_fields)
            {
                std::println("    {} = {}, {}/{} {}", field.m_tag, field.m_name, field.m_alignment, field.m_length, (int)field.m_type);
            }
        }
#endif
    }

    void generateGrammar()
    {
        for (auto& message: m_messages | std::views::values)
        {
            std::println("struct {} {{", message.m_name);
            for (auto& field : message.m_fields)
            {
                std::println("    {},", field.m_tag);
            }
            std::println("}};\n");
        }
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
    Generator generator;
    generator.process(doc);
    generator.print();
    generator.generateGrammar();
    return 0;
}

