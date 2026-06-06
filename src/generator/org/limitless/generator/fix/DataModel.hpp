//
// Created by Fredrik Dahlberg on 2026-06-06.
//

#ifndef SIMDFIX_DATA_MODEL_HPP
#define SIMDFIX_DATA_MODEL_HPP

#include "org/limitless/fix/decoder/Dictionary.hpp"

namespace org::limitless::generator::fix {

using namespace org::limitless::fix::decoder;

using limitless::fix::decoder::Category;
using limitless::fix::decoder::Parent;

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
    Parent m_parent{};
    Presence m_presence{Presence::Required};

    Field() = default;
    Field(const int32_t tag, std::string  name, std::string type,
          const int32_t length, const Presence presence, Category category, Parent parent) :
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
    Parent m_parent{};
    std::vector<Field> m_fields{};
    std::vector<Field> m_records{};

    Record() = default;

    Record(std::string name, std::string id, const Parent type, const std::vector<Field>& fields)
        : m_name(std::move(name)), m_id(std::move(id)), m_parent(type), m_fields{fields}
    {
    }

    Record(const Record& other) = default;
    Record(Record&& other) noexcept = default;
    Record& operator=(const Record& other) = default;
    Record& operator=(Record&& other) noexcept = default;
};

}

#endif //SIMDFIX_DATA_MODEL_HPP
