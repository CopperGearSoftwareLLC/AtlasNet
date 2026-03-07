#include "ShardService.hpp"

#include <fstream>
#include <stdexcept>

#include "Docker/DockerIO.hpp"

void ShardService::Internal_ScaleShardServiceDockerSwarm(uint32_t newShardCount)
{
	const std::string filter = "%7B%22label%22%3A%5B%22atlasnet.role%3Dshard%22%5D%7D";

	std::string listResp = DockerIO::Get().request("GET", "/services?filters=" + filter);
	auto services = Json::parse(listResp, nullptr, false);

	if (services.is_discarded() || !services.is_array() || services.empty())
	{
		ASSERT(false, "FAILURE");
	}

	const std::string serviceId = services[0]["ID"];
	std::string inspectResp = DockerIO::Get().request("GET", "/services/" + serviceId);
	auto inspectJson = Json::parse(inspectResp, nullptr, false);
	if (inspectJson.is_discarded())
	{
		logger.Warning("Swarm service inspect response was invalid JSON.");
		return;
	}

	int version = inspectJson["Version"]["Index"];
	auto spec = inspectJson["Spec"];
	spec["Mode"]["Replicated"]["Replicas"] = newShardCount;

	std::string updatePath =
		"/services/" + serviceId + "/update?version=" + std::to_string(version);

	std::string updateResp = DockerIO::Get().request("POST", updatePath, &spec);

	if (!updateResp.empty())
	{
		logger.DebugFormatted("Service update responded with\n{}", Json::parse(updateResp).dump(4));
	}

	logger.DebugFormatted("Scaled swarm shard service to {} replicas", newShardCount);
}
void ShardService::ScaleShardService(uint32_t newShardCount)
{
	auto IsEnvSet = [](const char* name)
	{
		const char* v = std::getenv(name);
		return v && v[0] != '\0';
	};
	activeShardCount = newShardCount;

	if (IsEnvSet("KUBERNETES_SERVICE_HOST"))
	{
		Internal_ScaleShardServiceKubernetes(newShardCount);
	}
	else if (IsEnvSet("ATLASNET_DOCKER_MODE"))
	{
		Internal_ScaleShardServiceDockerSwarm(newShardCount);
	}
	else
	{
		ASSERT(false, "No run mode specified");
	}
};

static std::string ReadTextFile(const char* path)
{
	std::ifstream file(path, std::ios::in | std::ios::binary);
	if (!file)
	{
		return {};
	}

	return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

static void TrimTrailingWhitespace(std::string& value)
{
	while (!value.empty() && (value.back() == '\n' || value.back() == '\r' ||
							  value.back() == '\t' || value.back() == ' '))
	{
		value.pop_back();
	}
}

static size_t CurlWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
	if (!userdata || !ptr)
	{
		return 0;
	}
	auto* out = static_cast<std::string*>(userdata);
	out->append(ptr, size * nmemb);
	return size * nmemb;
}

static std::string ResolvePodNamespace()
{
	if (const char* podNamespace = std::getenv("POD_NAMESPACE"); podNamespace && *podNamespace)
	{
		return std::string(podNamespace);
	}

	std::string namespaceFromFile =
		ReadTextFile("/var/run/secrets/kubernetes.io/serviceaccount/namespace");
	TrimTrailingWhitespace(namespaceFromFile);
	return namespaceFromFile;
}
void ShardService::Internal_ScaleShardServiceKubernetes(uint32_t newShardCount)
{
	const char* host = std::getenv("KUBERNETES_SERVICE_HOST");
	if (!host || !*host)
	{
		throw std::runtime_error("INVALID KUBERNETES_SERVICE_HOST");
	}
	const std::string port = [&]() -> std::string
	{
		const char* envPort = std::getenv("KUBERNETES_SERVICE_PORT_HTTPS");
		if (envPort && *envPort)
		{
			return std::string(envPort);
		}
		return "443";
	}();

	const std::string tokenPath = "/var/run/secrets/kubernetes.io/serviceaccount/token";
	std::string token = ReadTextFile(tokenPath.c_str());
	TrimTrailingWhitespace(token);
	ASSERT(!token.empty(), "Kubernetes service account token is missing.");

	const std::string podNamespace = ResolvePodNamespace();
	ASSERT(!podNamespace.empty(), "Unable to resolve pod namespace for Kubernetes scaling.");

	const std::string deploymentName = [&]() -> std::string
	{
		const char* envDeployment = std::getenv("ATLASNET_SHARD_DEPLOYMENT");
		if (envDeployment && *envDeployment)
		{
			return std::string(envDeployment);
		}
		return "atlasnet-shard";
	}();

	const std::string requestUrl =
		std::format("https://{}:{}/apis/apps/v1/namespaces/{}/deployments/{}/scale", host, port,
					podNamespace, deploymentName);

	Json payload;
	payload["spec"]["replicas"] = newShardCount;
	const std::string payloadText = payload.dump();

	static const bool curlInitialized = (curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK);
	if (!curlInitialized)
	{
		logger.Error("libcurl initialization failed for Kubernetes scaling.");
		throw std::runtime_error("libcurl initialization failed for Kubernetes scaling.");
	}

	CURL* curl = curl_easy_init();
	if (!curl)
	{
		logger.Error("Failed to initialize CURL handle for Kubernetes scaling.");
		throw std::runtime_error("Failed to initialize CURL handle for Kubernetes scaling.");
	}

	std::string responseBody;
	struct curl_slist* headers = nullptr;
	const std::string authHeader = "Authorization: Bearer " + token;
	headers = curl_slist_append(headers, authHeader.c_str());
	headers = curl_slist_append(headers, "Content-Type: application/merge-patch+json");

	const std::string caPath = "/var/run/secrets/kubernetes.io/serviceaccount/ca.crt";
	curl_easy_setopt(curl, CURLOPT_URL, requestUrl.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payloadText.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(payloadText.size()));
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
	curl_easy_setopt(curl, CURLOPT_CAINFO, caPath.c_str());

	const CURLcode curlCode = curl_easy_perform(curl);
	long responseCode = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	if (curlCode != CURLE_OK)
	{
		logger.ErrorFormatted("Kubernetes scale request failed: {}", curl_easy_strerror(curlCode));
		throw std::runtime_error("Kubernetes scale request failed:");
	}

	if (responseCode < 200 || responseCode >= 300)
	{
		logger.ErrorFormatted("Kubernetes scale request returned HTTP {}. Body: {}", responseCode,
							  responseBody);
		throw std::runtime_error("Kubernetes scale request failed");
	}
}
