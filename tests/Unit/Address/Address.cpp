
#include "atlasnet/core/Address.hpp"
#include "atlasnet/core/SocketAddress.hpp"
#include <gtest/gtest.h>
#include <unordered_set>

#include <functional>
#include <optional>
#include <string>
#include <unordered_set>
#include <variant>

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}


using namespace AtlasNet;

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------

// These helpers assume HostAddress exposes either:
//   - IsIPv4()/IsIPv6()/IsHostname()
//   - AsIPv4()/AsIPv6()/AsHostname()
// or similar.
// Adjust these in one place if your API differs.

static void ExpectHostAddressIsIPv4(const HostAddress& addr)
{
  EXPECT_TRUE(addr.IsIPv4());
  EXPECT_FALSE(addr.IsIPv6());
  EXPECT_FALSE(addr.IsHostName());
}

static void ExpectHostAddressIsIPv6(const HostAddress& addr)
{
  EXPECT_FALSE(addr.IsIPv4());
  EXPECT_TRUE(addr.IsIPv6());
  EXPECT_FALSE(addr.IsHostName());
}

static void ExpectHostAddressIsHostName(const HostAddress& addr)
{
  EXPECT_FALSE(addr.IsIPv4());
  EXPECT_FALSE(addr.IsIPv6());
  EXPECT_TRUE(addr.IsHostName());
}

// If you do not have string-deducing constructors, replace these with
// your actual parse helpers.
static HostAddress ParseHostAddress(const std::string& s)
{
  return HostAddress(s);
}

static SocketAddress ParseSocketAddress(const std::string& s)
{
  return SocketAddress(s);
}

// ------------------------------------------------------------
// IPv4 tests
// ------------------------------------------------------------

TEST(Address, IPv4_ConstructFromString)
{
  IPv4 addr("127.0.0.1");
  EXPECT_EQ(addr.to_string(), "127.0.0.1");
  EXPECT_EQ(addr[0], 127);
  EXPECT_EQ(addr[1], 0);
  EXPECT_EQ(addr[2], 0);
  EXPECT_EQ(addr[3], 1);
}

TEST(Address, IPv4_ConstructFromBytes)
{
  IPv4 addr(192, 168, 1, 42);
  EXPECT_EQ(addr.to_string(), "192.168.1.42");
  EXPECT_EQ(addr[0], 192);
  EXPECT_EQ(addr[1], 168);
  EXPECT_EQ(addr[2], 1);
  EXPECT_EQ(addr[3], 42);
}

TEST(Address, IPv4_ConstructFromPacked)
{
  IPv4 addr(0x7F000001u);
  EXPECT_EQ(addr.to_string(), "127.0.0.1");
  EXPECT_EQ(addr[0], 127);
  EXPECT_EQ(addr[1], 0);
  EXPECT_EQ(addr[2], 0);
  EXPECT_EQ(addr[3], 1);
}

TEST(Address, IPv4_RoundTripsString)
{
  const std::string s = "8.8.4.4";
  IPv4 addr(s);
  EXPECT_EQ(addr.to_string(), s);

  IPv4 addr2(addr.to_string());
  EXPECT_EQ(addr, addr2);
}

TEST(Address, IPv4_Equality)
{
  EXPECT_EQ(IPv4("127.0.0.1"), IPv4(127, 0, 0, 1));
  EXPECT_NE(IPv4("127.0.0.1"), IPv4("127.0.0.2"));
  EXPECT_NE(IPv4("127.0.0.1"), IPv4("128.0.0.1"));
}

TEST(Address, IPv4_HashEqualObjectsMatch)
{
  const IPv4 a("127.0.0.1");
  const IPv4 b(127, 0, 0, 1);
  const IPv4 c("127.0.0.2");

  EXPECT_EQ(std::hash<IPv4>{}(a), std::hash<IPv4>{}(b));
  EXPECT_EQ(a.hash(), b.hash());
  EXPECT_NE(a, c);
}

