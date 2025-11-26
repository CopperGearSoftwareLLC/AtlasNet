#include "SecretGenerator.hpp"

static std::string to_hex(const unsigned char* data, size_t len) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) oss << std::setw(2) << (int)data[i];
    return oss.str();
}
std::string SecretGenerator::GenerateUniqueSecret() const
{

    using namespace std::chrono;
    // high-res time (ns)
    auto now = high_resolution_clock::now();
    uint64_t ns = static_cast<uint64_t>(duration_cast<nanoseconds>(now.time_since_epoch()).count());

    // some entropy from random_device
    std::random_device rd;
    uint64_t r1 = (static_cast<uint64_t>(rd()) << 32) ^ rd();

    // include thread id and pid for extra uniqueness
    auto tid_hash = std::hash<std::thread::id>{}(std::this_thread::get_id());
#ifdef __unix__
    auto pid = getpid();
#else
    auto pid = 0;
#endif

    std::ostringstream seed;
    seed << ns << '-' << r1 << '-' << tid_hash << '-' << pid;

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char *>(seed.str().data()), seed.str().size(), hash);

    return to_hex(hash, SHA256_DIGEST_LENGTH); // 64 hex chars
}
