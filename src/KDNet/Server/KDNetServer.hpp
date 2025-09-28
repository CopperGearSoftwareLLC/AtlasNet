#include "KDNet/KDEntity.hpp"
#include "KDNet/KDNetInterface.hpp"
#include "Singleton.hpp"
#include "pch.hpp"
#include "Debug/Log.hpp"
enum KDServerRequestType {
    Raycast,
    SphereOverlap,

};
struct KDServerRequest {
    KDServerRequestType Type;
};
using KDServerResponseType = std::vector<std::byte>;
class KDNetServer : public KDNetInterface, public Singleton<KDNetServer> {
    
    std::shared_ptr<Log> logger = std::make_shared<Log>("KDNetServer");

   public:
    KDNetServer() {};
    /**
     * @brief
     *
     */
    struct InitializeProperties {
	std::function<KDServerRequestType(KDServerRequest)> RequestHandleFunction;
    };
    /**
     * @brief Initializes the KDNet Front end
     *
     */
    void Initialize(InitializeProperties properties = {});

    /**
     * @brief Update tick for KDNet.
     *
     * @param entities Your current Entity information.
     * @param IncomingEntities Entities incoming that you must store and keep track of.
     * @param OutgoingEntities Entity IDs of entities you should get rid of.
     */
    void Update(std::span<KDEntity> entities, std::vector<KDEntity>& IncomingEntities,
		std::vector<KDEntityID>& OutgoingEntities);
};