TEST(Address, IPv4_HashWorksInUnorderedSet)
{
  std::unordered_set<IPv4> set;
  set.insert(IPv4("127.0.0.1"));
  set.insert(IPv4("127.0.0.1"));
  set.insert(IPv4("127.0.0.2"));

  EXPECT_EQ(set.size(), 2u);
  EXPECT_TRUE(set.contains(IPv4("127.0.0.1")));
  EXPECT_TRUE(set.contains(IPv4("127.0.0.2")));
  EXPECT_FALSE(set.contains(IPv4("127.0.0.3")));
}

TEST(Address, IPv4_IndexOutOfRangeThrows)
{
  IPv4 addr("127.0.0.1");
  EXPECT_THROW((void)addr[4], std::out_of_range);
  EXPECT_THROW((void)addr[100], std::out_of_range);
}

TEST(Address, IPv4_Invalid_TooFewOctets)
{
  EXPECT_THROW(IPv4("127.0.0"), std::invalid_argument);
  EXPECT_THROW(IPv4("1.2.3"), std::invalid_argument);
}

TEST(Address, IPv4_Invalid_TooManyOctets)
{
  // Your current parser may accidentally accept these.
  // That is a bug worth catching with tests.
  EXPECT_THROW(IPv4("127.0.0.1.5"), std::invalid_argument);
  EXPECT_THROW(IPv4("1.2.3.4.5"), std::invalid_argument);
}

TEST(Address, IPv4_Invalid_OctetOutOfRange)
{
  EXPECT_THROW(IPv4("256.0.0.1"), std::out_of_range);
  EXPECT_THROW(IPv4("127.999.0.1"), std::out_of_range);
}

TEST(Address, IPv4_Invalid_NonNumeric)
{
  EXPECT_THROW(IPv4("abc.def.ghi.jkl"), std::invalid_argument);
  EXPECT_THROW(IPv4("127.0.0.one"), std::invalid_argument);
  EXPECT_THROW(IPv4("127.0..1"), std::invalid_argument);
}

TEST(Address, IPv4_Invalid_Empty)
{
  EXPECT_THROW(IPv4(""), std::invalid_argument);
}

// ------------------------------------------------------------
// IPv6 tests
// ------------------------------------------------------------

TEST(Address, IPv6_ConstructFromStringFullForm)
{
  IPv6 addr("2001:0db8:0000:0000:0000:ff00:0042:8329");

  EXPECT_EQ(addr.to_string(), "2001:0db8:0000:0000:0000:ff00:0042:8329");
  EXPECT_EQ(addr.size(), 16u);
}

TEST(Address, IPv6_ConstructFromSegments)
{
  IPv6 addr(0x2001, 0x0db8, 0x0000, 0x0000,
            0x0000, 0xff00, 0x0042, 0x8329);

  EXPECT_EQ(addr.to_string(), "2001:0db8:0000:0000:0000:ff00:0042:8329");
}

TEST(Address, IPv6_RoundTripsString)
{
  const std::string s = "ffff:0000:1111:2222:3333:4444:5555:6666";
  IPv6 addr(s);
  EXPECT_EQ(addr.to_string(), s);

  IPv6 addr2(addr.to_string());
  EXPECT_EQ(addr, addr2);
}

TEST(Address, IPv6_Equality)
{
  EXPECT_EQ(
      IPv6("2001:0db8:0000:0000:0000:ff00:0042:8329"),
      IPv6(0x2001, 0x0db8, 0x0000, 0x0000,
           0x0000, 0xff00, 0x0042, 0x8329));

  EXPECT_NE(
      IPv6("2001:0db8:0000:0000:0000:ff00:0042:8329"),
      IPv6("2001:0db8:0000:0000:0000:ff00:0042:8330"));
}

