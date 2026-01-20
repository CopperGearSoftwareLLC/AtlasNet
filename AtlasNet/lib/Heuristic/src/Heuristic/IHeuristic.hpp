#pragma once
#include <boost/describe/enum.hpp>
#include <cstdint>
#include <optional>
#include <pch.hpp>
#include <unordered_map>
#include <vector>

#include "Entity.hpp"
#include "EntityList.hpp"
#include "IBounds.hpp"
#include "Serialize/ByteWriter.hpp"
#include "pch.hpp"
class IHeuristic
{
   public:
	enum class Type
	{
		eNone,
		eGridCell,
		eOctree,
		eQuadtree
	};
	static constexpr const char* TypeToString(Type type) noexcept
	{
		switch (type)
		{
			case Type::eNone:
				return "None";
			case Type::eGridCell:
				return "GridCell";
			case Type::eOctree:
				return "Octree";
			case Type::eQuadtree:
				return "Quadtree";
			default:
				return "Unknown";
		}
	}
	static inline bool TypeFromString(std::string_view str, Type& outType) noexcept
	{
		if (str == "None")
		{
			outType = Type::eNone;
			return true;
		}
		if (str == "GridCell")
		{
			outType = Type::eGridCell;
			return true;
		}
		if (str == "Octree")
		{
			outType = Type::eOctree;
			return true;
		}
		if (str == "Quadtree")
		{
			outType = Type::eQuadtree;
			return true;
		}

		outType = Type::eNone;
		return false;
	}
	// #ifndef SWIG
	//	//BOOST_DESCRIBE_NESTED_ENUM(Type, eNone, eGridCell, eOctree, eQuadtree)
	// #endif


   private:
   public:
	[[nodiscard]] virtual Type GetType() const = 0;

	virtual void Compute(const AtlasEntitySpan<const AtlasEntityMinimal>& span) = 0;

	virtual uint32_t GetBoundsCount() const = 0;

	virtual void SerializeBounds(std::unordered_map<IBounds::BoundsID, ByteWriter>& bws) = 0;
};
template <typename BoundType>
struct TBoundDelta
{
    BoundType OldShape;
    std::optional<BoundType> NewShape;
};

template <typename BoundType>
class THeuristic : public IHeuristic
{
public:

    virtual void GetBounds(std::vector<BoundType>& out_bounds) const = 0;
    virtual void GetBoundDeltas(std::vector<TBoundDelta<BoundType>>& out_deltas) const = 0;
};