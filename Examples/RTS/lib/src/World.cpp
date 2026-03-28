#include "World.hpp"
#ifdef RTS_CLIENT
#include "Debug/DebugDraw.hpp"
#include "imgui.h"
#endif
#include "Global/Types/AABB.hpp"
#include "Ray.hpp"
void World::UpdateAllTransforms()
{
	std::unique_lock<std::mutex> lock(entityMutex);
	// Start from all root entities (no parent)
	for (auto& [ID, en] : entities)
	{
		if (!en->GetParent().has_value())
		{
			en->UpdateTransforms(glm::mat4(1.0f));	// Identity matrix as parent
		}
	}
}
#ifdef RTS_CLIENT
void World::DebugMenu()
{
	std::unique_lock<std::mutex> lock(entityMutex);
	ImGui::Begin("World Entities");
	static bool DrawAABB = false;

	ImGui::Checkbox("Draw Colliders", &DrawAABB);
	if (DrawAABB)
	{
		World::Get().DebugDrawAABBTree();
	}
	for (auto& [ID, en] : entities)
	{
		// Only draw root entities
		if (!en->GetParent().has_value())
		{
			en->DebugTree();
		}
	}

	ImGui::End();
}
#endif
void World::UpdateEntityRecursive(Entity& entity, float deltaTime)
{
	entity.Update(deltaTime);  // Call the entity's own update
	entity.UpdateTransforms(
		entity.GetParent().has_value()
			? World::Get().GetEntity<Entity>(*entity.GetParent()).GetWorldMatrix()
			: glm::mat4(1.0f));	 // Recompute WorldMatrix

	for (EntityID childID : entity.GetChildren())
	{
		UpdateEntityRecursive(World::Get().GetEntity<Entity>(childID), deltaTime);
	}
}
#ifdef RTS_CLIENT
void World::RenderEntityRecursive(Entity& entity)
{
	entity.Render();
	for (EntityID childID : entity.GetChildren())
	{
		RenderEntityRecursive(World::Get().GetEntity<Entity>(childID));
	}
}

void World::RenderAll()
{
	std::unique_lock<std::mutex> lock(entityMutex);
	for (auto& [ID, en] : entities)
	{
		// Only render root entities
		if (!en->GetParent().has_value())
			RenderEntityRecursive(*en);
	}
}
#endif
void World::UpdateAll(float deltaTime)
{
	std::unique_lock<std::mutex> lock(entityMutex);

	// 1. Update all entities recursively
	for (auto& [ID, en] : entities)
	{
		if (!en->GetParent().has_value())
			UpdateEntityRecursive(*en, deltaTime);
	}

	// 2. Rebuild the AABB tree
	std::vector<Value3f> boxes;
	boxes.reserve(entities.size());

	for (auto& [ID, en] : entities)
	{
		if (en->HasCollider())
		{
			// Get the collider in world space
			const auto& localAABB = en->GetCollider();
			glm::mat4 worldMat = en->GetWorldMatrix();

			// Transform local AABB corners to world space
			glm::vec3 min = localAABB.min;
			glm::vec3 max = localAABB.max;

			glm::vec3 corners[8] = {{min.x, min.y, min.z}, {max.x, min.y, min.z},
									{min.x, max.y, min.z}, {min.x, min.y, max.z},
									{max.x, max.y, min.z}, {min.x, max.y, max.z},
									{max.x, min.y, max.z}, {max.x, max.y, max.z}};

			glm::vec3 wsMin(FLT_MAX), wsMax(-FLT_MAX);
			for (auto& c : corners)
			{
				glm::vec4 wc = worldMat * glm::vec4(c, 1.0f);
				wsMin = glm::min(wsMin, glm::vec3(wc));
				wsMax = glm::max(wsMax, glm::vec3(wc));
			}

			Box3f box({wsMin.x, wsMin.y, wsMin.z}, {wsMax.x, wsMax.y, wsMax.z});
			boxes.emplace_back(box, ID);
		}
	}

	aabbTree = RTree3f(boxes.begin(), boxes.end());	 // Rebuild tree
}
#ifdef RTS_CLIENT
void World::DebugDrawAABBTree()
{
	std::unique_lock<std::mutex> lock(entityMutex);

	if (aabbTree.size() == 0)
		return;
	glm::vec3 color = glm::vec3(0.2f + 0.1f,   // R
								0.5f - 0.05f,  // G
								1.0f - 0.1f	   // B
	);
	color = glm::clamp(color, 0.0f, 1.0f);

	// Iterate over all tree nodes (each Value3f = pair<Box3f, EntityID>)

	for (const auto& value : aabbTree)
	{
		const Box3f& box = value.first;

		glm::vec3 min{box.min_corner().get<0>(), box.min_corner().get<1>(),
					  box.min_corner().get<2>()};
		glm::vec3 max{box.max_corner().get<0>(), box.max_corner().get<1>(),
					  box.max_corner().get<2>()};

		DebugDraw::Get().DrawBox(min, max, color, true);
	}
}
#endif
std::optional<EntityID> World::Raycast(const glm::vec2& ndc, const glm::mat4& viewProj)
{
	std::unique_lock<std::mutex> lock(entityMutex);
	Ray ray = BuildRayFromNDC(ndc, viewProj);

	glm::vec3 rayEnd = ray.origin + ray.dir * 10000.0f;

	Box3f queryBox({std::min(ray.origin.x, rayEnd.x), std::min(ray.origin.y, rayEnd.y),
					std::min(ray.origin.z, rayEnd.z)},
				   {std::max(ray.origin.x, rayEnd.x), std::max(ray.origin.y, rayEnd.y),
					std::max(ray.origin.z, rayEnd.z)});

	float closestT = FLT_MAX;
	std::optional<EntityID> result;

	auto itPair = aabbTree.qbegin(bgi::intersects(queryBox));
	auto end = aabbTree.qend();

	for (auto it = itPair; it != end; ++it)
	{
		const auto& [box, id] = *it;

		glm::vec3 min{box.min_corner().get<0>(), box.min_corner().get<1>(),
					  box.min_corner().get<2>()};
		glm::vec3 max{box.max_corner().get<0>(), box.max_corner().get<1>(),
					  box.max_corner().get<2>()};

		float t;
		AABB3f aabb(min, max);
		if (RayAABB(ray, aabb, t))
		{
			if (t < closestT)
			{
				closestT = t;
				result = id;
			}
		}
	}

	return result;
}
bool World::QueryBox(const Box3f& box, std::vector<EntityID>& outEntities) const

{
	std::vector<Value3f> queryResults;
	aabbTree.query(bgi::intersects(box), std::back_inserter(queryResults));
	for (const auto& [b, id] : queryResults)
	{
		outEntities.push_back(id);
	}
	return !queryResults.empty();
}
