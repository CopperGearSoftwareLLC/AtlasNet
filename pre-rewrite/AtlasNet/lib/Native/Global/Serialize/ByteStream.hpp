#pragma once
#include <bit>
#include <cstdint>
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/bitfield.hpp>
#include <glm/gtc/packing.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
static const std::string base64_chars =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789+/";

static inline bool is_base64(uint8_t c)
{
	return (isalnum(c) || (c == '+') || (c == '/'));
}
struct ByteError : std::runtime_error
{
	ByteError() : std::runtime_error("ByteError") {}
	using std::runtime_error::runtime_error;
};

