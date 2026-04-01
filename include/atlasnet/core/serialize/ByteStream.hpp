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
namespace AtlasNet
{
template <typename T>
concept Iterable =
    std::ranges::range<T> &&
    !std::is_same_v<std::remove_cvref_t<T>, std::string> &&
    !std::is_same_v<std::remove_cvref_t<T>, std::span<const uint8_t>> &&
    !std::is_same_v<std::remove_cvref_t<T>, std::span<uint8_t>>;

template <typename T>
concept SizedIterable = Iterable<T> && requires(const T& t) {
  { t.size() } -> std::convertible_to<std::size_t>;
};
template <typename T>
concept ResizableIterable =
    Iterable<T> && requires(T t, std::size_t n) { t.resize(n); };
static const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
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

template <typename T, typename = void> struct is_glm_vec : std::false_type
{
};

template <typename T>
struct is_glm_vec<T, std::void_t<typename T::value_type,
                                 decltype(std::declval<T>().length())>>
    : std::true_type
{
};

} // namespace AtlasNet
