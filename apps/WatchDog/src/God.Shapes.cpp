#include "God.hpp"

#include "AtlasNet.hpp"
#include "Connection.hpp"
#include "EntityManifest.hpp"
#include "GridCellManifest.hpp"
#include "InterlinkEnums.hpp"
#include "Misc/GeometryUtils.hpp"
#include "PartitionEntityManifest.hpp"
#include "ServerRegistry.hpp"
#include "ShapeManifest.hpp"
void God::sendHeaderBasedMessage(const std::string& message, AtlasNetMessageHeader header,
								 const std::string& partitionId, InterlinkMessageSendFlag sendFlag)
{
	std::vector<std::byte> buffer;
	buffer.reserve(1 + message.size());

	buffer.push_back(static_cast<std::byte>(header));
	const std::byte* ptr = reinterpret_cast<const std::byte*>(message.data());
	buffer.insert(buffer.end(), ptr, ptr + message.size());

	InterLinkIdentifier targetId =
		InterLinkIdentifier::MakeIDPartition(partitionId.substr(partitionId.find(' ') + 1));
	Interlink::Get().SendMessageRaw(targetId, std::span(buffer), sendFlag);
}

std::vector<InterLinkIdentifier> God::getPartitionServerIds() const
{
	const auto& servers = ServerRegistry::Get().GetServers();
	std::vector<InterLinkIdentifier> ids;
	for (const auto& server : servers)
	{
		if (server.first.Type == InterlinkType::eShard)
			ids.push_back(server.first);
	}
	return ids;
}

