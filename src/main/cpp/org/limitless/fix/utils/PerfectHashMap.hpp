//
// Created by Fredrik Dahlberg on 2026-04-27.
//

#ifndef SIMD_FIX_PERFECT_HASH_MAP_HPP
#define SIMD_FIX_PERFECT_HASH_MAP_HPP

#include <span>
#include <array>
#include <optional>
#include <cstdint>

struct BucketInfo
{
    uint32_t salt = 0;
    uint32_t offset = 0;
    uint16_t size = 0;
};

template<typename T>
struct Entry
{
    uint32_t tag;
    T data; // Your 16-byte metadata (including tag)
};

template<size_t N, typename T>
class PerfectHashMap
{
    // Using N buckets makes finding salts significantly faster for the compiler
    static constexpr size_t NumBuckets = (N > 0) ? N : 1;

    struct BucketInfo
    {
        uint32_t salt = 0;
        uint32_t offset = 0;
        uint32_t size = 0;
    };

    std::array<Entry<T>, N> values{};
    std::array<BucketInfo, NumBuckets> index{};

    // Fast bit mixer to distribute tags into buckets
    static constexpr uint32_t hash1(const uint32_t tag)
    {
        const uint32_t hash = tag * 0x45d9f3b;
        return hash % NumBuckets;
    }

    // High-entropy mixer for resolving local collisions
    static constexpr uint32_t hash2(const uint32_t tag, const uint32_t salt, const uint32_t size)
    {
        if (size <= 1)
        {
            return 0;
        }
        uint32_t hash = tag ^ salt;
        hash ^= hash >> 16;
        hash *= 0x85ebca6b;
        hash ^= hash >> 13;
        hash *= 0xc2b2ae35;
        hash ^= hash >> 16;
        return hash % size;
    }

public:
    explicit constexpr PerfectHashMap(std::span<const Entry<T>, N> span)
    {
        std::array<Entry<T>, N> input;
        for(size_t i = 0; i < N; ++i)
        {
            input[i] = span[i];
        }
        for (size_t i = 0; i < N; ++i)
        {
            for (size_t j = i + 1; j < N; ++j)
            {
                if (input[i].tag == input[j].tag)
                {
                    throw std::invalid_argument("PerfectHashMap: Duplicate tag found in input array!");
                }
            }
        }

        // 1. Distribution Phase
        std::array<uint32_t, NumBuckets> bucketSizes{};
        for (const auto& item: input)
        {
            ++bucketSizes[hash1(item.tag)];
        }

        uint32_t currentOffset = 0;
        for (size_t i = 0; i < NumBuckets; ++i)
        {
            index[i].offset = currentOffset;
            index[i].size = bucketSizes[i];
            currentOffset += bucketSizes[i];
        }

        // 2. Salt Finding Phase
        for (size_t i = 0; i < NumBuckets; ++i)
        {
            if (index[i].size == 0)
            {
                continue;
            }
            // Collect tags for this specific bucket
            uint32_t localTags[N];
            uint32_t count = 0;
            for (const auto& item: input)
            {
                if (hash1(item.tag) == i) localTags[count++] = item.tag;
            }

            uint32_t salt = 0;
            bool collision = true;
            while (collision)
            {
                // Safety valve for compiler step limit
                if (salt > 2000)
                {
                    throw std::invalid_argument("PerfectHashMap: Could not find salt.");
                }
                collision = false;
                for (uint32_t a = 0; a < count; ++a)
                {
                    for (uint32_t bucket = a + 1; bucket < count; ++bucket)
                    {
                        if (hash2(localTags[a], salt, count) == hash2(localTags[bucket], salt, count))
                        {
                            collision = true;
                            break;
                        }
                    }
                    if (collision)
                    {
                        break;
                    }
                }
                if (collision)
                {
                    salt++;
                }
            }
            index[i].salt = salt;

            // 3. Placement Phase
            for (const auto& item: input)
            {
                if (hash1(item.tag) == i)
                {
                    uint32_t pos = index[i].offset + hash2(item.tag, salt, count);
                    values[pos] = item;
                }
            }
        }
    }

    constexpr std::optional<T> lookup(uint32_t tag) const
    {
        const auto& bucket = index[hash1(tag)];
        if (bucket.size == 0)
        {
            return std::nullopt;
        }
        const auto& entry = values[bucket.offset + hash2(tag, bucket.salt, bucket.size)];
        return (entry.tag == tag) ? std::make_optional(entry.data) : std::nullopt;
    }
};

template<size_t N, typename T> PerfectHashMap(std::span<const Entry<T>, N>) -> PerfectHashMap<N, T>;

#endif //SIMD_FIX_PERFECT_HASH_MAP_HPP
