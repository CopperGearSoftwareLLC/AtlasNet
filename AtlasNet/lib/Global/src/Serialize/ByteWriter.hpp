#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ByteStream.hpp"
#include "Misc/UUID.hpp"
#include "pch.hpp"
#include <span>
#include <bit>
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
		std::string_view value_view(reinterpret_cast<const char*>(data.data()), data.size());
		return value_view;
	}

	void clear() { buf.clear(); }

	// ---------------- Raw -----------------------------------------

	void write(const void* p, size_t n)
	{
		const uint8_t* b = static_cast<const uint8_t*>(p);
		buf.insert(buf.end(), b, b + n);
	}

	// ---------------- Integers (BE) -------------------------------

	void u8(uint8_t v) { buf.push_back(v); }
	void i8(int8_t v) { buf.push_back(uint8_t(v)); }

	void u16(uint16_t v) { push_be(v); }
	void u32(uint32_t v) { push_be(v); }
	void u64(uint64_t v) { push_be(v); }

	void i16(int16_t v) { u16(uint16_t(v)); }
	void i32(int32_t v) { u32(uint32_t(v)); }
	void i64(int64_t v) { u64(uint64_t(v)); }

	// ---------------- Floats --------------------------------------

	void f32(float v) { u32(std::bit_cast<uint32_t>(v)); }

	void f64(double v) { u64(std::bit_cast<uint64_t>(v)); }
	template <typename T>
	void write_scalar(T v)
	{
		if constexpr (std::is_enum_v<T>)
		{
			using U = std::underlying_type_t<T>;
			write_scalar<U>(static_cast<U>(v));
		}
		else if constexpr (std::is_integral_v<T>)
		{
			if constexpr (sizeof(T) == 1)
				buf.push_back(uint8_t(v));
			else
				push_be(v);
		}
		else if constexpr (std::is_floating_point_v<T>)
		{
			if constexpr (sizeof(T) == 4)
				f32(float(v));
			else if constexpr (sizeof(T) == 8)
				f64(double(v));
		}
		else
		{
			static_assert(!sizeof(T*), "write_scalar: unsupported type");
		}
	}

	// ---------------- glm -----------------------------------------

	// void half(glm::half h) {
	//   uint16_t bits = glm::packHalf1x16(h);
	//   u16(bits);
	// }
	template <uint32_t Dim, typename T>
	void write_vector(const T& v)
	{
		for (uint32_t d = 0; d < Dim; ++d) write_scalar(v[d]);
	}
	void vec2(const glm::vec2& v)
	{
		f32(v.x);
		f32(v.y);
	}
	void vec3(const glm::vec3& v)
	{
		f32(v.x);
		f32(v.y);
		f32(v.z);
	}
	void vec4(const glm::vec4& v)
	{
		f32(v.x);
		f32(v.y);
		f32(v.z);
		f32(v.w);
	}

	void ivec2(const glm::ivec2& v)
	{
		i32(v.x);
		i32(v.y);
	}
	void ivec3(const glm::ivec3& v)
	{
		i32(v.x);
		i32(v.y);
		i32(v.z);
	}

	void quat(const glm::quat& q) { vec4(glm::vec4(q.x, q.y, q.z, q.w)); }

	void mat4(const glm::mat4& m)
	{
		const float* p = glm::value_ptr(m);
		for (int i = 0; i < 16; ++i) f32(p[i]);
	}

	// ---------------- Strings / blobs -----------------------------

	void str(const std::string& s)
	{
		var_u32(uint32_t(s.size()));
		write(s.data(), s.size());
	}
	template <std::size_t N>
	void str(const static_string<N>& s)
	{
		var_u32(uint32_t(s.size()));
		write(s.data(), s.size());
	}
	 void uuid(const UUID& id) {
        write(id.data(), id.size());
    }
	void blob(std::span<const uint8_t> b)
	{
		var_u32(uint32_t(b.size()));
		write(b.data(), b.size());
	}

	// ---------------- Varints -------------------------------------

	void var_u32(uint32_t v)
	{
		while (v >= 0x80)
		{
			u8(uint8_t(v) | 0x80);
			v >>= 7;
		}
		u8(uint8_t(v));
	}

	void var_i32(int32_t v)
	{
		var_u32((v << 1) ^ (v >> 31));	// zigzag
	}

	// ---------------- Bit packing ---------------------------------

	void bits(uint32_t value, uint8_t bitCount)
	{
		if (bitCount > 32)
			throw ByteError("bitCount > 32");
		bitBuffer |= uint64_t(value) << bitPos;
		bitPos += bitCount;
		flush_bits();
	}

	void flush_bits()
	{
		while (bitPos >= 8)
		{
			u8(uint8_t(bitBuffer));
			bitBuffer >>= 8;
			bitPos -= 8;
		}
	}

	void finalize_bits()
	{
		if (bitPos > 0)
		{
			u8(uint8_t(bitBuffer));
			bitBuffer = 0;
			bitPos = 0;
		}
	}

	// ---------------- TLV ------------------------------------------

	void tlv(uint16_t tag, const ByteWriter& payload)
	{
		u16(tag);
		var_u32(uint32_t(payload.size()));
		write(payload.data(), payload.size());
	}

   private:
	template <typename T>
	void push_be(T v)
	{
		for (int i = sizeof(T) - 1; i >= 0; --i) buf.push_back(uint8_t(v >> (i * 8)));
	}

	std::vector<uint8_t> buf;

	uint64_t bitBuffer = 0;
	uint8_t bitPos = 0;
};
