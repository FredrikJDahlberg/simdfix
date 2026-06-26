//
// Created by Fredrik Dahlberg on 2026-06-25.
//

#ifndef SIMD_FIX_MEMORY_STORAGE_HPP
#define SIMD_FIX_MEMORY_STORAGE_HPP

#include <cstdint>
#include <deque>
#include <span>
#include <vector>

#include "org/limitless/fix/storage/Storage.hpp"

namespace org::limitless::fix::storage
{

/**
 * In-memory FixStorageStrategy: retains every appended outbound message in RAM,
 * keyed by a contiguous, monotonically increasing sequence number (as produced
 * by an outbound FIX stream). Suitable for tests and sessions that recover within
 * a single process lifetime; it does not survive a restart.
 *
 * Payload bytes are copied into per-message buffers held in a std::deque, whose
 * elements keep stable addresses as more are appended, so the spans handed out by
 * getMessage / getMessages stay valid. The StoredMessage index is a contiguous
 * vector, which lets getMessages return a requested range as a single span.
 */
class MemoryStorage
{
    std::deque<std::vector<uint8_t>> m_payloads;
    std::vector<StoredMessage> m_messages;
    uint32_t m_baseSeqNum{0};

public:
    /**
     * Persists a copy of an outbound message. Sequence numbers are expected to be
     * appended contiguously from the first one seen.
     * @param seqNum the message's MsgSeqNum
     * @param messageType the message's MsgType
     * @param payload the raw encoded message bytes (copied into the store)
     */
    void appendMessage(const uint32_t seqNum, const uint16_t messageType, const std::span<const uint8_t> payload)
    {
        if (m_messages.empty())
        {
            m_baseSeqNum = seqNum;
        }
        const auto& bytes = m_payloads.emplace_back(payload.begin(), payload.end());
        m_messages.push_back(StoredMessage{seqNum, messageType,
                                           static_cast<uint16_t>(bytes.size()),
                                           std::span<const uint8_t>{bytes}});
    }

    /**
     * @param seqNum the MsgSeqNum to look up
     * @return the stored message, or a default-constructed StoredMessage if the
     *         sequence number is not held
     */
    [[nodiscard]] StoredMessage getMessage(const uint32_t seqNum) const
    {
        if (seqNum < m_baseSeqNum)
        {
            return {};
        }
        const auto index = seqNum - m_baseSeqNum;
        if (index >= m_messages.size())
        {
            return {};
        }
        return m_messages[index];
    }

    /**
     * @param fromSeqNum inclusive lower bound of the requested range
     * @param toSeqNum inclusive upper bound of the requested range
     * @return the stored messages whose sequence numbers fall in
     *         [fromSeqNum, toSeqNum], clamped to what is held; empty if the
     *         intersection is empty
     */
    [[nodiscard]] std::span<const StoredMessage> getMessages(const uint32_t fromSeqNum, const uint32_t toSeqNum) const
    {
        if (m_messages.empty() || toSeqNum < fromSeqNum || toSeqNum < m_baseSeqNum)
        {
            return {};
        }
        const auto lastSeqNum = m_baseSeqNum + static_cast<uint32_t>(m_messages.size()) - 1;
        const auto begin = fromSeqNum < m_baseSeqNum ? m_baseSeqNum : fromSeqNum;
        const auto end = toSeqNum > lastSeqNum ? lastSeqNum : toSeqNum;
        if (begin > end)
        {
            return {};
        }
        return std::span{m_messages}.subspan(begin - m_baseSeqNum, end - begin + 1);
    }

    void clear()
    {
        m_payloads.clear();
        m_messages.clear();
        m_baseSeqNum = 0;
    }
};

static_assert(FixStorageStrategy<MemoryStorage>);

}

#endif //SIMD_FIX_MEMORY_STORAGE_HPP
