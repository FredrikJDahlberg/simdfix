//
// C++20 polyfill for std::expected / std::unexpected.
// On C++23 or later this header simply pulls in <expected>.
//

#ifndef SIMD_FIX_EXPECTED_HPP
#define SIMD_FIX_EXPECTED_HPP

#include <version>

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202211L

#include <expected>

namespace org::limitless::fix
{
    template <typename T, typename E>
    using expected = std::expected<T, E>;

    template <typename E>
    using unexpected = std::unexpected<E>;
}

#else

#include <memory>
#include <type_traits>
#include <utility>

namespace org::limitless::fix
{

template <typename E>
class unexpected
{
    E m_error;

public:
    constexpr explicit unexpected(const E& error) : m_error(error) {}
    constexpr explicit unexpected(E&& error) : m_error(std::move(error)) {}

    [[nodiscard]] constexpr const E& error() const & noexcept { return m_error; }
    [[nodiscard]] constexpr E& error() & noexcept { return m_error; }
    [[nodiscard]] constexpr E&& error() && noexcept { return std::move(m_error); }
};

template <typename E>
unexpected(E) -> unexpected<E>;

template <typename T, typename E>
class expected
{
    union
    {
        T m_value;
        E m_error;
    };
    bool m_hasValue;

public:
    using value_type = T;
    using error_type = E;

    constexpr expected() requires std::is_default_constructible_v<T>
        : m_value{}, m_hasValue{true} {}

    constexpr expected(const T& value) : m_value(value), m_hasValue(true) {}
    constexpr expected(T&& value) : m_value(std::move(value)), m_hasValue(true) {}

    constexpr expected(const unexpected<E>& err)
        : m_error(err.error()), m_hasValue(false) {}

    constexpr expected(unexpected<E>&& err)
        : m_error(std::move(err).error()), m_hasValue(false) {}

    constexpr expected(const expected& other)
        : m_hasValue(other.m_hasValue)
    {
        if (m_hasValue)
            std::construct_at(&m_value, other.m_value);
        else
            std::construct_at(&m_error, other.m_error);
    }

    constexpr expected(expected&& other) noexcept
        : m_hasValue(other.m_hasValue)
    {
        if (m_hasValue)
            std::construct_at(&m_value, std::move(other.m_value));
        else
            std::construct_at(&m_error, std::move(other.m_error));
    }

    constexpr ~expected()
    {
        if (m_hasValue)
            std::destroy_at(&m_value);
        else
            std::destroy_at(&m_error);
    }

    constexpr expected& operator=(const expected& other)
    {
        if (this != &other)
        {
            this->~expected();
            std::construct_at(this, other);
        }
        return *this;
    }

    constexpr expected& operator=(expected&& other) noexcept
    {
        if (this != &other)
        {
            this->~expected();
            std::construct_at(this, std::move(other));
        }
        return *this;
    }

    constexpr expected& operator=(const unexpected<E>& err)
    {
        this->~expected();
        std::construct_at(&m_error, err.error());
        m_hasValue = false;
        return *this;
    }

    [[nodiscard]] constexpr bool has_value() const noexcept { return m_hasValue; }
    constexpr explicit operator bool() const noexcept { return m_hasValue; }

    [[nodiscard]] constexpr const T& value() const & { return m_value; }
    [[nodiscard]] constexpr T& value() & { return m_value; }
    [[nodiscard]] constexpr T&& value() && { return std::move(m_value); }

    [[nodiscard]] constexpr const E& error() const & { return m_error; }
    [[nodiscard]] constexpr E& error() & { return m_error; }

    [[nodiscard]] constexpr const T& operator*() const & noexcept { return m_value; }
    [[nodiscard]] constexpr T& operator*() & noexcept { return m_value; }

    constexpr const T* operator->() const noexcept { return &m_value; }
    constexpr T* operator->() noexcept { return &m_value; }

    template <typename U>
    [[nodiscard]] constexpr T value_or(U&& defaultValue) const &
    {
        return m_hasValue ? m_value : static_cast<T>(std::forward<U>(defaultValue));
    }
};

} // namespace org::limitless::fix

#endif // __cplusplus check

#endif //SIMD_FIX_EXPECTED_HPP