TEST(Address, IPv6_HashEqualObjectsMatch)
{
  const IPv6 a("2001:0db8:0000:0000:0000:ff00:0042:8329");
  const IPv6 b(0x2001, 0x0db8, 0x0000, 0x0000,
               0x0000, 0xff00, 0x0042, 0x8329);

  EXPECT_EQ(std::hash<IPv6>{}(a), std::hash<IPv6>{}(b));
  EXPECT_EQ(a.hash(), b.hash());
}

TEST(Address, IPv6_HashWorksInUnorderedSet)
{
  std::unordered_set<IPv6> set;
  set.insert(IPv6("2001:0db8:0000:0000:0000:ff00:0042:8329"));
  set.insert(IPv6("2001:0db8:0000:0000:0000:ff00:0042:8329"));
  set.insert(IPv6("ffff:0000:1111:2222:3333:4444:5555:6666"));

  EXPECT_EQ(set.size(), 2u);
}

TEST(Address, IPv6_IndexOutOfRangeThrows)
{
  IPv6 addr("2001:0db8:0000:0000:0000:ff00:0042:8329");
  EXPECT_THROW((void)addr[16], std::out_of_range);
  EXPECT_THROW((void)addr[100], std::out_of_range);
}

TEST(Address, IPv6_Invalid_TooFewSegments)
{
  EXPECT_THROW(IPv6("2001:0db8:0000:0000:0000:ff00:0042"), std::invalid_argument);
}

TEST(Address, IPv6_Invalid_TooManySegments)
{
  EXPECT_THROW(
      IPv6("2001:0db8:0000:0000:0000:ff00:0042:8329:1234"),
      std::invalid_argument);
}

TEST(Address, IPv6_Invalid_BadHex)
{
  EXPECT_THROW(
      IPv6("2001:0db8:0000:0000:0000:zzzz:0042:8329"),
      std::invalid_argument);
}

TEST(Address, IPv6_Invalid_SegmentOutOfRange)
{
  EXPECT_THROW(
      IPv6("2001:0db8:0000:0000:0000:10000:0042:8329"),
      std::invalid_argument);
}

TEST(Address, IPv6_Invalid_EmptySegment)
{
  // Your current parser does not support "::" compression.
  // This should currently be invalid unless you later add compressed IPv6 parsing.
  EXPECT_THROW(IPv6("2001:db8::1"), std::invalid_argument);
  EXPECT_THROW(IPv6("::1"), std::invalid_argument);
}

// ------------------------------------------------------------
// Hostname tests
// ------------------------------------------------------------

TEST(Address, HostName_ConstructFromString)
{
  HostName host("example.com");
  EXPECT_EQ(host.to_string(), "example.com");
}

TEST(Address, HostName_Equality)
{
  EXPECT_EQ(HostName("example.com"), HostName("example.com"));
  EXPECT_NE(HostName("example.com"), HostName("example.org"));
}

TEST(Address, HostName_HashEqualObjectsMatch)
{
  const HostName a("example.com");
  const HostName b("example.com");
  const HostName c("example.org");

  EXPECT_EQ(std::hash<HostName>{}(a), std::hash<HostName>{}(b));
  EXPECT_EQ(a.hash(), b.hash());
  EXPECT_NE(a, c);
}

TEST(Address, HostName_HashWorksInUnorderedSet)
{
  std::unordered_set<HostName> set;
  set.insert(HostName("example.com"));
  set.insert(HostName("example.com"));
  set.insert(HostName("localhost"));

  EXPECT_EQ(set.size(), 2u);
  EXPECT_TRUE(set.contains(HostName("example.com")));
  EXPECT_TRUE(set.contains(HostName("localhost")));
}

TEST(Address, HostName_ValidSimpleNames)
{
  EXPECT_NO_THROW(HostName("localhost"));
  EXPECT_NO_THROW(HostName("example.com"));
  EXPECT_NO_THROW(HostName("sub.domain.example"));
  EXPECT_NO_THROW(HostName("a-b.example"));
  EXPECT_NO_THROW(HostName("example.com."));
}

