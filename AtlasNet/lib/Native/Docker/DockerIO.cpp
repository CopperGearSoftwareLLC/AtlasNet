#include "DockerIO.hpp"

#include <ifaddrs.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <fstream>
#include <iostream>
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
	((std::string*)userp)->append((char*)contents, size * nmemb);
	return size * nmemb;
}

std::string DockerIO::request(const std::string& method, const std::string& endpoint,
							  const nlohmann::json* body,
							  const std::vector<std::string>& extraHeaders) const
{
	CURL* curl = curl_easy_init();
	if (!curl)
	{
		throw std::runtime_error("Failed to init curl");
	}
	std::string response;
	struct curl_slist* headers = nullptr;
	headers = curl_slist_append(headers, "Content-Type: application/json");

	for (auto& h : extraHeaders)
	{
		headers = curl_slist_append(headers, h.c_str());
	}

	std::string url;
	if (unixSocket.rfind("http", 0) == 0)
	{  // remote TCP
		url = unixSocket + endpoint;
	}
	else
	{  // local UNIX socket
		url = "http://localhost" + endpoint;
		curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, unixSocket.c_str());
	}

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	std::string bodyStr;
	if (body)
	{
		bodyStr = body->dump();
		curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, bodyStr.c_str());
	}

	CURLcode res = curl_easy_perform(curl);
	curl_slist_free_all(headers);
	if (res != CURLE_OK)
	{
		headers = nullptr;
		curl_easy_cleanup(curl);
		throw std::runtime_error(std::string("curl_easy_perform failed: ") +
								 curl_easy_strerror(res));
	}
	curl_easy_cleanup(curl);
	return response;
}

std::string DockerIO::InspectContainer(const DockerContainerID& ContainerID) const
{
	return request("GET", std::string("/containers/").append(ContainerID).append("/json"));
}

nlohmann::json DockerIO::InspectSelf() const
{
	if (SelfJson.has_value())
		return SelfJson.value();
	std::string endpoint = "/containers/" + GetSelfContainerName() + "/json";
	std::string response = request("GET", endpoint, nullptr, {});
	SelfJson.emplace(nlohmann::json::parse(response));
	return *SelfJson;
}

std::string DockerIO::GetSelfContainerName() const
{
	std::ifstream file("/etc/hostname");
	std::string id;
	if (file)
		std::getline(file, id);
	if (id.empty())
		throw std::runtime_error("Could not read /etc/hostname for container Name");

	return id;
}

std::string DockerIO::GetSelfContainerIP() const
{
	struct ifaddrs* ifaddr = nullptr;
	if (getifaddrs(&ifaddr) != 0 || !ifaddr)
		return "";

	std::string out;
	for (auto* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
	{
		if (!ifa->ifa_name || !ifa->ifa_addr)
			continue;
		if (std::strcmp(ifa->ifa_name, "eth0") != 0)
			continue;
		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;

		char buf[INET_ADDRSTRLEN] = {};
		auto* sin = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr);
		if (inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf)))
		{
			out = buf;
			break;
		}
	}

	freeifaddrs(ifaddr);
	return out;
}

std::vector<std::pair<uint32_t, uint32_t>> DockerIO::GetSelfExposedPorts() const
{
	std::vector<std::pair<uint32_t, uint32_t>> portBindings;
	const auto ports = InspectSelf()["NetworkSettings"]["Ports"];
	const auto portInfo = ports.items();
	for (const auto& [key, value] : portInfo)
	{
		const uint32_t InternalPort = std::stoi(key.substr(0, key.find('/')));
		const uint32_t ExternalPort =
			value.empty() ? 0 : std::stoi(value[0]["HostPort"].get<std::string>());
		if (ExternalPort != 0)
			portBindings.push_back({InternalPort, ExternalPort});
	}
	return portBindings;
}

std::optional<uint32_t> DockerIO::GetSelfExposedPortForInternalBind(uint32_t InternalPort) const
{
	const auto ports = GetSelfExposedPorts();
	std::cerr << "Looking for " << InternalPort << std::endl;
	const auto find = std::find_if(ports.begin(), ports.end(),
								   [InternalPort = InternalPort](std::pair<uint32_t, uint32_t> p)
								   {
									   std::cerr << p.first << std::endl;
									   return InternalPort == p.first;
								   });
	if (find == ports.end())
		return std::nullopt;
	return find->second;
}

