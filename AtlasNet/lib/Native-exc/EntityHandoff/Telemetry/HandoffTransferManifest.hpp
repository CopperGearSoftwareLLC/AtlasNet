#pragma once

#include <chrono>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Entity/Entity.hpp"
#include "Global/Misc/Singleton.hpp"
#include "InternalDB/InternalDB.hpp"
#include "Network/NetworkIdentity.hpp"

// Tracks active timed handoffs for watchdog discrepancy checks.
// Scope is intentionally narrow: in-flight transfers only.
class HandoffTransferManifest : public Singleton<HandoffTransferManifest>
{
  public:
	struct ActiveTransferRecord
	{
		AtlasEntityID entityId = 0;
		std::string source;
		std::string target;
		uint64_t transferTimeUs = 0;
		std::string lastAuthority;
		std::string state;
		uint64_t updatedAtUs = 0;
	};

	static inline constexpr std::chrono::seconds kHolderKeyTtl =
		std::chrono::seconds(30);

	[[nodiscard]] uint64_t NowUnixTimeUs() const
	{
		const auto t = InternalDB::Get()->GetTimeNow();
		if (t.seconds > 0)
		{
			return static_cast<uint64_t>(t.seconds) * 1000000ULL +
				   static_cast<uint64_t>(t.microseconds);
		}
		return static_cast<uint64_t>(
			std::chrono::duration_cast<std::chrono::microseconds>(
				std::chrono::system_clock::now().time_since_epoch())
				.count());
	}

	void MarkTransferStarted(AtlasEntityID entityId, const NetworkIdentity& source,
							 const NetworkIdentity& target, uint64_t transferTimeUs)
	{
		const uint64_t nowUs = NowUnixTimeUs();
		const std::string sourceId = source.ToString();
		const std::string targetId = target.ToString();
		Upsert(ActiveTransferRecord{
			.entityId = entityId,
			.source = sourceId,
			.target = targetId,
			.transferTimeUs = transferTimeUs,
			.lastAuthority = sourceId,
			.state = "started",
			.updatedAtUs = nowUs});

		const std::string holderKey = HoldersKey(entityId);
		const std::vector<std::string_view> members{sourceId};
		const long long added = InternalDB::Get()->SAdd(holderKey, members);
		(void)added;
		const bool ttlSet = InternalDB::Get()->Expire(holderKey, kHolderKeyTtl);
		(void)ttlSet;
	}

	void MarkIncomingAdopted(AtlasEntityID entityId, const NetworkIdentity& source,
							 const NetworkIdentity& target, uint64_t transferTimeUs)
	{
		const std::string sourceId = source.ToString();
		const std::string targetId = target.ToString();
		ActiveTransferRecord record;
		if (!TryGet(entityId, record))
		{
			record.entityId = entityId;
			record.source = sourceId;
			record.target = targetId;
			record.transferTimeUs = transferTimeUs;
		}

		if (record.source.empty())
		{
			record.source = sourceId;
		}
		if (record.target.empty())
		{
			record.target = targetId;
		}
		if (record.transferTimeUs == 0)
		{
			record.transferTimeUs = transferTimeUs;
		}
		record.lastAuthority = targetId;
		record.state = "adopted";
		record.updatedAtUs = NowUnixTimeUs();
		Upsert(record);

		const std::string holderKey = HoldersKey(entityId);
		const std::vector<std::string_view> members{targetId};
		const long long added = InternalDB::Get()->SAdd(holderKey, members);
		(void)added;
		const bool ttlSet = InternalDB::Get()->Expire(holderKey, kHolderKeyTtl);
		(void)ttlSet;
		TryFinalize(entityId, sourceId, targetId);
	}

	void MarkOutgoingCommitted(AtlasEntityID entityId, const NetworkIdentity& source,
							   const NetworkIdentity& target)
	{
		const std::string sourceId = source.ToString();
		const std::string targetId = target.ToString();
		ActiveTransferRecord record;
		if (!TryGet(entityId, record))
		{
			record.entityId = entityId;
			record.source = sourceId;
			record.target = targetId;
		}
		if (record.source.empty())
		{
			record.source = sourceId;
		}
		if (record.target.empty())
		{
			record.target = targetId;
		}
		record.lastAuthority = targetId;
		record.state = "committed";
		record.updatedAtUs = NowUnixTimeUs();
		Upsert(record);

		const std::string holderKey = HoldersKey(entityId);
		const std::vector<std::string_view> members{sourceId};
		const long long removed = InternalDB::Get()->SRem(holderKey, members);
		(void)removed;
		TryFinalize(entityId, sourceId, targetId);
	}