TEST(Address, HostName_InvalidNames)
{
  EXPECT_THROW(HostName(""), std::invalid_argument);
  EXPECT_THROW(HostName("-example.com"), std::invalid_argument);
  EXPECT_THROW(HostName("example-.com"), std::invalid_argument);
  EXPECT_THROW(HostName("exa_mple.com"), std::invalid_argument);
  EXPECT_THROW(HostName(".example.com"), std::invalid_argument);
  EXPECT_THROW(HostName("example..com"), std::invalid_argument);
  EXPECT_THROW(HostName("-"), std::invalid_argument);
  EXPECT_THROW(HostName("."), std::invalid_argument);
}

TEST(Address, HostName_InvalidLabelTooLong)
{
  std::string label(64, 'a');
  EXPECT_THROW(HostName(label + ".com"), std::invalid_argument);
}

TEST(Address, HostName_InvalidWholeNameTooLong)
{
  std::string too_long;
  while (too_long.size() <= 253)
  {
    if (!too_long.empty())
      too_long += ".";
    too_long += std::string(63, 'a');
  }

  EXPECT_ANY_THROW((HostName(too_long)));
}

// ------------------------------------------------------------
// HostName resolution tests
// ------------------------------------------------------------

TEST(Address, HostName_ResolveLocalhost)
{
  HostName host("localhost");
  auto resolved = host.resolve();

  ASSERT_TRUE(resolved.has_value());

  EXPECT_TRUE(std::holds_alternative<IPv4>(*resolved) ||
              std::holds_alternative<IPv6>(*resolved));
}

TEST(Address, HostName_ResolveInvalidFails)
{
  // Reserved TLD that should not resolve in normal environments.
  HostName host("definitely-does-not-exist.invalid");
  auto resolved = host.resolve();

  EXPECT_FALSE(resolved.has_value());
}

TEST(Address, HostName_ResolveCacheStable)
{
  HostName host("localhost");

  auto a = host.resolve();
  auto b = host.resolve();

  ASSERT_EQ(a.has_value(), b.has_value());

  if (a && b)
  {
    EXPECT_EQ(a->index(), b->index());

    if (std::holds_alternative<IPv4>(*a))
      EXPECT_EQ(std::get<IPv4>(*a), std::get<IPv4>(*b));
    else
      EXPECT_EQ(std::get<IPv6>(*a), std::get<IPv6>(*b));
  }
}

// ------------------------------------------------------------
// HostAddress deduction / parsing tests
// ------------------------------------------------------------

TEST(Address, HostAddress_DeducesIPv4)
{
  HostAddress addr = ParseHostAddress("127.0.0.1");
  ExpectHostAddressIsIPv4(addr);
  EXPECT_EQ(addr.to_string(), "127.0.0.1");
}

TEST(Address, HostAddress_DeducesIPv6)
{
  HostAddress addr = ParseHostAddress("2001:0db8:0000:0000:0000:ff00:0042:8329");
  ExpectHostAddressIsIPv6(addr);
  EXPECT_EQ(addr.to_string(), "2001:0db8:0000:0000:0000:ff00:0042:8329");
}

TEST(Address, HostAddress_DeducesHostname)
{
  HostAddress addr = ParseHostAddress("example.com");
  ExpectHostAddressIsHostName(addr);
  EXPECT_EQ(addr.to_string(), "example.com");
}

TEST(Address, HostAddress_RoundTripsIPv4)
{
  HostAddress a = ParseHostAddress("192.168.1.10");
  HostAddress b(a.to_string());
  EXPECT_EQ(a, b);
}

TEST(Address, HostAddress_RoundTripsIPv6)
{
  HostAddress a = ParseHostAddress("2001:0db8:0000:0000:0000:ff00:0042:8329");
  HostAddress b(a.to_string());
  EXPECT_EQ(a, b);
}

