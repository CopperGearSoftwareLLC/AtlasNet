#pragma once

#include <boost/container/small_vector.hpp>

#include "EntityID.hpp"
#include "Global/Types/AABB.hpp"
#include "Global/pch.hpp"
#include "Transform.hpp"
#ifdef RTS_CLIENT
#include "imgui.h"
#endif


class Entity
{
	const EntityID ID;
	std::optional<EntityID> Parent;
	boost::container::small_vector<EntityID, 5> Children;
	mat4 WorldMatrix;
	std::optional<AABB3f> Collider;

   public:
	Transform transform;
	Entity(EntityID _ID) : ID(_ID) {}
	virtual ~Entity() {}

	virtual void Start() = 0;
	virtual void Update(float deltaTime) = 0;
	#ifdef RTS_CLIENT
	virtual void Render() = 0;
	virtual void DebugMenu() {}
	void DebugTree();
	#endif
	virtual void End() = 0;
	void UpdateTransforms(const mat4& parent);
	const mat4& GetWorldMatrix() const { return WorldMatrix; }
	EntityID GetID() const { return ID; }
	auto GetParent() const { return Parent; }
	const auto& GetChildren() const { return Children; }

	void Own(EntityID childID);

	/// Remove ownership of `childID`
	void Disown(EntityID childID);
	[[nodiscard]] bool HasCollider() const { return Collider.has_value(); }
	[[nodiscard]] const AABB3f& GetCollider() const
	{
		ASSERT(Collider.has_value(), "SHIT");
		return Collider.value();
	}
	void SetCollider(const AABB3f& c) { Collider = c; }
	void ClearCollider() { Collider.reset(); }

   private:
	void SetParent(EntityID parentID);
	void AddChild(EntityID childID);
	void RemoveChild(EntityID childID);
	
};