std::optional<std::string> God::findPartitionForPoint(const vec2& point) const
{
	// First try fast spatial index lookup
	if (auto fastResult = shapeCache.fastLookup(point))
	{
		return fastResult;
	}

	// Fallback to slow iteration if spatial index not available
	for (const auto& [partitionId, shapeIdx] : shapeCache.partitionToShapeIndex)
	{
		const Shape& shape = shapeCache.shapes[shapeIdx];
		if (shape.contains(point))
			return partitionId;
	}
	return std::nullopt;
}
// Redistribute entity outliers to the correct partitions based on new shapes
void God::RedistributeOutliersToPartitions()
{
	if (!shapeCache.isValid)
	{
		return;
	}

	logger->Debug("Checking and redistributing outliers to partitions");

	std::vector<InterLinkIdentifier> partitionIds = getPartitionServerIds();

	// Check each partition's outliers
	for (const auto& sourceId : partitionIds)
	{
		std::string sourcePartition = sourceId.ToString();
		std::vector<AtlasEntity> outliers = EntityManifest::FetchOutliers(sourcePartition);

		if (outliers.empty())
		{
			continue;
		}

		logger->DebugFormatted("Found {} outliers in partition {}", outliers.size(),
							   sourcePartition);

		// Process each outlier
		for (const auto& entity : outliers)
		{
			vec2 entityPos{entity.Position.x, entity.Position.z};
			std::string targetPartition;
			vec3 newPosition = entity.Position;
			float minDistance = std::numeric_limits<float>::max();
			bool needsRepositioning = true;

			// Try to find containing partition
			if (auto found = findPartitionForPoint(entityPos))
			{
				targetPartition = *found;
				needsRepositioning = false;
			}
			else
			{
				// Track closest point to any triangle across all shapes to gently reposition
				for (const auto& [partitionId, shapeIdx] : shapeCache.partitionToShapeIndex)
				{
					const Shape& shape = shapeCache.shapes[shapeIdx];
					for (const auto& triangle : shape.triangles)
					{
						vec2 closest = GeometryUtils::ClosestPointOnTriangle(
							entityPos, triangle[0], triangle[1], triangle[2]);
						float dist = (entityPos.x - closest.x) * (entityPos.x - closest.x) +
									 (entityPos.y - closest.y) * (entityPos.y - closest.y);
						if (dist < minDistance)
						{
							minDistance = dist;
							targetPartition = partitionId;
							newPosition = vec3{closest.x, 0.0f, closest.y};
						}
					}
				}
			}

			if (!targetPartition.empty() && targetPartition != sourcePartition)
			{
				logger->DebugFormatted("REDISTRIBUTING: Entity {} from {} to {}", entity.ID,
									   sourcePartition, targetPartition);

				// Remove entity from source partition outliers FIRST
				bool removed = EntityManifest::RemoveEntityFromOutliers(sourcePartition, entity.ID);
				if (removed)
				{
					logger->DebugFormatted(
						"DATABASE: Successfully deleted entity {} from {} outliers in database",
						entity.ID, sourcePartition);
				}
				else
				{
					logger->ErrorFormatted(
						"DATABASE: Failed to delete entity {} from {} outliers in database",
						entity.ID, sourcePartition);
				}

				// Update position if needed
				AtlasEntity entityToStore = entity;
				if (needsRepositioning)
				{
					entityToStore.Position = newPosition;
					logger->DebugFormatted("Moved entity {} to new position in shape space",
										   entity.ID);
				}

				// Store entity in target partition outliers
				bool stored = EntityManifest::StoreEntity(targetPartition, entityToStore);
				if (stored)
				{
					logger->DebugFormatted(
						"DATABASE: Successfully stored entity {} in {} outliers in database",
						entity.ID, targetPartition);
				}
				else
				{
					logger->ErrorFormatted(
						"DATABASE: Failed to store entity {} in {} outliers in database", entity.ID,
						targetPartition);
				}
				// Send entity fetch message to target partition using header-based messaging
				std::string fetchMsg = sourcePartition + ":" + std::to_string(entityToStore.ID);
				sendHeaderBasedMessage(fetchMsg, AtlasNetMessageHeader::EntityIncoming,
									   targetPartition);
				// Tell source partition to remove the entity from its managed entities
				std::string removeMsg = std::to_string(entity.ID);
				sendHeaderBasedMessage(removeMsg, AtlasNetMessageHeader::EntityOutgoing,
									   sourcePartition);

				logger->DebugFormatted(
					"NOTIFICATION: Sent fetch message for entity {} to target partition {}",
					entity.ID, targetPartition);
				logger->DebugFormatted(
					"NOTIFICATION: Sent remove message for entity {} to source partition {}",
					entity.ID, sourcePartition);
			}
			else if (targetPartition.empty())
			{
				// Entity doesn't belong to any partition - remove it from outliers entirely
				bool removed = EntityManifest::RemoveEntityFromOutliers(sourcePartition, entity.ID);
				if (removed)
				{
					logger->DebugFormatted(
						"NO PARTITION: Entity {} doesn't belong to any partition - successfully "
						"removed from outliers in database",
						entity.ID);
				}
				else
				{
					logger->ErrorFormatted(
						"NO PARTITION: Entity {} doesn't belong to any partition - failed to "
						"remove from outliers in database",
						entity.ID);
				}
			}
		}
	}
}

