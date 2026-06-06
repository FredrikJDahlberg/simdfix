//
// Created by Fredrik Dahlberg on 2026-06-06.
//

#ifndef SIMD_FIX_PROCESSOR_HPP
#define SIMD_FIX_PROCESSOR_HPP

#include <algorithm>
#include <ranges>
#include <unordered_map>
#include <vector>

#include "pugixml.hpp"

#include "org/limitless/generator/fix/DataModel.hpp"

namespace org::limitless::generator::fix {
struct Processor
{
    std::unordered_map<std::string, Type> m_types{};
    std::unordered_map<std::string, Record> m_recordsByType{};
    std::vector<Record> m_records{};
    std::vector<Record> m_grammar{};

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

    void processFields(const pugi::xml_object_range<pugi::xml_node_iterator>& nodes, const Parent parent, Record& record)
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
            Presence presence{presenceAttr};
            auto refType = m_types.find(resolvedType);
            if (nodeType == "group")
            {
                const auto groupName = field.attribute("name").as_string();
                record.m_fields.emplace_back(0, std::string{groupName}, std::string{groupName}, 0,
                                             presence, Category::Struct, parent);
                const auto counterName = field.attribute("counter").as_string();
                record.m_fields.emplace_back(tag, std::string{counterName}, std::string{groupName}, 0,
                                             presence, Category::Counter, parent);
                record.m_records.emplace_back(tag, groupName, groupName, 0, presence,
                                              Category::Group, parent);
            }
            else
            {
                if (refType == m_types.end())
                {
                    auto found = m_recordsByType.find(resolvedType);
                    if (found != m_recordsByType.end())
                    {
                        auto comp = found->second.m_name;
                        record.m_fields.emplace_back(0, std::string{name}, std::string{type}, 0,
                                                     presence, Category::Struct, parent);
                        record.m_records.emplace_back(0, comp, comp, 0, presence,
                                                      Category::Struct, parent);
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
                    record.m_fields.emplace_back(tag, std::string{name}, std::string{type}, length,
                                                 presence, ref.m_type, parent);
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
            Record record{std::string{name}, std::string{}, parent, fields};
            processFields(node.children(), parent, record);
            if (parent == Parent::Message)
            {
                record.m_id = node.attribute("id").as_string();
            }
            for (const auto& field : fields)
            {
                if (field.m_category == Category::Struct)
                {
                    record.m_records.push_back(field);
                }
            }
            m_recordsByType.emplace(std::string{name}, record);
            m_records.emplace_back(record);
        }
    }

    void resolveGrammar(const Record& old)
    {
        auto record = old; // copy
        while (!record.m_records.empty())
        {
            const auto& ref = record.m_records.back();
            if (const auto found = m_recordsByType.find(ref.m_type); found != m_recordsByType.end())
            {
                const auto& component = found->second;
                record.m_fields.append_range(component.m_fields);
                record.m_records.pop_back();
                record.m_records.append_range(component.m_records);
            }
            else
            {
                std::println("Record not found: name = {}, type = {}", ref.m_name, ref.m_type);
                exit(1);
            }
        }
        std::erase_if(record.m_fields, [](const Field& f) { return f.m_tag == 0; });
        std::ranges::sort(record.m_fields, {}, &Field::m_tag);
        m_grammar.push_back(record);
    }

    void process(const pugi::xml_document& doc)
    {
        const pugi::xml_node protocol = doc.child("protocol");
        processTypes(protocol.child("types").children());
        processRecords(protocol.select_nodes(".//group"), Parent::Group);
        processRecords(protocol.select_nodes(".//component"), Parent::Component);
        processRecords(protocol.select_nodes(".//message"), Parent::Message);
        for (auto& record : m_records)
        {
            if (record.m_parent == Parent::Message)
            {
                resolveGrammar(record);
            }
        }
    }

};
}

#endif //SIMD_FIX_PROCESSOR_HPP
