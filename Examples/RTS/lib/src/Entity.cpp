#include "Entity.hpp"

#include "World.hpp"
void Entity::UpdateTransforms(const mat4& parent)
{
	WorldMatrix = parent * transform.GetModelMatrix();
	for (const EntityID eID : Children)
	{
		World::Get().GetEntity<Entity>(eID).UpdateTransforms(WorldMatrix);
	}
}
void Entity::SetParent(EntityID parentID)
{
	// Remove from previous parent's children list if needed
	if (Parent.has_value())
	{
		Entity& oldParent = World::Get().GetEntity<Entity>(*Parent);
		oldParent.RemoveChild(ID);
	}

	Parent = parentID;

	// Add self to new parent's children list
	Entity& newParent = World::Get().GetEntity<Entity>(parentID);
	newParent.AddChild(ID);
}
void Entity::AddChild(EntityID childID)
{
	if (std::find(Children.begin(), Children.end(), childID) == Children.end())
		Children.push_back(childID);
}
void Entity::RemoveChild(EntityID childID)
{
	auto it = std::find(Children.begin(), Children.end(), childID);
	if (it != Children.end())
		Children.erase(it);
}
	#ifdef RTS_CLIENT

void Entity::DebugTree()
{
	// Make the label show ID (and optionally parent)
	std::string label = "Entity " + std::to_string(ID);
	if (Parent.has_value())
		label += " (Parent: " + std::to_string(*Parent) + ")";

	// ImGui tree node
	if (ImGui::TreeNode(label.c_str()))
	{
		// Show transform info
		transform.DebugMenu();
        
        DebugMenu();
		// Recursively show children
		for (EntityID childID : Children)
		{
			Entity& child = World::Get().GetEntity<Entity>(childID);
			child.DebugTree();
		}

		ImGui::TreePop();
	}
}
#endif
void Entity::Disown(EntityID childID)
{
	Entity& child = World::Get().GetEntity<Entity>(childID);

	// Only disown if we are the current parent
	if (child.Parent.has_value() && *child.Parent == ID)
	{
		child.Parent.reset();
		RemoveChild(childID);
	}
}
void Entity::Own(EntityID childID)
{
	Entity& child = World::Get().GetEntity<Entity>(childID);

	// Remove child from previous parent
	if (child.Parent.has_value())
	{
		Entity& oldParent = World::Get().GetEntity<Entity>(*child.Parent);
		oldParent.RemoveChild(childID);
	}

	// Set parent and add to children list
	child.Parent = ID;
	AddChild(childID);
}