void God::processOutliersForPartition(const std::string& partitionId)
{
	if (!shapeCache.isValid)
	{
		logger->Error("Cannot process outliers - no valid shapes or database connection");
		return;
	}

	// Fetch outliers from the partition
	std::vector<AtlasEntity> outliers = EntityManifest::FetchOutliers( partitionId);
	if (outliers.empty())
	{
		logger->DebugFormatted("No outliers found for partition {}", partitionId);
		return;
	}

	logger->DebugFormatted("PROCESSING: Starting to process {} outliers from partition {}",
						   outliers.size(), partitionId);

	// Process each outlier
	for (const auto& entity : outliers)
	{
		vec2 entityPos{entity.Position.x, entity.Position.z};
		std::string targetPartition;
		bool foundShape = false;

		logger->DebugFormatted(
			"GOD PROCESSING: Checking entity {} at ({}, {}) for correct partition", entity.ID,
			entityPos.x, entityPos.y);

		// First check if entity is within valid bounds (0.0 to 1.0 for normalized coordinates)
		// Entities outside this range should remain as outliers
		if (!GeometryUtils::IsWithinBounds(entityPos))
		{
			logger->DebugFormatted(
				"OUT OF BOUNDS: Entity {} at ({}, {}) is outside valid coordinate range [0,1] - "
				"keeping as outlier",
				entity.ID, entityPos.x, entityPos.y);
			continue;  // Skip redistribution for out-of-bounds entities
		}

		// Check each partition's shape to find where this entity belongs
		for (const auto& [targetId, shapeIdx] : shapeCache.partitionToShapeIndex)
		{
			const Shape& shape = shapeCache.shapes[shapeIdx];

			// Debug: Log triangle coordinates for this partition
			logger->DebugFormatted("GOD DEBUG: Checking partition {} with {} triangles", targetId,
								   shape.triangles.size());
			for (size_t i = 0; i < shape.triangles.size(); ++i)
			{
				const auto& tri = shape.triangles[i];
				logger->DebugFormatted("GOD DEBUG: Triangle {}: ({}, {}) ({}, {}) ({}, {})", i,
									   tri[0].x, tri[0].y, tri[1].x, tri[1].y, tri[2].x, tri[2].y);
			}

			// Check if point is in any triangle of this shape
			for (const auto& triangle : shape.triangles)
			{
				if (GeometryUtils::PointInTriangle(entityPos, triangle[0], triangle[1],
												   triangle[2]))
				{
					targetPartition = targetId;
					foundShape = true;
					logger->DebugFormatted("GOD FOUND: Entity {} belongs to partition {}",
										   entity.ID, targetId);
					break;
				}
			}
			if (foundShape)
				break;
		}

		if (foundShape && targetPartition != partitionId)
		{
			logger->DebugFormatted("REDISTRIBUTING: Entity {} from {} to {}", entity.ID,
								   partitionId, targetPartition);

			// NEW REDISTRIBUTION FLOW:
			// 1. Remove entity from source partition outliers FIRST to prevent duplicate processing
			// 2. Tell source partition to remove the entity from its managed entities
			// 3. Source partition will send entity info back to God
			// 4. God will then redistribute the entity to the correct partition

			// Remove from source partition outliers first to prevent duplicate processing
			bool removed =
				EntityManifest::RemoveEntityFromOutliers(partitionId, entity.ID);
			if (removed)
			{
				logger->DebugFormatted(
					"DATABASE: Successfully removed entity {} from {} outliers before "
					"redistribution",
					entity.ID, partitionId);
			}
			else
			{
				logger->ErrorFormatted(
					"DATABASE: Failed to remove entity {} from {} outliers before redistribution",
					entity.ID, partitionId);
				// Continue anyway - the entity might have been removed already
			}

			std::string removeMsg = "RemoveEntity:" + std::to_string(entity.ID);
			InterLinkIdentifier sourceId =
				InterLinkIdentifier::MakeIDPartition(partitionId.substr(partitionId.find(' ') + 1));
			Interlink::Get().SendMessageRaw(sourceId, std::as_bytes(std::span(removeMsg)));

			logger->DebugFormatted(
				"REDISTRIBUTION: Sent remove message for entity {} to source partition {} - "
				"waiting for entity info",
				entity.ID, partitionId);
		}
		else if (!foundShape)
		{
			// Entity doesn't belong to any shape - remove it from outliers entirely
			bool removed =
				EntityManifest::RemoveEntityFromOutliers(partitionId, entity.ID);
			if (removed)
			{
				logger->DebugFormatted(
					"NO SHAPE: Entity {} doesn't belong to any shape - successfully removed from "
					"outliers in database",
					entity.ID);
			}
			else
			{
				logger->ErrorFormatted(
					"NO SHAPE: Entity {} doesn't belong to any shape - failed to remove from "
					"outliers in database",
					entity.ID);
			}
		}
		else
		{
			// Entity belongs to the same partition - remove from outliers
			bool removed =
				EntityManifest::RemoveEntityFromOutliers( partitionId, entity.ID);
			if (removed)
			{
				logger->DebugFormatted(
					"SAME PARTITION: Entity {} belongs to same partition - successfully removed "
					"from outliers in database",
					entity.ID);
			}
			else
			{
				logger->ErrorFormatted(
					"SAME PARTITION: Entity {} belongs to same partition - failed to remove from "
					"outliers in database",
					entity.ID);
			}
		}
	}
}

