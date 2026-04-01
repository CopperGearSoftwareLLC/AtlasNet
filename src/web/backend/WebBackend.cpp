#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include <drogon/drogon.h>
#include <json/json.h>

namespace
{
namespace fs = std::filesystem;

fs::path findCartographDirectory(fs::path startPath)
{
    startPath = fs::absolute(startPath);

    while (true)
    {
        const auto candidate = startPath / "cartograph-map";
        if (fs::exists(candidate) && fs::is_directory(candidate))
        {
            return candidate;
        }

        if (startPath == startPath.root_path())
        {
            return {};
        }

        startPath = startPath.parent_path();
    }
}

std::optional<fs::path> findTopologyFile(const fs::path &directory)
{
    static const std::vector<std::string> candidates = {
        "map.json",
        "topology.json",
    };

    for (const auto &fileName : candidates)
    {
        const auto candidate = directory / fileName;
        if (fs::exists(candidate) && fs::is_regular_file(candidate))
        {
            return candidate;
        }
    }

    return std::nullopt;
}

std::optional<Json::Value> readJsonFile(const fs::path &filePath, std::string &error)
{
    std::ifstream input(filePath);
    if (!input.is_open())
    {
        error = "Unable to open topology file";
        return std::nullopt;
    }

    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;

    Json::Value root;
    std::string parseErrors;
    if (!Json::parseFromStream(builder, input, &root, &parseErrors))
    {
        error = parseErrors;
        return std::nullopt;
    }

    return root;
}

bool isSupportedTexture(const fs::path &filePath)
{
    auto extension = filePath.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });

    return extension == ".webp" || extension == ".png" || extension == ".jpg" || extension == ".jpeg";
}

bool isSafeLocalFileName(const std::string &fileName)
{
    if (fileName.empty())
    {
        return false;
    }

    const fs::path filePath(fileName);
    return filePath == filePath.filename();
}

std::optional<fs::path> resolveTextureFile(const fs::path &directory)
{
    for (const auto &entry : fs::directory_iterator(directory))
    {
        if (entry.is_regular_file() && isSupportedTexture(entry.path()))
        {
            return entry.path();
        }
    }

    return std::nullopt;
}

Json::Value buildMissingTopologyPayload(const fs::path &directory, const std::optional<fs::path> &textureFile)
{
    Json::Value payload;
    payload["status"] = "missing_topology";
    payload["message"] = "No topology JSON file was found in cartograph-map";
    payload["topologyAvailable"] = false;
    payload["expectedTopologyFiles"].append("map.json");
    payload["expectedTopologyFiles"].append("topology.json");
    payload["source"]["directory"] = directory.filename().string();

    if (textureFile)
    {
        payload["source"]["textureFile"] = textureFile->filename().string();
        payload["textureUrl"] = "/cartograph/map/texture?file=" +
                                drogon::utils::urlEncode(textureFile->filename().string());
    }

    return payload;
}
}  // namespace

int main()
{
    const auto cartographDirectory = findCartographDirectory(fs::current_path());

    drogon::app().registerHandler(
        "/cartograph/map",
        [cartographDirectory](const drogon::HttpRequestPtr &,
                              std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            if (cartographDirectory.empty())
            {
                Json::Value payload;
                payload["status"] = "error";
                payload["message"] = "cartograph-map directory could not be located";
                auto response = drogon::HttpResponse::newHttpJsonResponse(payload);
                response->setStatusCode(drogon::k500InternalServerError);
                callback(response);
                return;
            }

            const auto topologyFile = findTopologyFile(cartographDirectory);
            if (!topologyFile)
            {
                callback(drogon::HttpResponse::newHttpJsonResponse(
                    buildMissingTopologyPayload(cartographDirectory, resolveTextureFile(cartographDirectory))));
                return;
            }

            std::string parseError;
            const auto topology = readJsonFile(*topologyFile, parseError);
            if (!topology)
            {
                Json::Value payload;
                payload["status"] = "error";
                payload["message"] = "Failed to parse topology JSON";
                payload["details"] = parseError;
                payload["source"]["directory"] = cartographDirectory.filename().string();
                payload["source"]["topologyFile"] = topologyFile->filename().string();

                auto response = drogon::HttpResponse::newHttpJsonResponse(payload);
                response->setStatusCode(drogon::k400BadRequest);
                callback(response);
                return;
            }

            const auto textureFile = resolveTextureFile(cartographDirectory);

            Json::Value payload;
            payload["status"] = "ok";
            payload["topologyAvailable"] = true;
            payload["topology"] = *topology;
            payload["source"]["directory"] = cartographDirectory.filename().string();
            payload["source"]["topologyFile"] = topologyFile->filename().string();

            if (textureFile)
            {
                payload["source"]["textureFile"] = textureFile->filename().string();
                payload["textureUrl"] = "/cartograph/map/texture?file=" +
                                        drogon::utils::urlEncode(textureFile->filename().string());
            }

            callback(drogon::HttpResponse::newHttpJsonResponse(payload));
        },
        {drogon::Get});

    drogon::app().registerHandler(
        "/cartograph/map/texture",
        [cartographDirectory](const drogon::HttpRequestPtr &request,
                              std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            if (cartographDirectory.empty())
            {
                auto response = drogon::HttpResponse::newHttpResponse();
                response->setStatusCode(drogon::k500InternalServerError);
                callback(response);
                return;
            }

            const auto fileName = request->getParameter("file");
            if (!isSafeLocalFileName(fileName))
            {
                auto response = drogon::HttpResponse::newHttpResponse();
                response->setStatusCode(drogon::k400BadRequest);
                callback(response);
                return;
            }

            const auto texturePath = cartographDirectory / fileName;
            if (!fs::exists(texturePath) || !fs::is_regular_file(texturePath) || !isSupportedTexture(texturePath))
            {
                auto response = drogon::HttpResponse::newHttpResponse();
                response->setStatusCode(drogon::k404NotFound);
                callback(response);
                return;
            }

            callback(drogon::HttpResponse::newFileResponse(texturePath.string()));
        },
        {drogon::Get});

    drogon::app().addListener("0.0.0.0", 8080);
    drogon::app().setThreadNum(1);
    drogon::app().run();
    return 0;
}