// returns public IP and sets outPort to the mapped port
std::string DockerIO::GetSelfPublicIP(uint32_t& outPort) const
{
	outPort = 0;  // default in case no mapping is found

	try
	{
		nlohmann::json info = InspectSelf();
		auto ports = info["NetworkSettings"]["Ports"];

		for (auto& [containerPort, value] : ports.items())
		{
			if (!value.empty())
			{
				// Docker-provided mapping
				std::string hostIp = value[0]["HostIp"].get<std::string>();
				std::string hostPort = value[0]["HostPort"].get<std::string>();

				// Set output port
				outPort = std::stoul(hostPort);

				// Normalize IP (Docker often reports "0.0.0.0")
				if (hostIp == "0.0.0.0" || hostIp == "::")
				{
					// fallback to system or external IP
					CURL* curl = curl_easy_init();
					if (!curl)
						return "0.0.0.0";
					std::string response;
					curl_easy_setopt(curl, CURLOPT_URL, "https://api.ipify.org");
					curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
					curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
					CURLcode res = curl_easy_perform(curl);
					curl_easy_cleanup(curl);
					if (res == CURLE_OK && !response.empty())
						return response;
				}

				return hostIp;
			}
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << "[DockerIO] GetSelfPublicIP failed: " << e.what() << std::endl;
	}

	// If we reach here, Docker didn’t have a port mapping
	// → fallback to external IP discovery only
	CURL* curl = curl_easy_init();
	if (curl)
	{
		std::string response;
		curl_easy_setopt(curl, CURLOPT_URL, "https://api.ipify.org");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		CURLcode res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);

		if (res == CURLE_OK && !response.empty())
			return response;
	}

	return "0.0.0.0";
}

std::optional<uint32_t> DockerIO::GetSelfPublicPortFor(uint32_t internalPort) const
{
	return GetSelfExposedPortForInternalBind(internalPort);
}

std::string DockerIO::EncodeFilters(const nlohmann::json& filters) const
{
	std::string raw = filters.dump();

	char* enc = curl_easy_escape(nullptr, raw.c_str(), raw.length());
	if (!enc)
		throw std::runtime_error("curl_easy_escape failed");

	std::string encoded(enc);
	curl_free(enc);

	return encoded;
}

std::optional<uint32_t> DockerIO::GetServicePublishedPort(const std::string& serviceName,
														  uint32_t internalPort,
														  const std::string& protocol) const
{
	try
	{
		// --- Query Docker service ---
		nlohmann::json service = nlohmann::json::parse(request("GET", "/services/" + serviceName));

		// Normalize protocol (udp/tcp)
		std::string desired = protocol;
		std::transform(desired.begin(), desired.end(), desired.begin(), ::tolower);

		// Helper function to search inside an array of port dicts
		auto findPort = [&](const nlohmann::json& arr,
							const std::string& pathName) -> std::optional<uint32_t>
		{
			if (!arr.is_array())
				return std::nullopt;

			for (const auto& p : arr)
			{
				if (!p.contains("Protocol") || !p.contains("TargetPort") ||
					!p.contains("PublishedPort"))
					continue;

				std::string proto = p["Protocol"].get<std::string>();
				std::transform(proto.begin(), proto.end(), proto.begin(), ::tolower);

				uint32_t target = p["TargetPort"].get<uint32_t>();
				uint32_t published = p["PublishedPort"].get<uint32_t>();

				if (proto == desired && target == internalPort)
				{
					std::cerr << "[Swarm] Match in " << pathName << " → PublishedPort=" << published
							  << "\n";
					return published;
				}
			}

			return std::nullopt;
		};

		// --- 1. Try runtime ports (Endpoint.Ports) ---
		if (service.contains("Endpoint") && service["Endpoint"].contains("Ports"))
		{
			auto result = findPort(service["Endpoint"]["Ports"], "Endpoint.Ports");
			if (result)
				return result;
		}

		// --- 2. Try Spec.EndpointSpec.Ports ---
		if (service.contains("Spec") && service["Spec"].contains("EndpointSpec") &&
			service["Spec"]["EndpointSpec"].contains("Ports"))
		{
			auto result =
				findPort(service["Spec"]["EndpointSpec"]["Ports"], "Spec.EndpointSpec.Ports");
			if (result)
				return result;
		}

		// --- If still nothing, print entire JSON for debugging ---
		std::cerr << "\n[Swarm] Port lookup FAILED for service '" << serviceName
				  << "' (internalPort=" << internalPort << ", protocol=" << desired << ")\n";

		std::cerr << "[Swarm] FULL SERVICE JSON:\n" << service.dump(4) << "\n";

		std::cerr << "[Swarm] nullopt public PORT " << "\n";
		return std::nullopt;
	}
	catch (const std::exception& e)
	{
		std::cerr << "[Swarm] Exception: " << e.what() << "\n";
		return std::nullopt;
	}
}

std::optional<std::string> DockerIO::GetServiceNodePublicIP(const std::string& serviceName) const
{
	try
	{
		nlohmann::json filters = {{"service", {serviceName}}};

		std::string encoded = EncodeFilters(filters);

		nlohmann::json tasks = nlohmann::json::parse(request("GET", "/tasks?filters=" + encoded));

		if (!tasks.is_array() || tasks.empty())
			return std::nullopt;

		std::string nodeID = tasks[0]["NodeID"];

		nlohmann::json node = nlohmann::json::parse(request("GET", "/nodes/" + nodeID));

		if (node.contains("Status") && node["Status"].contains("Addr"))
		{
			return node["Status"]["Addr"].get<std::string>();
		}
	}
	catch (...)
	{
	}
	std::cerr << "[Swarm] nullopt public IP " << "\n";
	return std::nullopt;
}
