#include "AtlasNet/AtlasEntity.hpp"
#include "AtlasNet/AtlasNetInterface.hpp"
#include "Singleton.hpp"
#include "pch.hpp"
#include "Debug/Log.hpp"
#include "Docker/DockerEvents.hpp"
enum KDServerRequestType {
    Raycast,
    SphereOverlap,

};
struct KDServerRequest {
    KDServerRequestType Type;
};
using KDServerResponseType = std::vector<std::byte>;
class AtlasNetServer : public AtlasNetInterface, public Singleton<AtlasNetServer> {
    
    std::shared_ptr<Log> logger = std::make_shared<Log>("AtlasNetServer");

   public:
    AtlasNetServer() {};
    /**
     * @brief
     *
     */
    struct InitializeProperties {
	std::function<KDServerRequestType(KDServerRequest)> RequestHandleFunction;
    std::string ExePath;
    std::function<void(SignalType signal)>OnShutdownRequest;
    };
    /**
     * @brief Initializes the AtlasNet Front end
     *
     */
    void Initialize(InitializeProperties& properties);

    /**
     * @brief Update tick for AtlasNet.
     *
     * @param entities Your current Entity information.
     * @param IncomingEntities Entities incoming that you must store and keep track of.
     * @param OutgoingEntities Entity IDs of entities you should get rid of.
     */
    void Update(std::span<AtlasEntity> entities, std::vector<AtlasEntity>& IncomingEntities,
		std::vector<AtlasEntityID>& OutgoingEntities);
};