//
// Created by Fredrik Dahlberg on 2026-06-26.
//

#ifndef SIMD_FIX_CONFIG_MAPPING_HPP
#define SIMD_FIX_CONFIG_MAPPING_HPP

#include <span>
#include <string>
#include <string_view>

namespace org::limitless::generator::fix {

/**
 * One row of a strategy table: the config attribute value (@c name), the
 * fully-qualified C++ type it resolves to (@c type), and the extra header that
 * provides that type (@c header, empty when the type is already reachable via
 * the always-included session headers).
 *
 * To add a built-in storage/transport/application class, add a row to the
 * corresponding table below; nothing else in the generator needs to change.
 */
struct ClassMapping
{
    std::string_view name;
    std::string_view type;
    std::string_view header;
};

// First row of each table is the default selected by an unspecified attribute.

inline constexpr ClassMapping StorageClasses[] = {
    {"null",   "storage::NullStorage",   ""},
    {"memory", "storage::MemoryStorage", "org/limitless/fix/storage/MemoryStorage.hpp"},
};

inline constexpr ClassMapping TransportClasses[] = {
    {"discard", "DiscardTransport", ""},
};

inline constexpr ClassMapping ApplicationClasses[] = {
    {"reject", "RejectApplication", ""},
};

/// A resolved strategy: its C++ type and the extra header to include (if any).
struct ResolvedClass
{
    std::string type;
    std::string header;
};

/**
 * Resolves a config attribute value against a strategy table. An empty value
 * selects the table's default (first row); a known value maps to its built-in
 * type; any other value is treated as a fully-qualified custom type written
 * verbatim, with no header added (the author includes it themselves).
 */
inline ResolvedClass resolveClass(const std::span<const ClassMapping> table, const std::string& value)
{
    if (value.empty())
    {
        return {std::string{table.front().type}, std::string{table.front().header}};
    }
    for (const auto& entry : table)
    {
        if (entry.name == value)
        {
            return {std::string{entry.type}, std::string{entry.header}};
        }
    }
    return {value, {}};
}

/**
 * Maps a config <session> role attribute to its CRTP session base class.
 */
inline std::string sessionClass(const std::string& role)
{
    return role == "client" ? "ClientSession" : "ServerSession";
}

} // namespace org::limitless::generator::fix

#endif //SIMD_FIX_CONFIG_MAPPING_HPP