void God::processExistingOutliers()
{
	if (!shapeCache.isValid)
	{
		logger->Error("Cannot process existing outliers - no valid shapes or database connection");
		return;
	}

	logger->Debug(
		"PROCESSING EXISTING OUTLIERS: Checking database for existing outliers to redistribute");

	std::vector<InterLinkIdentifier> partitionIds = getPartitionServerIds();

	// Check each partition for existing outliers
	for (const auto& partitionId : partitionIds)
	{
		std::string partitionKey = partitionId.ToString();
		logger->DebugFormatted("CHECKING: Looking for existing outliers in partition {}",
							   partitionKey);

		std::vector<AtlasEntity> outliers = EntityManifest::FetchOutliers(partitionKey);

		if (!outliers.empty())
		{
			logger->DebugFormatted("EXISTING OUTLIERS: Found {} existing outliers for partition {}",
								   outliers.size(), partitionKey);

			// Process these outliers using the same logic as the message handler
			processOutliersForPartition(partitionKey);
		}
		else
		{
			logger->DebugFormatted("NO OUTLIERS: No existing outliers found for partition {}",
								   partitionKey);
		}
	}
}

void God::notifyPartitionsToFetchShapes()
{
	// Only notify partitions that we just assigned shapes to
	for (const auto& id : assignedPartitions)
	{
		// Extract just the container name from the full identifier (removes "ePartition " prefix)
		std::string containerName = id.ToString().substr(id.ToString().find(' ') + 1);

		// Clear any existing partition entity data for this partition to prevent stale data
		std::string partitionKey = id.ToString();
		PartitionEntityManifest::ClearPartition(partitionKey);

		// Send grid cell shape fetch message using header-based messaging
		logger->DebugFormatted(
			"Sending grid shape notify to partition {} which was assigned a shape", partitionKey);
		sendHeaderBasedMessage("", AtlasNetMessageHeader::FetchGridShape, partitionKey);
	}
}

const Shape* God::getCachedShape(const std::string& partitionId) const
{
	if (!shapeCache.isValid)
	{
		return nullptr;
	}

	auto it = shapeCache.partitionToShapeIndex.find(partitionId);
	if (it != shapeCache.partitionToShapeIndex.end() && it->second < shapeCache.shapes.size())
	{
		return &shapeCache.shapes[it->second];
	}
	return nullptr;
}

void God::setHeuristicType(HeuristicType type)
{
	currentHeuristicType = type;
	heuristic.setHeuristicType(type);
	logger->DebugFormatted("Heuristic type set to {}", static_cast<int>(type));
}

