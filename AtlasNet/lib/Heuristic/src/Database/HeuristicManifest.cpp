#include "HeuristicManifest.hpp"

#include <boost/describe/enum_from_string.hpp>

#include "Heuristic/IHeuristic.hpp"
#include "InternalDB.hpp"
//IHeuristic::Type HeuristicManifest::GetActiveHeuristic() const
//{
//	const auto TypeEntry = InternalDB::Get()->Get(HeuristicTypeKey);
//	if (!TypeEntry) //if the entry in redis does not exist the its none of em.
//		return IHeuristic::Type::eNone;
//
//	IHeuristic::Type type;
//
//	boost::describe::enum_from_string(TypeEntry.value(), type);
//	return type;
//}
void HeuristicManifest::GetPendingBoundsAsByteReaders(
	std::vector<std::string>& data_for_readers,
	std::unordered_map<IBounds::BoundsID, ByteReader>& brs)
{
	const auto values = InternalDB::Get()->HGetAll(PendingHashTable);
	for (const auto& [key_raw, value_raw] : values)
	{
		data_for_readers.push_back(value_raw);
		ByteReader key_reader(key_raw);
		const auto ID = key_reader.read_scalar<IBounds::BoundsID>();
		brs.emplace(ID, ByteReader(data_for_readers.back()));
	}
}
void HeuristicManifest::StorePendingBoundsFromByteWriters(
	const std::unordered_map<IBounds::BoundsID, ByteWriter>& in_writers)
{
	std::string_view s_id;
	for (const auto& [ID, writer] : in_writers)
	{
		ByteWriter bw_id;
		bw_id.u32(ID);
		s_id = bw_id.as_string_view();
		InternalDB::Get()->HSet(PendingHashTable, s_id, writer.as_string_view());
	}
}
void HeuristicManifest::SetActiveHeuristicType(IHeuristic::Type type)
{
	const char* str = IHeuristic::TypeToString(type);
	const bool result = InternalDB::Get()->Set(HeuristicTypeKey, str);
	ASSERT(result, "Failed to set key");
}
IHeuristic::Type HeuristicManifest::GetActiveHeuristicType() const
{
	const auto get = InternalDB::Get()->Get(HeuristicTypeKey);
	if (!get)
		return IHeuristic::Type::eNone;
	IHeuristic::Type type;
	IHeuristic::TypeFromString(get.value(), type);
	return type;
}
