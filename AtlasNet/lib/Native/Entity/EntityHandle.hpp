#pragma once
#include <functional>
#include <future>
#include <variant>

#include "Entity.hpp"
class AtlasEntityHandle
{
	AtlasEntityID id;
	std::variant<AtlasEntityMinimal, AtlasEntity> EntityData;

   public:
	std::future<const AtlasEntityMinimal&> Get()
	{
		return std::async(
			std::launch::async,
			[this]() -> const AtlasEntityMinimal&
			{
				// Simulate delay
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				AtlasEntityMinimal em;
				EntityData = em;
				return em;
			});
	}
	std::future<const AtlasEntity&> GetFull() {}

	std::future<bool> Call(std::function<bool(AtlasEntity&)>);
};
