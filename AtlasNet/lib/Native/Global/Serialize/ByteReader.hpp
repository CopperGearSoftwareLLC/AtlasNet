#pragma once
#include <bit>
#include <boost/beast/core/detail/base64.hpp>
#include <iostream>
#include <span>

#include "ByteStream.hpp"
#include "Global/Misc/String_utils.hpp"
#include "Global/Misc/UUID.hpp"
#include "Global/pch.hpp"
class ByteReader
{
   private:
   public:
	ByteReader(std::span<const uint8_t> data, bool IsEncodedBase64 = false)
		: p(data.data()), n(data.size())
	{
		if (IsEncodedBase64)
			DecodeBase64();
	}
	explicit ByteReader(const std::string& s, bool IsEncodedBase64 = false)
		: p(reinterpret_cast<const uint8_t*>(s.data())), n(s.size())
	{
		if (IsEncodedBase64)
			DecodeBase64();
	}
	void DecodeBase64()
	{
		size_t real_len = std::strlen(reinterpret_cast<const char*>(p));
		std::size_t decoded_size = boost::beast::detail::base64::decoded_size(real_len);
		DecodedBase64Data.resize(decoded_size);

		const auto result = boost::beast::detail::base64::decode(
			DecodedBase64Data.data(), reinterpret_cast<const char*>(p), real_len);
		DecodedBase64Data.resize(result.first);
		p = DecodedBase64Data.data();
		n = DecodedBase64Data.size();
	}

	size_t remaining() const { return n - i; }
	size_t position() const { return i; }

	// ---------------- Raw -----------------------------------------

	void read(void* out, size_t bytes)
	{
		if (i + bytes > n)
			throw ByteError("read overflow");
		std::memcpy(out, p + i, bytes);
		i += bytes;
	}

	// ---------------- Integers ------------------------------------

	uint8_t u8() { return read_int<uint8_t>(); }
	uint16_t u16() { return read_be<uint16_t>(); }
	uint32_t u32() { return read_be<uint32_t>(); }
	uint64_t u64() { return read_be<uint64_t>(); }

	int32_t i8() { return read_be<int8_t>(); }
	int32_t i16() { return read_be<int16_t>(); }
	int32_t i32() { return read_be<int32_t>(); }
	int64_t i64() { return read_be<int64_t>(); }

	// ---------------- Floats --------------------------------------

	float f32() { return std::bit_cast<float>(u32()); }
	double f64() { return std::bit_cast<double>(u64()); }
	// ---------------- Generic Scalar -----------------------------
	template <typename T>
	T read_scalar()
	{
		if constexpr (std::is_enum_v<T>)
		{
			using U = std::underlying_type_t<T>;
			return static_cast<T>(read_scalar<U>());
		}
		else if constexpr (std::is_integral_v<T>)
		{
			if constexpr (sizeof(T) == 1)
				return read_int<T>();
			else
				return read_be<T>();
		}
		else if constexpr (std::is_floating_point_v<T>)
		{
			if constexpr (sizeof(T) == 4)
				return f32();
			else if constexpr (sizeof(T) == 8)
				return f64();
		}
		else
		{
			static_assert(!sizeof(T*), "read_scalar: unsupported type");
		}
	}

	// ---------------- Generic Vector -----------------------------
	template <uint32_t Dim, typename T>
	T read_vector()
	{
		T out;
		for (uint32_t d = 0; d < Dim; ++d) out[d] = read_scalar<typename T::value_type>();
		return out;
	}
	// ---------------- glm -----------------------------------------

	// glm::half half() {
	//   return glm::unpackHalf1x16(u16());
	// }

	glm::vec2 vec2() { return {f32(), f32()}; }
	glm::vec3 vec3() { return {f32(), f32(), f32()}; }
	glm::vec4 vec4() { return {f32(), f32(), f32(), f32()}; }

	glm::ivec2 ivec2() { return {i32(), i32()}; }
	glm::ivec3 ivec3() { return {i32(), i32(), i32()}; }

	glm::quat quat()
	{
		auto v = vec4();
		return glm::quat(v.w, v.x, v.y, v.z);
	}

	glm::mat4 mat4()
	{
		glm::mat4 m;
		float* p = glm::value_ptr(m);
		for (int i = 0; i < 16; ++i) p[i] = f32();
		return m;
	}

	// ---------------- Strings / blobs -----------------------------

	std::string str()
	{
		uint32_t len = var_u32();
		if (remaining() < len)
			throw ByteError("string overflow");
		std::string s(reinterpret_cast<const char*>(p + i), len);
		i += len;
		return s;
	}
	UUID uuid()
	{
		if (remaining() < 16)
			throw ByteError("UUID overflow");
		UUID id;
		read(id.data(), id.size());
		return id;
	}
	template <std::size_t N>
	static_string<N> str()
	{
		uint32_t len = var_u32();
		if (remaining() < len)
			throw ByteError("string overflow");
		static_string<N> s(reinterpret_cast<const char*>(p + i), len);
		i += len;
		return s;
	}
	std::span<const uint8_t> blob()
	{
		uint32_t len = var_u32();
		if (remaining() < len)
			throw ByteError("blob overflow");
		auto out = std::span<const uint8_t>(p + i, len);
		i += len;
		return out;
	}

	// ---------------- Varints -------------------------------------

	uint32_t var_u32()
	{
		uint32_t v = 0;
		int shift = 0;
		for (;;)
		{
			uint8_t b = u8();
			v |= uint32_t(b & 0x7F) << shift;
			if (!(b & 0x80))
				break;
			shift += 7;
			if (shift > 35)
				throw ByteError("varint overflow");
		}
		return v;
	}

	int32_t var_i32()
	{
		uint32_t v = var_u32();
		return (v >> 1) ^ -(int32_t(v & 1));
	}

	// ---------------- Bit unpacking -------------------------------

	uint32_t bits(uint8_t bitCount)
	{
		if (bitCount > 32)
			throw ByteError("bitCount > 32");
		while (bitPos < bitCount)
		{
			bitBuffer |= uint64_t(u8()) << bitPos;
			bitPos += 8;
		}
		uint32_t out = uint32_t(bitBuffer & ((1ull << bitCount) - 1));
		bitBuffer >>= bitCount;
		bitPos -= bitCount;
		return out;
	}

	void align_bits()
	{
		bitBuffer = 0;
		bitPos = 0;
	}

	// ---------------- TLV ------------------------------------------

	bool next_tlv(uint16_t& tag, std::span<const uint8_t>& value)
	{
		if (remaining() == 0)
			return false;
		tag = u16();
		uint32_t len = var_u32();
		if (remaining() < len)
			throw ByteError("TLV overflow");
		value = {p + i, len};
		i += len;
		return true;
	}

	std::span<const uint8_t> GetReadSource() const { return {p, n}; }

   private:
	template <typename T>
	T read_int()
	{
		if (i + sizeof(T) > n)
			throw ByteError("read overflow");
		T v;
		std::memcpy(&v, p + i, sizeof(T));
		i += sizeof(T);
		return v;
	}

	template <typename T>
	T read_be()
	{
		if (i + sizeof(T) > n)
			throw ByteError("read overflow");
		T v = 0;
		for (size_t j = 0; j < sizeof(T); ++j) v = (v << 8) | p[i++];
		return v;
	}

	std::vector<uint8_t> DecodedBase64Data;
	const uint8_t* p;
	size_t n;
	size_t i = 0;

	uint64_t bitBuffer = 0;
	uint8_t bitPos = 0;
};
