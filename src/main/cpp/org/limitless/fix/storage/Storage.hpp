//
// Created by Fredrik Dahlberg on 2026-06-25.
//

#ifndef SIMD_FIX_STORAGE_HPP
#define SIMD_FIX_STORAGE_HPP

#include <concepts>
#include <string_view>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>

#include "org/limitless/fix/decoder/MessageDecoder.hpp"

namespace org::limitless::fix::storage {

template <typename T>
concept FixStorageStrategy = requires(T storage, int seqNum, std::string_view msg) {
    { storage.appendMessage(seqNum, msg) } -> std::same_as<void>;
    { storage.getMessage(seqNum) } -> std::same_as<const decoder::MessageDecoder&>;
    { storage.clear() } -> std::same_as<void>;
};

}

#endif //SIMD_FIX_STORAGE_HPP
