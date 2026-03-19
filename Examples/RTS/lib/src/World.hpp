#pragma once

#include <atomic>
#include <boost/container/flat_map.hpp>
#include <boost/geometry.hpp>
#include <memory>
#include <type_traits>
#include <unordered_map>

#include "Entity.hpp"
#include "EntityID.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Global/pch.hpp"
namespace bg = boost::geometry;
namespace bgi = boost::geometry::index;

// For simplicity, using float 3D AABBs
using Box3f = bg::model::box<bg::model::point<float, 3, bg::cs::cartesian>>;
using Value3f = std::pair<Box3f, EntityID>;
using RTree3f = bgi::rtree<Value3f, bgi::quadratic<16>>;
class World : public Singleton<World>
{
	boost::container::flat_map<EntityID, std::unique_ptr<Entity>> entities;
	RTree3f aabbTree;
	std::atomic<EntityID> NextEntityID = 0;

   public:
	template <typename T>
		requires std::is_base_of_v<Entity, T>
	[[nodiscard]] T& CreateEntity()
	{
		EntityID ID = NextEntityID++;

		std::unique_ptr<Entity> newEntity = std::make_unique<T>(ID);
		ASSERT(newEntity, "Failed to create entity");
		const auto [it, success] = entities.emplace(ID, std::move(newEntity));
		ASSERT(success, "Failed to create ENtitiy");
		return static_cast<T&>(*entities.at(ID));
	}

	template <typename T>
		requires std::is_base_of_v<Entity, T>
	T& GetEntity(EntityID ID)
	{
		ASSERT(entities.contains(ID), "INVALID ID");
		T* e = dynamic_cast<T*>(entities.at(ID).get());
		ASSERT(e, "FAILED CAST");
		return *e;
	}
	void UpdateAllTransforms();
	void DebugMenu();
	void DebugDrawAABBTree();
	void UpdateAll(float deltaTime);

	#ifdef RTS_CLIENT
	void RenderAll();		
	#endif
	template <typename FN>
		requires std::is_invocable_v<FN, Entity&>
	void ForEachEntity(FN&& f)
	{
		for (auto& [ID, en] : entities)
		{
			f(*en);
		}
	}
	const decltype(aabbTree)& GetAABBTree() const 
	{return aabbTree;}
	std::optional<EntityID> Raycast(const glm::vec2& ndc, const glm::mat4& viewProj);

   private:
	void UpdateEntityRecursive(Entity& entity, float deltaTime);
		#ifdef RTS_CLIENT
		void RenderEntityRecursive(Entity& entity);
		#endif
};