bool God::computeAndStorePartitions()
{
	try
	{
		// Ensure heuristic is using the correct type
		heuristic.setHeuristicType(currentHeuristicType);

		// Try to fetch entities for density-based algorithms
		std::vector<AtlasEntity> entities;

		entities = EntityManifest::FetchAllEntitiesFromAllPartitions(true, true);
		entities = EntityManifest::DeduplicateEntities(entities);
		logger->DebugFormatted("Fetched {} entities for heuristic computation", entities.size());

		// Compute partition shapes using the current heuristic type
		std::vector<Shape> partitionShapes = heuristic.computePartition(entities);

		// Update shape cache
		shapeCache.shapes = partitionShapes;
		shapeCache.partitionToShapeIndex.clear();

		logger->DebugFormatted("Computed {} partition shapes", partitionShapes.size());

		// Initialize cache database if not already done


		// Get all partition servers from registry
		const auto& servers = ServerRegistry::Get().GetServers();
		std::vector<InterLinkIdentifier> partitionIds;

		// Filter to get only partition IDs
		for (const auto& server : servers)
		{
			if (server.first.Type == InterlinkType::eShard)
			{
				partitionIds.push_back(server.first);
			}
		}

		// Sort partition IDs to ensure consistent assignment
		std::sort(partitionIds.begin(), partitionIds.end());

		size_t numShapesToAssign = std::min(partitionShapes.size(), partitionIds.size());
		logger->DebugFormatted("Assigning {} shapes to {} partitions", numShapesToAssign,
							   partitionIds.size());

		// Keep track of which partitions were assigned shapes
		assignedPartitions.clear();

		// Convert triangle shapes to grid cell shapes and store them
		for (size_t i = 0; i < numShapesToAssign; ++i)
		{
			// Convert triangle-based shape to grid cell shape
			GridShape gridShape;
			gridShape.partitionId = partitionIds[i].ToString();

			// Convert triangles to grid cells (assuming 2 triangles per grid cell)
			partitionShapes[i].appendGridCellsFromTriangles(gridShape, i);

			// Store grid cell shape instead of triangle shape
			if (!GridCellManifest::Store( partitionIds[i].ToString(), i, gridShape))
			{
				logger->ErrorFormatted("Failed to store grid shape for partition {} in database",
									   partitionIds[i].ToString());
				return false;
			}

			// Track this partition as having a shape assigned
			assignedPartitions.insert(partitionIds[i]);

			logger->DebugFormatted("Stored grid shape for partition {} with {} cells",
								   partitionIds[i].ToString(), gridShape.cells.size());
		}

		// If there's only one shape, log that other partitions are ignored
		if (partitionShapes.size() == 1 && partitionIds.size() > 1)
		{
			logger->Debug(
				"Only one shape available - assigned to first partition, remaining partitions "
				"ignored");
		}

		// Store metadata about the grid cell partition computation
		if (!GridCellManifest::StoreMetadata( numShapesToAssign, partitionIds.size()))
		{
			logger->Error("Failed to store grid cell partition metadata");
			return false;
		}

		// Update shape cache indexing
		for (size_t i = 0; i < numShapesToAssign; ++i)
		{
			shapeCache.partitionToShapeIndex[partitionIds[i].ToString()] = i;
		}
		shapeCache.isValid = true;

		// Build spatial index for fast partition lookups
		shapeCache.buildSpatialIndex();
		logger->DebugFormatted("Built spatial index with {} entries for fast partition lookups",
							   shapeCache.spatialIndex.size());

		logger->Debug(
			"Successfully computed and stored all partition shapes in database and memory cache");
		return true;
	}
	catch (const std::exception& e)
	{
		logger->ErrorFormatted("Error in computeAndStorePartitions: {}", e.what());
		return false;
	}
}

