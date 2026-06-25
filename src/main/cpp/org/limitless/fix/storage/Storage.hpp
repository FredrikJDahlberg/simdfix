//
// Created by Fredrik Dahlberg on 2026-06-25.
//

#ifndef SIMD_FIX_STORAGE_HPP
#define SIMD_FIX_STORAGE_HPP

#include <concepts>
#include <cstdint>
#include <span>

namespace org::limitless::fix::storage
{

/**
 * One stored outbound message: its sequence number, MsgType, and the raw encoded
 * payload bytes. The payload is a view into store-owned memory and stays valid
 * for as long as the store retains the message.
 */
struct StoredMessage
{
    uint32_t m_sequenceNumber;
    uint16_t m_messageType;
    uint16_t m_payloadSize;
    std::span<const uint8_t> m_payload;
};

/**
 * Persistence strategy for the resend / gap-fill path: wraps a store of many
 * outbound messages keyed by sequence number. appendMessage persists a message
 * triple; getMessage returns a single stored message and getMessages returns the
 * inclusive [fromSeqNum, toSeqNum] range as a span for replaying a ResendRequest
 * in one call.
 *
 * This header stays free of the heavy I/O includes a concrete (in-memory or
 * file-backed) store needs; those belong in the store's own translation unit.
 */
template <typename Storage>
concept FixStorageStrategy = requires(Storage storage, uint32_t seqNum,
                                      uint32_t fromSeqNum, uint32_t toSeqNum,
                                      uint16_t messageType, std::span<const uint8_t> payload)
{
    { storage.appendMessage(seqNum, messageType, payload) } -> std::same_as<void>;
    { storage.getMessage(seqNum) } -> std::same_as<StoredMessage>;
    { storage.getMessages(fromSeqNum, toSeqNum) } -> std::same_as<std::span<const StoredMessage>>;
    { storage.clear() } -> std::same_as<void>;
};

/**
 * Default no-op store: satisfies FixStorageStrategy while retaining nothing. Acts
 * as a placeholder so a session can be instantiated without persistence; a real
 * store is supplied via the Session's storage template parameter.
 */
struct NullStorage
{
    void appendMessage(uint32_t, uint16_t, std::span<const uint8_t>) noexcept {}
    [[nodiscard]] StoredMessage getMessage(uint32_t) const noexcept { return {}; }
    [[nodiscard]] std::span<const StoredMessage> getMessages(uint32_t, uint32_t) const noexcept { return {}; }
    void clear() noexcept {}
};

}

#endif //SIMD_FIX_STORAGE_HPP
