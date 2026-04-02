
#include "atlasnet/core/Address.hpp"
#include <gtest/gtest.h>

TEST(ADDRESS, IPV4)
{
  IPv4Address addr("127.0.0.1");
  EXPECT_EQ(addr.to_string(), "127.0.0.1");
  EXPECT_EQ(addr[0], 127);
  EXPECT_EQ(addr[1], 0);
  EXPECT_EQ(addr[2], 0);
  EXPECT_EQ(addr[3], 1);
}
TEST(ADDRESS, IPV6)
{
  IPv6Address addr("2001:0db8:85a3:0000:0000:8a2e:0370:7334");
  EXPECT_EQ(addr.to_string(), "2001:0db8:85a3:0000:0000:8a2e:0370:7334");
  EXPECT_EQ(addr[0], 0x20);
  EXPECT_EQ(addr[1], 0x01);
  EXPECT_EQ(addr[2], 0x0d);
  EXPECT_EQ(addr[3], 0xb8);
  EXPECT_EQ(addr[4], 0x85);
  EXPECT_EQ(addr[5], 0xa3);
  EXPECT_EQ(addr[6], 0x00);
  EXPECT_EQ(addr[7], 0x00);
  EXPECT_EQ(addr[8], 0x00);
  EXPECT_EQ(addr[9], 0x00);
  EXPECT_EQ(addr[10], 0x8a);
  EXPECT_EQ(addr[11], 0x2e);
  EXPECT_EQ(addr[12], 0x03);
  EXPECT_EQ(addr[13], 0x70);
  EXPECT_EQ(addr[14], 0x73);
  EXPECT_EQ(addr[15], 0x34);
}
TEST(ADDRESS, DNS)
{
  DNSAddress addr("localhost");
  EXPECT_EQ(addr.to_string(), "localhost");
}
TEST(ADDRESS, DNS_RESOLVE)
{
  DNSAddress addr("localhost");
  auto resolved = addr.resolve();
  EXPECT_TRUE(resolved.has_value());
  EXPECT_TRUE(std::holds_alternative<IPv4Address>(*resolved) ||
              std::holds_alternative<IPv6Address>(*resolved));

  if (std::holds_alternative<IPv4Address>(*resolved))
  {
    const IPv4Address& ipv4 = std::get<IPv4Address>(*resolved);
    const IPv4Address localHost(127, 0, 0, 1);
    EXPECT_EQ(ipv4, localHost);
  }
  else if (std::holds_alternative<IPv6Address>(*resolved))
  {
    const IPv6Address& ipv6 = std::get<IPv6Address>(*resolved);
    // We won't assert the exact address since it can vary, but we can check
    // that it's not all zeros

    const IPv6Address localHost(0, 0, 0, 0, 0, 0, 0, 1);
    EXPECT_EQ(ipv6, localHost);
  }
}


int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}