void God::processOutliersWithGridCells()
{
	if (!shapeCache.isValid)
	{
		logger->Error(
			"Cannot process outliers with grid cells - no valid shapes or database connection");
		return;
	}

	logger->Debug("PROCESSING: Using efficient grid cell-based outlier detection");

	// Get all partition servers from registry
	const auto& servers = ServerRegistry::Get().GetServers();
	std::vector<InterLinkIdentifier> partitionIds;
	for (const auto& server : servers)
	{
		if (server.first.Type == InterlinkType::eShard)
		{
			partitionIds.push_back(server.first);
		}
	}

	// Check each partition for existing outliers
	for (const auto& partitionId : partitionIds)
	{
		std::string partitionKey = partitionId.ToString();
		std::vector<AtlasEntity> outliers =
			EntityManifest::FetchOutliers(partitionKey);

		if (outliers.empty())
		{
			continue;
		}

		logger->DebugFormatted("GRID CELL PROCESSING: Found {} outliers for partition {}",
							   outliers.size(), partitionKey);

		// Process each outlier using grid cell detection
		for (const auto& entity : outliers)
		{
			vec2 entityPos{entity.Position.x, entity.Position.z};
			std::string targetPartition;
			bool foundPartition = false;

			logger->DebugFormatted("GRID CELL CHECK: Entity {} at ({}, {})", entity.ID, entityPos.x,
								   entityPos.y);

			if (auto found = findPartitionForPoint(entityPos))
			{
				targetPartition = *found;
				foundPartition = true;
				logger->DebugFormatted("GRID CELL FOUND: Entity {} belongs to partition {}",
									   entity.ID, targetPartition);
			}

			if (foundPartition && targetPartition != partitionKey)
			{
				logger->DebugFormatted("GRID CELL REDISTRIBUTING: Entity {} from {} to {}",
									   entity.ID, partitionKey, targetPartition);

				// Remove entity from source partition outliers
				bool removed =
					EntityManifest::RemoveEntityFromOutliers( partitionKey, entity.ID);
				if (removed)
				{
					logger->DebugFormatted(
						"GRID CELL DATABASE: Successfully deleted entity {} from {} outliers",
						entity.ID, partitionKey);
				}
				else
				{
					logger->ErrorFormatted(
						"GRID CELL DATABASE: Failed to delete entity {} from {} outliers",
						entity.ID, partitionKey);
				}

				// Store entity in target partition outliers
				bool stored = EntityManifest::StoreEntity(targetPartition, entity);
				if (stored)
				{
					logger->DebugFormatted(
						"GRID CELL DATABASE: Successfully stored entity {} in {} outliers",
						entity.ID, targetPartition);
				}
				else
				{
					logger->ErrorFormatted(
						"GRID CELL DATABASE: Failed to store entity {} in {} outliers", entity.ID,
						targetPartition);
				}

				// Notify partitions
				std::string fetchMsg = partitionKey + ":" + std::to_string(entity.ID);
				sendHeaderBasedMessage(fetchMsg, AtlasNetMessageHeader::EntityIncoming,
									   targetPartition);
				std::string removeMsg = std::to_string(entity.ID);
				sendHeaderBasedMessage(removeMsg, AtlasNetMessageHeader::EntityOutgoing,
									   partitionKey);

				logger->DebugFormatted(
					"GRID CELL NOTIFICATION: Sent messages for entity {} redistribution",
					entity.ID);
			}
			else if (!foundPartition)
			{
				// Entity doesn't belong to any partition - remove it from outliers
				bool removed =
					EntityManifest::RemoveEntityFromOutliers(partitionKey, entity.ID);
				if (removed)
				{
					logger->DebugFormatted(
						"GRID CELL NO PARTITION: Entity {} doesn't belong to any partition - "
						"removed from outliers",
						entity.ID);
				}
				else
				{
					logger->ErrorFormatted(
						"GRID CELL NO PARTITION: Failed to remove entity {} from outliers",
						entity.ID);
				}
			}
			else
			{
				// Entity belongs to the same partition - remove from outliers
				bool removed =
					EntityManifest::RemoveEntityFromOutliers( partitionKey, entity.ID);
				if (removed)
				{
					logger->DebugFormatted(
						"GRID CELL SAME PARTITION: Entity {} belongs to same partition - removed "
						"from outliers",
						entity.ID);
				}
				else
				{
					logger->ErrorFormatted(
						"GRID CELL SAME PARTITION: Failed to remove entity {} from outliers",
						entity.ID);
				}
			}
		}
	}
}

