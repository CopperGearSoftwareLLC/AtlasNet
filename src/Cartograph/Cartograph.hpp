#pragma once
#include "pch.hpp"
#include "Singleton.hpp"
#include "GL/Texture.hpp"
#include "Debug/Log.hpp"
#include "Database/RedisCacheDatabase.hpp"
#include "Heuristic/Shape.hpp"
#include "Database/GridCellManifest.hpp"
#include "AtlasNet/AtlasEntity.hpp"
class Cartograph : public Singleton<Cartograph>
{
    GLFWwindow *_glfwwindow;
    std::shared_ptr<Log> logger = std::make_shared<Log>("Cartograph");
    std::unique_ptr<RedisCacheDatabase> database;

    struct ActivePartition
    {
        GridShape shape;
        std::string Name;
        std::string NodeID;
        vec2 ScreenSpaceShapeCenter;
        vec3 UniquePartitionColor;
        std::vector<AtlasEntity> entities;
    };
    
    std::vector<ActivePartition> partitions;
    struct IndexByName;
    struct IndexByNode;
    boost::multi_index_container<
		ActivePartition,
		boost::multi_index::indexed_by<
			// non-unique by name
			boost::multi_index::ordered_unique<
				boost::multi_index::tag<IndexByName>,
				boost::multi_index::member<ActivePartition, std::string, &ActivePartition::Name>>,
			// non-unique by node
			boost::multi_index::ordered_unique<
				boost::multi_index::tag<IndexByNode>,
				boost::multi_index::member<ActivePartition, std::string, &ActivePartition::NodeID>>>>
		_partitions;

    std::optional<uint32> PartitionIndexSelected;

    struct NodeWorker
    {
        std::string id, hostname, addr;
        vec3 Color;
    };
    std::unordered_map<std::string, NodeWorker> workers;

    enum class ConnectionState
    {
        eIdle,
        eFailed,
        eConnected
    };
    ConnectionState connectionState;
    std::string connectionLog;
    int32 pollingRate = 500;
    bool DebugMode = false;
    void Startup();
    void DrawBackground();

    std::string IP2Manager = "localhost";
    void DrawConnectTo();

    void DrawMap();
    void DrawPartitionGrid();
    void DrawLog();
    void DrawOptions();

    void Update();

    void Render();
    void ConnectTo(const std::string &ip);
    void DrawTo(ImDrawList *, const GridShape &sh, const mat4 &transform = mat4(1.0f),vec4 Color = vec4(1.0f),float rounding = 0.0f,float thickness = 1.0f);
    void DrawToPlot(ImDrawList *, const GridShape &sh, const mat4 &transform = mat4(1.0f),vec4 Color = vec4(1.0f),float rounding = 0.0f,float thickness = 1.0f);
    static std::shared_ptr<Texture> LoadTextureFromCompressed(const void *data, size_t size);

    struct Container
    {
        std::string IP;
        std::string image;
        std::string name;
        std::string ID;
        std::string Node; // Which worker is it in
        std::vector<std::pair<uint32, uint32>> Out2InPorts;
    };
    void GetNodes();
    void GetEntities();
    void ForEachContainer(std::function<void(const Container &, const Json &)> func);
    static vec3 HSVtoRGB(vec3 vec);

    vec3 GetUniqueColor(int index);
    static const std::string InitialLayout;
    static glm::mat4 FitBoundsToTarget(const glm::vec2 &minBound,
                                       const glm::vec2 &maxBound,
                                       const glm::vec2 &targetHalf, // half-extent: maps to [-targetHalf, +targetHalf]
                                       float padding = 0.0f);

public:
    Cartograph() {}
    void Run();
};