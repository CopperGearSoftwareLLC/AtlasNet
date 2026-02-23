#pragma once
#include <bit>
#include <boost/beast/core/detail/base64.hpp>
#include <boost/container/small_vector.hpp>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>

#include "ByteStream.hpp"
#include "Global/Misc/UUID.hpp"
#include "Global/pch.hpp"
class ByteWriter
{
public:
    ByteWriter() = default;
    explicit ByteWriter(size_t reserve) { buf.reserve(reserve); }

    std::span<const uint8_t> bytes() const { return buf; }
    const uint8_t* data() const { return buf.data(); }
    size_t size() const { return buf.size(); }

    std::string_view as_string_view() const
    {
        const auto data = bytes();
        return std::string_view(reinterpret_cast<const char*>(data.data()), data.size());
    }

    std::string as_string_base_64() const
    {
        std::size_t encoded_len =
            boost::beast::detail::base64::encoded_size(buf.size());

        std::string out;
        out.resize(encoded_len);

        const size_t written_count =
            boost::beast::detail::base64::encode(out.data(), buf.data(), buf.size());

        ASSERT(written_count > 0, "BASE64 encode did not write");
        return out;
    }

    // ---------------- Core ---------------------------------------

    ByteWriter& clear()
    {
        buf.clear();
        return *this;
    }

    ByteWriter& write(const void* p, size_t n)
    {
        const uint8_t* b = static_cast<const uint8_t*>(p);
        buf.insert(buf.end(), b, b + n);
        return *this;
    }

    // ---------------- Integers -----------------------------------

    ByteWriter& u8(uint8_t v) { buf.push_back(v); return *this; }
    ByteWriter& i8(int8_t v)  { buf.push_back(uint8_t(v)); return *this; }

    ByteWriter& u16(uint16_t v) { push_be(v); return *this; }
    ByteWriter& u32(uint32_t v) { push_be(v); return *this; }
    ByteWriter& u64(uint64_t v) { push_be(v); return *this; }

    ByteWriter& i16(int16_t v) { return u16(uint16_t(v)); }
    ByteWriter& i32(int32_t v) { return u32(uint32_t(v)); }
    ByteWriter& i64(int64_t v) { return u64(uint64_t(v)); }

    // ---------------- Floats -------------------------------------

    ByteWriter& f32(float v)
    {
        return u32(std::bit_cast<uint32_t>(v));
    }

    ByteWriter& f64(double v)
    {
        return u64(std::bit_cast<uint64_t>(v));
    }

    template <typename T>
    ByteWriter& write_scalar(T v)
    {
        if constexpr (std::is_enum_v<T>)
        {
            using U = std::underlying_type_t<T>;
            return write_scalar<U>(static_cast<U>(v));
        }
        else if constexpr (std::is_integral_v<T>)
        {
            if constexpr (sizeof(T) == 1)
                return u8(uint8_t(v));
            else
            {
                push_be(v);
                return *this;
            }
        }
        else if constexpr (std::is_floating_point_v<T>)
        {
            if constexpr (sizeof(T) == 4)
                return f32(float(v));
            else if constexpr (sizeof(T) == 8)
                return f64(double(v));
        }
        else
        {
            static_assert(!sizeof(T*), "write_scalar: unsupported type");
        }
    }

    // ---------------- glm -----------------------------------------

    template <uint32_t Dim, typename T>
    ByteWriter& write_vector(const T& v)
    {
        for (uint32_t d = 0; d < Dim; ++d)
            write_scalar(v[d]);
        return *this;
    }

    ByteWriter& vec2(const glm::vec2& v)
    {
        return f32(v.x).f32(v.y);
    }

    ByteWriter& vec3(const glm::vec3& v)
    {
        return f32(v.x).f32(v.y).f32(v.z);
    }

    ByteWriter& vec4(const glm::vec4& v)
    {
        return f32(v.x).f32(v.y).f32(v.z).f32(v.w);
    }

    ByteWriter& ivec2(const glm::ivec2& v)
    {
        return i32(v.x).i32(v.y);
    }

    ByteWriter& ivec3(const glm::ivec3& v)
    {
        return i32(v.x).i32(v.y).i32(v.z);
    }

    ByteWriter& quat(const glm::quat& q)
    {
        return vec4(glm::vec4(q.x, q.y, q.z, q.w));
    }

    ByteWriter& mat4(const glm::mat4& m)
    {
        const float* p = glm::value_ptr(m);
        for (int i = 0; i < 16; ++i)
            f32(p[i]);
        return *this;
    }

    // ---------------- Strings / blobs -----------------------------

    ByteWriter& str(const std::string& s)
    {
        return var_u32(uint32_t(s.size()))
               .write(s.data(), s.size());
    }

    template <std::size_t N>
    ByteWriter& str(const static_string<N>& s)
    {
        return var_u32(uint32_t(s.size()))
               .write(s.data(), s.size());
    }

    ByteWriter& uuid(const UUID& id)
    {
        return write(id.data(), id.size());
    }

    ByteWriter& blob(std::span<const uint8_t> b)
    {
        return var_u32(uint32_t(b.size()))
               .write(b.data(), b.size());
    }

    // ---------------- Varints -------------------------------------

    ByteWriter& var_u32(uint32_t v)
    {
        while (v >= 0x80)
        {
            u8(uint8_t(v) | 0x80);
            v >>= 7;
        }
        u8(uint8_t(v));
        return *this;
    }

    ByteWriter& var_i32(int32_t v)
    {
        return var_u32((v << 1) ^ (v >> 31));
    }

    // ---------------- Bit packing ---------------------------------

    ByteWriter& bits(uint32_t value, uint8_t bitCount)
    {
        if (bitCount > 32)
            throw ByteError("bitCount > 32");

        bitBuffer |= uint64_t(value) << bitPos;
        bitPos += bitCount;
        flush_bits();
        return *this;
    }

    ByteWriter& flush_bits()
    {
        while (bitPos >= 8)
        {
            u8(uint8_t(bitBuffer));
            bitBuffer >>= 8;
            bitPos -= 8;
        }
        return *this;
    }

    ByteWriter& finalize_bits()
    {
        if (bitPos > 0)
        {
            u8(uint8_t(bitBuffer));
            bitBuffer = 0;
            bitPos = 0;
        }
        return *this;
    }

    // ---------------- TLV ------------------------------------------

    ByteWriter& tlv(uint16_t tag, const ByteWriter& payload)
    {
        return u16(tag)
               .var_u32(uint32_t(payload.size()))
               .write(payload.data(), payload.size());
    }

private:
    template <typename T>
    void push_be(T v)
    {
        for (int i = sizeof(T) - 1; i >= 0; --i)
            buf.push_back(uint8_t(v >> (i * 8)));
    }

    boost::container::small_vector<uint8_t, 128> buf;

    uint64_t bitBuffer = 0;
    uint8_t bitPos = 0;
};