void God::redistributeEntityToCorrectPartition(const AtlasEntity& entity,
											   const std::string& sourcePartition)
{
	if (!shapeCache.isValid)
	{
		logger->Error("Cannot redistribute entity - no valid shapes or database connection");
		return;
	}

	vec2 entityPos{entity.Position.x, entity.Position.z};
	std::string targetPartition;
	bool foundPartition = false;

	logger->DebugFormatted("REDISTRIBUTION: Finding correct partition for entity {} at ({}, {})",
						   entity.ID, entityPos.x, entityPos.y);

	// First check if entity is within valid bounds (0.0 to 1.0 for normalized coordinates)
	// Entities outside this range should remain as outliers
	if (!GeometryUtils::IsWithinBounds(entityPos))
	{
		logger->DebugFormatted(
			"OUT OF BOUNDS: Entity {} at ({}, {}) is outside valid coordinate range [0,1] - "
			"keeping as outlier",
			entity.ID, entityPos.x, entityPos.y);
		return;	 // Don't redistribute out-of-bounds entities
	}

	if (auto found = findPartitionForPoint(entityPos))
	{
		targetPartition = *found;
		foundPartition = true;
		logger->DebugFormatted("REDISTRIBUTION FOUND: Entity {} belongs to partition {}", entity.ID,
							   targetPartition);
	}

	if (foundPartition && targetPartition != sourcePartition)
	{
		logger->DebugFormatted("REDISTRIBUTION: Moving entity {} from {} to {}", entity.ID,
							   sourcePartition, targetPartition);

		// Check if this is a dummy response (entity not found in source partition)
		if (entity.Position.x == 0.0f && entity.Position.y == 0.0f && entity.Position.z == 0.0f)
		{
			logger->DebugFormatted(
				"DUMMY RESPONSE: Entity {} was not found in source partition - skipping "
				"redistribution",
				entity.ID);
			return;
		}

		// Store entity in target partition outliers
		bool stored = EntityManifest::StoreEntity( targetPartition, entity);
		if (stored)
		{
			logger->DebugFormatted(
				"REDISTRIBUTION DATABASE: Successfully stored entity {} in {} outliers", entity.ID,
				targetPartition);

			// Also add to target partition's entity manifest
			if (!PartitionEntityManifest::AddEntity(targetPartition, entity))
			{
				logger->ErrorFormatted(
					"REDISTRIBUTION MANIFEST: Failed to add entity {} to {} partition manifest",
					entity.ID, targetPartition);
			}
			else
			{
				logger->DebugFormatted(
					"REDISTRIBUTION MANIFEST: Added entity {} to {} partition manifest", entity.ID,
					targetPartition);
			}

			// Tell target partition to fetch the entity
			// switch this over using header based messaging
			std::string fetchMsg = sourcePartition + ":" + std::to_string(entity.ID);
			sendHeaderBasedMessage(fetchMsg, AtlasNetMessageHeader::EntityIncoming,
								   targetPartition);

			logger->DebugFormatted(
				"REDISTRIBUTION NOTIFICATION: Sent fetch message for entity {} to target partition "
				"{}",
				entity.ID, targetPartition);
		}
		else
		{
			logger->ErrorFormatted(
				"REDISTRIBUTION DATABASE: Failed to store entity {} in {} outliers", entity.ID,
				targetPartition);
		}
	}
	else if (!foundPartition)
	{
		// Entity doesn't belong to any partition - keep it in outliers database
		logger->DebugFormatted(
			"REDISTRIBUTION: Entity {} doesn't belong to any partition - keeping in outliers "
			"database",
			entity.ID);

		// Store it back in the source partition's outliers since it doesn't belong anywhere else
		bool stored = EntityManifest::StoreEntity( sourcePartition, entity);
		if (stored)
		{
			logger->DebugFormatted(
				"REDISTRIBUTION: Stored entity {} back in {} outliers (no valid partition found)",
				entity.ID, sourcePartition);
		}
		else
		{
			logger->ErrorFormatted("REDISTRIBUTION: Failed to store entity {} back in {} outliers",
								   entity.ID, sourcePartition);
		}
	}
	else
	{
		// Entity belongs to the same partition - this shouldn't happen but handle gracefully
		logger->DebugFormatted(
			"REDISTRIBUTION: Entity {} already belongs to partition {} - no action needed",
			entity.ID, targetPartition);
	}
}