TEST(Address, HostAddress_RoundTripsHostname)
{
  HostAddress a = ParseHostAddress("localhost");
  HostAddress b(a.to_string());
  EXPECT_EQ(a, b);
}

TEST(Address, HostAddress_HashEqualObjectsMatch)
{
  HostAddress a = ParseHostAddress("127.0.0.1");
  HostAddress b = ParseHostAddress("127.0.0.1");
  HostAddress c = ParseHostAddress("127.0.0.2");

  EXPECT_EQ(std::hash<HostAddress>{}(a), std::hash<HostAddress>{}(b));
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
}

TEST(Address, HostAddress_InvalidStringThrows)
{
  EXPECT_THROW(ParseHostAddress(""), std::invalid_argument);
  EXPECT_THROW(ParseHostAddress("not a valid host name with spaces"), std::invalid_argument);
  EXPECT_THROW(ParseHostAddress("127.0.0.1.5"), std::invalid_argument);
}

// ------------------------------------------------------------
// SocketAddress parsing / formatting tests
// ------------------------------------------------------------

TEST(Address, SocketAddress_ConstructIPv4AndPort)
{
  SocketAddress addr(IPv4("127.0.0.1"), 8080);
  EXPECT_TRUE(addr.IsIPv4());
  EXPECT_EQ(addr.get_ipv4().to_string(), "127.0.0.1");
  EXPECT_EQ(addr.get_port(), 8080);
  EXPECT_EQ(addr.to_string(), "127.0.0.1:8080");
}

TEST(Address, SocketAddress_ConstructHostNameAndPort)
{
  SocketAddress addr(HostName("localhost"), 27015);
  EXPECT_TRUE(addr.IsHostName());
  EXPECT_EQ(addr.get_hostname().to_string(), "localhost");
  EXPECT_EQ(addr.get_port(), 27015);
  EXPECT_EQ(addr.to_string(), "localhost:27015");
}

TEST(Address, SocketAddress_ConstructIPv6AndPort)
{
  SocketAddress addr(
      IPv6("2001:0db8:0000:0000:0000:ff00:0042:8329"),
      443);

  EXPECT_EQ(addr.get_port(), 443);
  EXPECT_TRUE(addr.IsIPv6());

  // Standard formatting should bracket IPv6 when combined with port.
  EXPECT_EQ(addr.to_string(),
            "[2001:0db8:0000:0000:0000:ff00:0042:8329]:443");
}

TEST(Address, SocketAddress_ParseIPv4AndPort)
{
  SocketAddress addr = ParseSocketAddress("127.0.0.1:8080");
  EXPECT_TRUE(addr.IsIPv4());
  EXPECT_EQ(addr.get_port(), 8080);
  EXPECT_EQ(addr.get_ipv4().to_string(), "127.0.0.1");
  EXPECT_EQ(addr.to_string(), "127.0.0.1:8080");
}

TEST(Address, SocketAddress_ParseHostNameAndPort)
{
  SocketAddress addr = ParseSocketAddress("example.com:53");

  EXPECT_EQ(addr.get_port(), 53);
  EXPECT_EQ(addr.get_hostname().to_string(), "example.com");
  EXPECT_EQ(addr.to_string(), "example.com:53");
}

TEST(Address, SocketAddress_ParseIPv6AndPort)
{
  SocketAddress addr =
      ParseSocketAddress("[2001:0db8:0000:0000:0000:ff00:0042:8329]:443");

  EXPECT_EQ(addr.get_port(), 443);
  EXPECT_EQ(addr.get_ipv6().to_string(),
            "2001:0db8:0000:0000:0000:ff00:0042:8329");
  EXPECT_EQ(addr.to_string(),
            "[2001:0db8:0000:0000:0000:ff00:0042:8329]:443");
}

