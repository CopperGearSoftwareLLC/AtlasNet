#pragma once
#include <Global/pch.hpp>
#include <boost/describe/enum.hpp>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "Debug/Log.hpp"
#include "Entity/Entity.hpp"

#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Global/pch.hpp"
#include "IBounds.hpp"
class IHeuristic
{
   public:
	enum class Type
	{
		eNone,
		eGridCell,
		eOctree,
		eQuadtree,
		eInvalid
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
			case Type::eInvalid:
				return "Invalid";
			default:
				return "Unknown";
		}
	}
	static inline bool TypeFromString(std::string_view str,
									  Type& outType) noexcept
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
		if (str == "Invalid")
		{
			outType = Type::eInvalid;
			return true;
		}
		outType = Type::eNone;
		return false;
	}
	// #ifndef SWIG
	//	//BOOST_DESCRIBE_NESTED_ENUM(Type, eNone, eGridCell, eOctree, eQuadtree)
	// #endif

   protected:
   Log logger = Log("Heuristic");
   public:
	virtual ~IHeuristic() = default;
	[[nodiscard]] virtual Type GetType() const = 0;

	virtual void Compute(
		const std::span<const AtlasEntityMinimal>& span) = 0;

	virtual uint32_t GetBoundsCount() const = 0;
	/** */
	virtual void SerializeBounds(
		std::unordered_map<IBounds::BoundsID, ByteWriter>& bws) = 0;
	virtual void Serialize(ByteWriter& bw) const = 0;
	virtual void Deserialize(ByteReader& br) = 0;
	[[nodiscard]] virtual std::optional<IBounds::BoundsID> QueryPosition(vec3 p) = 0;
	[[nodiscard]] virtual std::unique_ptr<IBounds> GetBound(IBounds::BoundsID id) = 0;
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
	virtual void GetBoundDeltas(
		std::vector<TBoundDelta<BoundType>>& out_deltas) const = 0;
};