	void MarkTransferCanceled(AtlasEntityID entityId)
	{
		ClearTransfer(entityId);
	}

	void ClearTransfer(AtlasEntityID entityId)
	{
		const std::string entityField = std::to_string(entityId);
		const long long hdel = InternalDB::Get()->HDel(kActiveTransferTable, {entityField});
		(void)hdel;
		const long long delKey = InternalDB::Get()->DelKey(HoldersKey(entityId));
		(void)delKey;
	}

	void GetAllActiveTransfers(std::vector<ActiveTransferRecord>& out) const
	{
		out.clear();
		const auto all = InternalDB::Get()->HGetAll(kActiveTransferTable);
		out.reserve(all.size());
		for (const auto& [entityField, payload] : all)
		{
			ActiveTransferRecord record;
			if (!ParseRecord(entityField, payload, record))
			{
				continue;
			}
			out.push_back(std::move(record));
		}
	}

	[[nodiscard]] std::vector<std::string>
	GetTransferHolders(AtlasEntityID entityId) const
	{
		return InternalDB::Get()->SMembers(HoldersKey(entityId));
	}

  private:
	static inline constexpr std::string_view kActiveTransferTable =
		"EntityHandoff:TransferActive";
	static inline constexpr std::string_view kHolderKeyPrefix =
		"EntityHandoff:TransferHolders:";

	[[nodiscard]] static std::string HoldersKey(AtlasEntityID entityId)
	{
		return std::format("{}{}", kHolderKeyPrefix, entityId);
	}

	[[nodiscard]] static std::string Encode(const ActiveTransferRecord& record)
	{
		return std::format("{}\t{}\t{}\t{}\t{}\t{}", record.source, record.target,
						   record.transferTimeUs, record.lastAuthority,
						   record.state, record.updatedAtUs);
	}

	void Upsert(const ActiveTransferRecord& record)
	{
		const std::string entityField = std::to_string(record.entityId);
		const long long wrote =
			InternalDB::Get()->HSet(kActiveTransferTable, entityField, Encode(record));
		(void)wrote;
	}

	void TryFinalize(AtlasEntityID entityId, const std::string& sourceId,
					 const std::string& targetId)
	{
		const auto holders = InternalDB::Get()->SMembers(HoldersKey(entityId));
		bool hasSource = false;
		bool hasTarget = false;
		for (const auto& holder : holders)
		{
			if (holder == sourceId)
			{
				hasSource = true;
				continue;
			}
			if (holder == targetId)
			{
				hasTarget = true;
			}
		}
		if (hasTarget && !hasSource)
		{
			ClearTransfer(entityId);
		}
	}

	[[nodiscard]] bool TryGet(AtlasEntityID entityId,
							  ActiveTransferRecord& out) const
	{
		const std::string entityField = std::to_string(entityId);
		const auto payload =
			InternalDB::Get()->HGet(kActiveTransferTable, entityField);
		if (!payload.has_value())
		{
			return false;
		}
		return ParseRecord(entityField, *payload, out);
	}

	[[nodiscard]] static bool ParseRecord(const std::string& entityField,
										  const std::string& payload,
										  ActiveTransferRecord& out)
	{
		std::vector<std::string> cols;
		cols.reserve(6);
		size_t start = 0;
		while (start <= payload.size())
		{
			const size_t end = payload.find('\t', start);
			if (end == std::string::npos)
			{
				cols.push_back(payload.substr(start));
				break;
			}
			cols.push_back(payload.substr(start, end - start));
			start = end + 1;
		}
		if (cols.size() != 6)
		{
			return false;
		}

		try
		{
			out.entityId = static_cast<AtlasEntityID>(std::stoull(entityField));
			out.source = cols[0];
			out.target = cols[1];
			out.transferTimeUs = std::stoull(cols[2]);
			out.lastAuthority = cols[3];
			out.state = cols[4];
			out.updatedAtUs = std::stoull(cols[5]);
			return true;
		}
		catch (...)
		{
			return false;
		}
	}
};
