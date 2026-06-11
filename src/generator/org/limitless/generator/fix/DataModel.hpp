//
// Created by Fredrik Dahlberg on 2026-06-06.
//

#ifndef SIMDFIX_DATA_MODEL_HPP
#define SIMDFIX_DATA_MODEL_HPP

#include <vector>
#include <unordered_map>

#include "pugixml.hpp"

#include "org/limitless/fix/decoder/DecoderTypes.hpp"

namespace org::limitless::generator::fix {

using namespace org::limitless::fix::decoder;

using limitless::fix::decoder::Category;
using limitless::fix::decoder::ParentType;

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
    Category m_category{Category::Null};
    ParentType m_parent{};
    Presence m_presence{Presence::Required};

    Field() = default;
    Field(const int32_t tag, std::string  name, std::string type,
          const int32_t length, const Presence presence, const Category category, const ParentType parent) :
        m_tag(tag), m_name(std::move(name)), m_type(std::move(type)), m_length(length),
        m_category(category), m_parent(parent), m_presence{presence}
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
    ParentType m_parent{};
    std::vector<Field> m_fields{};
    std::vector<Field> m_records{};
    uint32_t m_tag;

    Record() = default;

    Record(std::string name, std::string id, const ParentType parent)
        : m_name(std::move(name)), m_id(std::move(id)), m_parent(parent), m_tag{0}
    {
    }

    Record(std::string name, std::string id, const ParentType type, const std::vector<Field>& fields)
        : m_name(std::move(name)), m_id(std::move(id)), m_parent(type), m_fields{fields}, m_tag{0}
    {
    }

    Record(const Record& other) = default;
    Record(Record&& other) noexcept = default;
    Record& operator=(const Record& other) = default;
    Record& operator=(Record&& other) noexcept = default;
};

struct DataModel
{
    std::unordered_map<std::string, Type> m_types{};
    std::unordered_map<std::string, Record> m_recordsByType{};
    std::vector<Record> m_enums{};
    std::vector<Record> m_records{};

    void processTypes(const pugi::xml_object_range<pugi::xml_node_iterator>& types)
    {
        m_types.try_emplace("char",   "char",   1, 1, Category::String);
        m_types.try_emplace("uint8",  "uint8",  1, 1, Category::Uint8);
        m_types.try_emplace("int32",  "int32",  4, 1, Category::Int32);
        m_types.try_emplace("uint32", "uint32", 4, 1, Category::Uint32);
        m_types.try_emplace("int64",  "int32",  4, 1, Category::Int64);
        m_types.try_emplace("uint64", "uint32", 4, 1, Category::Uint64);
        m_types.try_emplace("timestamp", "std::chrono::millis", 8, 1, Category::Timestamp);
        m_types.try_emplace("string", "std::string_view", 1, 1, Category::String);
        for (auto typeNode : types)
        {
            const auto name = std::string{typeNode.attribute("name").as_string()};
            const auto primitive = std::string{typeNode.attribute("primitiveType").as_string()};
            const auto type = std::string{typeNode.attribute("type").as_string()};
            auto primitiveType = m_types.find(std::string{primitive});
            auto refType = m_types.find(std::string{type});
            if (std::strncmp(typeNode.name(), "enum", 4) == 0)
            {
                Record record{name, std::string{}, ParentType::Enum};
                record.m_fields.emplace_back(0, "Null", "?", 1, Presence::Null,
                                             Category::Enum, ParentType::Enum);
                for (auto element : typeNode.children())
                {
                    const auto fieldName = std::string{element.attribute("name").as_string()};
                    const auto fieldValue = std::string{element.attribute("value").as_string()};
                    record.m_fields.emplace_back(0, fieldName, fieldValue,1, Presence::Null,
                                                 Category::Enum, ParentType::Enum);
                }
                m_types.try_emplace(name, name, 1, 1, Category::Enum);
                m_enums.emplace_back(record);
            }
            else if (!primitive.empty() && primitiveType == m_types.end())
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
                m_types.try_emplace(name, name, ref->m_size, length, ref->m_type);
            }
        }
    }

    void processFields(const pugi::xml_object_range<pugi::xml_node_iterator>& nodes,
                       const ParentType parent,
                       Record& record)
    {
        for (const auto& field : nodes)
        {
            const std::string_view nodeType = field.name();
            const auto name = std::string{field.attribute("name").as_string()};
            const int32_t tag = field.attribute("tag").as_int();
            const auto type = std::string{field.attribute("type").as_string()};
            const std::string_view primitive = field.attribute("primitiveType").as_string();
            const auto resolvedType = std::string{type.empty() ? primitive : type};
            const std::string_view presenceAttr = field.attribute("presence").as_string();
            Presence presence{presenceAttr};
            auto refType = m_types.find(resolvedType);
            if (nodeType == "group")
            {
                const auto groupName = std::string{field.attribute("name").as_string()};
                record.m_fields.emplace_back(0, groupName, groupName, 0, presence,
                                             Category::Struct, parent);
                const auto counterName = std::string{field.attribute("counter").as_string()};
                record.m_fields.emplace_back(tag, counterName, groupName, 0, presence,
                                             Category::Counter, parent);
                record.m_records.emplace_back(tag, groupName, groupName, 0, presence,
                                              Category::Group, parent);
            }
            else
            {
                if (refType == m_types.end())
                {
                    if (auto found = m_recordsByType.find(resolvedType); found != m_recordsByType.end())
                    {
                        auto& component = found->second;
                        record.m_fields.append_range(component.m_fields);
                        for (const auto& field : component.m_records)
                        {
                            record.m_records.emplace_back(field.m_tag, field.m_name, field.m_name, 0,
                                                          presence, Category::Struct, parent);
                        }
                    }
                    else
                    {
                        std::println("type not found {} for field {}", name, field.name());
                    }
                }
                else
                {
                    const auto& ref = refType->second;
                    const int32_t length = std::max(field.attribute("length").as_int(), ref.m_length);
                    record.m_fields.emplace_back(tag, name, resolvedType, length,
                                                 presence, ref.m_type, parent);
                }
            }
        }
    }

    void processRecords(const pugi::xpath_node_set& records, const ParentType parent)
    {
        for (const auto& recordNode : records)
        {
            processRecords(recordNode.node().select_nodes("group"), ParentType::Group);

            const auto node = recordNode.node();
            std::string_view name = node.attribute("name").as_string();
            std::vector<Field> fields{};
            Record record{std::string{name}, std::string{}, parent, fields};
            processFields(node.children(), parent, record);
            if (parent == ParentType::Message)
            {
                record.m_id = node.attribute("id").as_string();
            }
            if (parent == ParentType::Group)
            {
                record.m_tag = node.attribute("tag").as_int();
            }
            for (const auto& field : fields)
            {
                if (field.m_category == Category::Struct)
                {
                    record.m_records.push_back(field);
                }
            }
            auto [old, success] = m_recordsByType.try_emplace(std::string{name}, record);
            if (success)
            {
                if (record.m_parent != ParentType::Component)
                {
                    m_records.emplace_back(record);
                }
            }
        }
    }

    void process(const pugi::xml_document& doc)
    {
        const pugi::xml_node protocol = doc.child("protocol");
        processTypes(protocol.child("types").children());
        processRecords(protocol.select_nodes(".//component"), ParentType::Component);
        processRecords(protocol.select_nodes(".//group"), ParentType::Group);
        processRecords(protocol.select_nodes(".//message"), ParentType::Message);
    }
};
}

#endif //SIMDFIX_DATA_MODEL_HPP
