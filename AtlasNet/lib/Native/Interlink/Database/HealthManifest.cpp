#include "HealthManifest.hpp"

#include <cstdlib>

#include "Docker/DockerIO.hpp"
#include "Interlink/Database/ServerRegistry.hpp"
#include "Interlink/Telemetry/NetworkManifest.hpp"

namespace
{
std::optional<std::string> GetNonEmptyEnv(const char* name)
{
	if (const char* value = std::getenv(name); value != nullptr && *value != '\0')
	{
		return std::string(value);
	}
	return std::nullopt;
}

std::string ResolveInterlinkAdvertiseIp()
{
	if (const auto explicitIp = GetNonEmptyEnv("INTERLINK_ADVERTISE_IP"); explicitIp.has_value())
	{
		return *explicitIp;
	}
	if (const auto podIp = GetNonEmptyEnv("POD_IP"); podIp.has_value())
	{
		return *podIp;
	}
	return DockerIO::Get().GetSelfContainerIP();
}

IPAddress BuildInterlinkAddress(const std::string& ip, const PortType port)
{
	IPAddress address;
	address.Parse(ip + ":" + std::to_string(port));
	return address;
}

bool ShouldRefreshServerRegistry(const NetworkIdentity& identifier)
{
	switch (identifier.Type)
	{
		case NetworkIdentityType::eShard:
		case NetworkIdentityType::eWatchDog:
		case NetworkIdentityType::eCartograph:
		case NetworkIdentityType::eProxy:
			return true;
		default:
			return false;
	}
}

void RefreshServerRegistryFromHealthPing(const NetworkIdentity& identifier)
{
	if (!ShouldRefreshServerRegistry(identifier))
	{
		return;
	}

	const std::string advertiseIp = ResolveInterlinkAdvertiseIp();
	if (advertiseIp.empty())
	{
		return;
	}

	const IPAddress privateAddress = BuildInterlinkAddress(advertiseIp, _PORT_INTERLINK);
	ServerRegistry::Get().RegisterSelf(identifier, privateAddress);

	if (identifier.Type != NetworkIdentityType::eProxy)
	{
		return;
	}

	IPAddress publicAddress = privateAddress;
	if (const auto publicIp = GetNonEmptyEnv("INTERLINK_PUBLIC_IP"); publicIp.has_value())
	{
		uint32_t publicPort = _PORT_PROXY;
		if (const auto publicPortText = GetNonEmptyEnv("INTERLINK_PUBLIC_PORT");
			publicPortText.has_value())
		{
			try
			{
				publicPort = static_cast<uint32_t>(std::stoul(*publicPortText));
			}
			catch (...)
			{
				publicPort = _PORT_PROXY;
			}
		}
		publicAddress = BuildInterlinkAddress(*publicIp, static_cast<PortType>(publicPort));
	}

	ServerRegistry::Get().RegisterPublicAddress(identifier, publicAddress);
}
}  // namespace

void HealthManifest::HealthUpdate(const NetworkIdentity& identifier)
{
	ByteWriter bw;
	identifier.Serialize(bw);

	const double now = InternalDB::Get()->GetTimeNowSeconds();
	const double TTL = now + _HEALTH_PING_TIMESTAMP_LIFE_MS * 0.001;

	const auto setResult =
		InternalDB::Get()->HSet(HealthPingTable, bw.as_string_view(), std::to_string(TTL));
	if (setResult != 0)
	{
		std::printf("Failed to update health in Health Manifest. HSET result: %lli", setResult);
	}

	try
	{
		RefreshServerRegistryFromHealthPing(identifier);
	}
	catch (...)
	{
	}

	try
	{
		NetworkManifest::Get().TelemetryUpdate(identifier);
	}
	catch (...)
	{
	}
}
