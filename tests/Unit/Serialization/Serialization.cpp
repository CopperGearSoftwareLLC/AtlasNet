
#include "atlasnet/core/serialize/ByteReader.hpp"
#include "atlasnet/core/serialize/ByteWriter.hpp"
#include "glm/ext/quaternion_relational.hpp"
#include "glm/fwd.hpp"
#include "glm/gtc/epsilon.hpp"

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <string>

TEST(Serialization, BasicOps) {
  const uint8_t u8_val = 255;
  const uint16_t u16_val = 65535;
  const uint32_t u32_val = 4294967295;
  const uint64_t u64_val = 18446744073709551615ull;
  const int8_t i8_val = -128;
  const int16_t i16_val = -32768;
  const int32_t i32_val = -2147483648;
  const int64_t i64_val = -9223372036854775807ll - 1;
  const float f32_val = 3.14f;
  const double f64_val = 3.141592653589793;
  const std::string str_val = "Hello, AtlasNet!";
  const glm::vec4 vec4_val(1.0f, 2.0f, 3.0f, 4.0f);
  const uint32_t bits_val = 0b10101010101010101010101010101010;
  const glm::quat quat_val(1.0f, 0.0f, 0.0f, 0.0f);
  const glm::mat4 mat4_val(1.0f);
  const std::vector<uint8_t> blob_val = {0xDE, 0xAD, 0xBE, 0xEF};

  AtlasNet::ByteWriter writer;
  writer.u8(u8_val)
      .u16(u16_val)
      .u32(u32_val)
      .u64(u64_val)
      .i8(i8_val)
      .i16(i16_val)
      .i32(i32_val)
      .i64(i64_val)
      .f32(f32_val)
      .f64(f64_val)
      .str(str_val)
      .write_vector<4>(vec4_val)
      .bits(bits_val, 32)
      .quat(quat_val)
      .mat4(mat4_val)
      .blob(std::span(blob_val));

  uint8_t u8_out;
  uint16_t u16_out;
  uint32_t u32_out;
  uint64_t u64_out;
  int8_t i8_out;
  int16_t i16_out;
  int32_t i32_out;
  int64_t i64_out;
  float f32_out;
  double f64_out;
  std::string str_out;
  glm::vec4 vec4_out;
  uint32_t bits_out;
  glm::quat quat_out;
  glm::mat4 mat4_out;
  std::span<const uint8_t> blob_out;

  AtlasNet::ByteReader reader(writer.bytes());
  reader.u8(u8_out)
      .u16(u16_out)
      .u32(u32_out)
      .u64(u64_out)
      .i8(i8_out)
      .i16(i16_out)
      .i32(i32_out)
      .i64(i64_out)
      .f32(f32_out)
      .f64(f64_out)
      .str(str_out)
      .read_vector<4>(vec4_out)
      .bits(32, bits_out)
      .quat(quat_out)
      .mat4(mat4_out)
      .blob(blob_out);

  EXPECT_EQ(u8_out, u8_val);
  EXPECT_EQ(u16_out, u16_val);
  EXPECT_EQ(u32_out, u32_val);
  EXPECT_EQ(u64_out, u64_val);
  EXPECT_EQ(i8_out, i8_val);
  EXPECT_EQ(i16_out, i16_val);
  EXPECT_EQ(i32_out, i32_val);
  EXPECT_EQ(i64_out, i64_val);
  EXPECT_FLOAT_EQ(f32_out, f32_val);
  EXPECT_DOUBLE_EQ(f64_out, f64_val);
  EXPECT_EQ(str_out, str_val);
  EXPECT_TRUE(glm::all(glm::epsilonEqual(vec4_out, vec4_val, 0.0001f)));
  EXPECT_EQ(bits_out, bits_val);
  EXPECT_TRUE(glm::all(glm::epsilonEqual(quat_out, quat_val, 0.0001f)));
  EXPECT_EQ(mat4_out, mat4_val);
  EXPECT_TRUE(std::equal(blob_out.begin(), blob_out.end(), blob_val.begin(),
                         blob_val.end()));
}
struct Object {
  uint8_t u8_val;
  uint16_t u16_val;
  uint32_t u32_val;
  uint64_t u64_val;
  int8_t i8_val;
  int16_t i16_val;
  int32_t i32_val;
  int64_t i64_val;
  float f32_val;
  double f64_val;
  std::string str_val;
  glm::vec4 vec4_val;
  uint32_t bits_val;
  glm::quat quat_val;
  glm::mat4 mat4_val;
  std::span<const uint8_t> blob_val;

  template <typename Archive> void serialize(Archive &ar) {
    ar(u8_val);
    ar(u16_val);
    ar(u32_val);
    ar(u64_val);
    ar(i8_val);
    ar(i16_val);
    ar(i32_val);
    ar(i64_val);
    ar(f32_val);
    ar(f64_val);
    ar(str_val);
    ar(vec4_val);
    ar(bits_val);
    ar(quat_val);
    ar(mat4_val);
    ar(blob_val);
  }
};
TEST(Serialization, archivetest) {

    std::vector<uint8_t> blob = {0xDE, 0xAD, 0xBE, 0xEF};
  Object in{
      255,
      65535,
      4294967295,
      18446744073709551615ull,
      -128,
      -32768,
      -2147483648,
      -9223372036854775807ll - 1,
      3.14f,
      3.141592653589793,
      "Hello, AtlasNet!",
      glm::vec4(1.0f, 2.0f, 3.0f, 4.0f),
      0b10101010101010101010101010101010,
      glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
      glm::mat4(1.0f),
      blob,
  };

  AtlasNet::ByteWriter writer;
  in.serialize(writer);

  Object out;
  AtlasNet::ByteReader reader(writer.bytes());
  out.serialize(reader);
  EXPECT_EQ(out.u8_val, in.u8_val);
  EXPECT_EQ(out.u16_val, in.u16_val);
  EXPECT_EQ(out.u32_val, in.u32_val);
  EXPECT_EQ(out.u64_val, in.u64_val);
  EXPECT_EQ(out.i8_val, in.i8_val);
  EXPECT_EQ(out.i16_val, in.i16_val);
  EXPECT_EQ(out.i32_val, in.i32_val);
  EXPECT_EQ(out.i64_val, in.i64_val);
  EXPECT_FLOAT_EQ(out.f32_val, in.f32_val);
  EXPECT_DOUBLE_EQ(out.f64_val, in.f64_val);
  EXPECT_EQ(out.str_val, in.str_val);
  EXPECT_TRUE(glm::all(glm::epsilonEqual(out.vec4_val, in.vec4_val, 0.0001f)));

  EXPECT_EQ(out.bits_val, in.bits_val);
  EXPECT_TRUE(glm::all(glm::epsilonEqual(out.quat_val, in.quat_val, 0.0001f)));
  EXPECT_EQ(out.mat4_val, in.mat4_val);
  EXPECT_TRUE(std::equal(out.blob_val.begin(), out.blob_val.end(),
                         in.blob_val.begin(), in.blob_val.end()));
}
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}