TEST(Address, SocketAddress_Equality)
{
  EXPECT_EQ(
      SocketAddress(IPv4("127.0.0.1"), 80),
      SocketAddress(IPv4("127.0.0.1"), 80));

  EXPECT_NE(
      SocketAddress(IPv4("127.0.0.1"), 80),
      SocketAddress(IPv4("127.0.0.1"), 81));

  EXPECT_NE(
      SocketAddress(IPv4("127.0.0.1"), 80),
      SocketAddress(IPv4("127.0.0.2"), 80));
}

TEST(Address, SocketAddress_HashEqualObjectsMatch)
{
  const SocketAddress a(IPv4("127.0.0.1"), 8080);
  const SocketAddress b(IPv4("127.0.0.1"), 8080);
  const SocketAddress c(IPv4("127.0.0.1"), 8081);

  EXPECT_EQ(std::hash<SocketAddress>{}(a), std::hash<SocketAddress>{}(b));
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
}

TEST(Address, SocketAddress_HashWorksInUnorderedSet)
{
  std::unordered_set<SocketAddress> set;
  set.insert(SocketAddress(IPv4("127.0.0.1"), 80));
  set.insert(SocketAddress(IPv4("127.0.0.1"), 80));
  set.insert(SocketAddress(IPv4("127.0.0.1"), 81));

  EXPECT_EQ(set.size(), 2u);
}

TEST(Address, SocketAddress_InvalidStringsThrow)
{
  EXPECT_THROW(ParseSocketAddress(""), std::invalid_argument);
  EXPECT_THROW(ParseSocketAddress("127.0.0.1"), std::invalid_argument);
  EXPECT_THROW(ParseSocketAddress("127.0.0.1:"), std::invalid_argument);
  EXPECT_THROW(ParseSocketAddress(":8080"), std::invalid_argument);
  EXPECT_THROW(ParseSocketAddress("127.0.0.1:99999"), std::invalid_argument);
  EXPECT_THROW(ParseSocketAddress("[2001:db8::1]"), std::invalid_argument);
  EXPECT_THROW(ParseSocketAddress("[2001:db8::1]:abc"), std::invalid_argument);
}

// ------------------------------------------------------------
// Invalid-address classification tests
// ------------------------------------------------------------

TEST(Address, Deduction_PrefersIPv4WhenStringIsIPv4)
{
  HostAddress addr = ParseHostAddress("8.8.8.8");
  ExpectHostAddressIsIPv4(addr);
}

TEST(Address, Deduction_PrefersIPv6WhenStringIsIPv6)
{
  HostAddress addr = ParseHostAddress("ffff:0000:1111:2222:3333:4444:5555:6666");
  ExpectHostAddressIsIPv6(addr);
}

TEST(Address, Deduction_FallsBackToHostnameWhenStringIsHostname)
{
  HostAddress addr = ParseHostAddress("redis.internal");
  ExpectHostAddressIsHostName(addr);
}

TEST(Address, Deduction_InvalidAddressRejected)
{
  EXPECT_ANY_THROW(ParseHostAddress("300.1.1.1"));
  EXPECT_ANY_THROW(ParseHostAddress("2001:::1"));
  EXPECT_ANY_THROW(ParseHostAddress("bad host name"));
  EXPECT_ANY_THROW(ParseHostAddress("[]"));
}

// ------------------------------------------------------------
// Cross-type inequality sanity tests
// ------------------------------------------------------------

TEST(Address, DifferentAddressKindsAreNotEqual)
{
  HostAddress ipv4 = ParseHostAddress("127.0.0.1");
  HostAddress ipv6 = ParseHostAddress("0000:0000:0000:0000:0000:0000:0000:0001");
  HostAddress host = ParseHostAddress("localhost");

  EXPECT_NE(ipv4, ipv6);
  EXPECT_NE(ipv4, host);
  EXPECT_NE(ipv6, host);
}