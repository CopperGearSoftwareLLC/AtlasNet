#pragma once
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <compare>
#include <string>
#include <string_view>
class UUID
{
  boost::uuids::uuid id;

public:
  UUID() : id() {}

  explicit UUID(const boost::uuids::uuid& uuid) : id(uuid) {}
  explicit UUID(const std::string_view uuid_str)
      : id(boost::uuids::string_generator()(uuid_str.data()))
  {
  }

  std::strong_ordering operator<=>(const UUID& other) const
  {
    return std::lexicographical_compare_three_way(
        std::begin(id.data), std::end(id.data), std::begin(other.id.data),
        std::end(other.id.data));
  }
  std::string to_string() const
  {
    return boost::uuids::to_string(id);
  }

  static UUID Generate()
  {
    static boost::uuids::random_generator generator;
    return UUID(generator());
  }

  uint8_t operator[](size_t index) const
  {
    return id.data[index];
  }

  void* data()
  {
    return id.data;
  }
  const void* data() const
  {
    return id.data;
  }
  size_t size() const
  {
    return sizeof(id.data);
  }
};

template <typename Tag> struct StrongUUID : public UUID
{

  StrongUUID() = default;
  explicit StrongUUID(UUID v) : UUID(std::move(v)) {}

  friend bool operator==(const StrongUUID&, const StrongUUID&) = default;
};