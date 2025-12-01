#include "Partition.hpp"
#include "Interlink/Interlink.hpp"
#include "pch.hpp"
#include "Database/IDatabase.hpp"
#include "Database/RedisCacheDatabase.hpp"
#include "Database/ShapeManifest.hpp"
#include "Database/GridCellManifest.hpp"
#include "Database/EntityManifest.hpp"
#include "Database/PartitionEntityManifest.hpp"
#include "Utils/GeometryUtils.hpp"
#include "Heuristic/Shape.hpp"

Partition::Partition()
{
	// Initialize persistent database connection
	database = std::make_unique<RedisCacheDatabase>();
	if (!database->Connect())
	{
		logger->Error("Failed to connect to database in Partition constructor");
	}

	// Initialize notification timer
	lastOutlierNotification = std::chrono::steady_clock::now();
	lastEntitiesSnapshotPush = std::chrono::steady_clock::now();
}
Partition::~Partition()
{
}

// Helper method to get current partition identifier
InterLinkIdentifier Partition::getCurrentPartitionId() const
{
	return InterLinkIdentifier(InterlinkType::ePartition, DockerIO::Get().GetSelfContainerName());
}
void Partition::Init()
{
	InterLinkIdentifier partitionIdentifier(InterlinkType::ePartition, DockerIO::Get().GetSelfContainerName());

	logger = std::make_shared<Log>(partitionIdentifier.ToString());
	Interlink::Get().Init(
		InterlinkProperties{
			.ThisID = partitionIdentifier,
			.logger = logger,
			.callbacks = {.acceptConnectionCallback = [](const Connection &c)
						  { return true; },
						  .OnConnectedCallback = [this](const InterLinkIdentifier &Connection)
						  {
                logger->Debug("[Partition] connected callback");
                if (Connection.Type == InterlinkType::eGameServer)
                {
                  ConnectedGameServer = std::make_unique<InterLinkIdentifier>(Connection);
                  logger->DebugFormatted("[Partition] ready to send messages to GameServer: {}", Connection.ToString().c_str());
                }
                else if (Connection.Type == InterlinkType::eDemigod)
                {
                  ConnectedProxies.insert(Connection);
                  logger->DebugFormatted("[Partition] ready to send messages to Proxy: {}", Connection.ToString().c_str());
                  logger->DebugFormatted("[Partition] Proxy count: {}", ConnectedProxies.size());
                }
              },
						  .OnMessageArrival = [](const Connection &fromWhom, std::span<const std::byte> data) {Partition::Get().MessageArrived(fromWhom,data);},
              .OnDisconnectedCallback = [this](const InterLinkIdentifier &Connection)
              {
                logger->Debug("[Partition] disconnected callback");
                if (Connection.Type == InterlinkType::eGameServer)
                {
                  ConnectedGameServer = nullptr;
                  logger->DebugFormatted("[Partition] GameServer disconnected: {}", Connection.ToString().c_str());
                }
                else if (Connection.Type == InterlinkType::eDemigod)
                {
                  ConnectedProxies.erase(Connection);
                  logger->DebugFormatted("[Partition] Proxy disconnected: {}", Connection.ToString().c_str());
                  logger->DebugFormatted("[Partition] Proxy count: {}", ConnectedProxies.size());
                }
              }
            }
          }
        );

	// Clear any existing partition entity data to prevent stale data
	std::string partitionKey = partitionIdentifier.ToString();
	PartitionEntityManifest::ClearPartition(database.get(), partitionKey);

	// Send initial test message to God
	std::string testMessage = "Hello This is a test message from " + partitionIdentifier.ToString();
	Interlink::Get().SendMessageRaw(InterLinkIdentifier::MakeIDGod(), std::as_bytes(std::span(testMessage)), InterlinkMessageSendFlag::eReliableNow);
	while (!ShouldShutdown)
	{
		// Continuously check for outliers and notify God if found
		checkForOutliersAndNotifyGod();

		// Periodically notify God about outliers (every 30 seconds)
		notifyGodAboutOutliers();

		// Periodically push a read-only snapshot of managed entities to the database
		pushManagedEntitiesSnapshot();

		Interlink::Get().Tick();
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}

	logger->Debug("Shutting Down");
	Interlink::Get().Shutdown();
}

void Partition::MessageArrived(const Connection &fromWhom, std::span<const std::byte> data)
{
	if (data.size() < 1)
	{
		logger->Error("Received empty message");
		return;
	}

	AtlasNetMessageHeader header = static_cast<AtlasNetMessageHeader>(data[0]);
	std::vector<AtlasEntity> entities;

	// ------------------------------------------------------------------------
	// Check if this message is a valid entity packet (raw AtlasEntity data)
	// NOTE: should cache as unordered_map or set using entity IDs instead
	// ------------------------------------------------------------------------
	if (ParseEntityPacket(data, header, entities))
	{
		logger->DebugFormatted("[Partition] Received {} entities from {} with header {}",
							   entities.size(), fromWhom.target.ToString(), static_cast<int>(header));
		// Update managed entities
		switch (header)
		{
		case AtlasNetMessageHeader::EntityUpdate:
		{
			managedEntities = entities;
			logger->Debug("[Partition] Proxy");
			for (const auto &proxy : ConnectedProxies)
			{
				logger->Debug("[Partition] forwarded to proxy");
				Interlink::Get().SendMessageRaw(proxy, data, InterlinkMessageSendFlag::eUnreliableNow);
			}
			return;
		}
		case AtlasNetMessageHeader::EntityIncoming:
		{
			for (const auto &entity : entities)
				managedEntities.push_back(entity);
			break;
		}
		case AtlasNetMessageHeader::EntityOutgoing:
		{
			for (const auto &entity : entities)
			{
				managedEntities.erase(
					std::remove_if(managedEntities.begin(), managedEntities.end(),
								   [&](const AtlasEntity &e)
								   { return e.ID == entity.ID; }),
					managedEntities.end());
			}
			break;
		}
		default:
			logger->WarningFormatted("[Partition] Unknown header {}", static_cast<int>(header));
			break;
		}
		// Forward to proxies
		for (const auto &proxy : ConnectedProxies)
		{
			Interlink::Get().SendMessageRaw(proxy, data, InterlinkMessageSendFlag::eUnreliableNow);
		}
		// Forward to GameServer
		if (ConnectedGameServer != nullptr)
		{
			std::vector<std::byte> buffer;
			buffer.reserve(1 + entities.size() * sizeof(AtlasEntity));
			buffer.push_back(static_cast<std::byte>(header));
			for (const auto &entity : entities)
			{
				const std::byte *ptr = reinterpret_cast<const std::byte *>(&entity);
				buffer.insert(buffer.end(), ptr, ptr + sizeof(AtlasEntity));
			}
			Interlink::Get().SendMessageRaw(*ConnectedGameServer, std::span(buffer), InterlinkMessageSendFlag::eUnreliableNow);
		}

		return;
	}

	// ------------------------------------------------------------------------
	// Handle header-based string messages (not entity packets)
	// ------------------------------------------------------------------------
	const std::byte *payload = data.data() + 1;
	const size_t payloadSize = data.size() - 1;
	std::string msg(reinterpret_cast<const char *>(payload), payloadSize);

	// Debug: log all incoming messages
	logger->DebugFormatted("MESSAGE RECEIVED: '{}' (length: {}, header: {})", msg, msg.length(), static_cast<int>(header));

	switch (header)
	{
	case AtlasNetMessageHeader::EntityIncoming:
	{
		// Handle fetch entity from database (format: "sourcePartition:entityId")
		// Parse message format: "sourcePartition:entityId"
		size_t colon = msg.find(':');
		if (colon == std::string::npos)
		{
			logger->ErrorFormatted("Invalid EntityIncoming message format (missing colon): {}", msg);
			return;
		}

		std::string sourcePartition = msg.substr(0, colon);
		std::string entityIdStr = msg.substr(colon + 1);

		// Validate entity ID string before conversion
		if (entityIdStr.empty())
		{
			logger->ErrorFormatted("Empty entity ID in EntityIncoming message: {}", msg);
			return;
		}

		// Check if string contains only digits
		if (entityIdStr.find_first_not_of("0123456789") != std::string::npos)
		{
			logger->ErrorFormatted("Invalid entity ID format (non-numeric): '{}' in message: {}", entityIdStr, msg);
			return;
		}

		AtlasEntityID entityId;
		try
		{
			entityId = std::stoi(entityIdStr);
		}
		catch (const std::invalid_argument &e)
		{
			logger->ErrorFormatted("Invalid entity ID format in EntityIncoming message: '{}' - Error: {}", entityIdStr, e.what());
			return;
		}
		catch (const std::out_of_range &e)
		{
			logger->ErrorFormatted("Entity ID out of range in EntityIncoming message: '{}' - Error: {}", entityIdStr, e.what());
			return;
		}

		// Use persistent database connection
		if (!database)
		{
			logger->Error("Database connection is null - cannot fetch entity");
			return;
		}

		// Fetch entity from source partition's outliers
		std::vector<AtlasEntity> outliers = EntityManifest::FetchOutliers(database.get(), sourcePartition);

		// Find the specific entity in outliers
		auto it = std::find_if(outliers.begin(), outliers.end(),
							   [entityId](const AtlasEntity &e)
							   { return e.ID == entityId; });

		if (it != outliers.end())
		{
			// Add to our managed entities
			managedEntities.push_back(*it);
			logger->DebugFormatted("FETCHED: Added entity {} to managed entities", entityId);

			// Update partition manifest with new entity
			std::string partitionKey = getCurrentPartitionId().ToString();
			if (!PartitionEntityManifest::AddEntity(database.get(), partitionKey, *it))
			{
				logger->ErrorFormatted("Failed to add entity {} to partition manifest", entityId);
			}
			else
			{
				logger->DebugFormatted("PARTITION MANIFEST: Added entity {} to partition manifest", entityId);
			}

			// Remove from our reported outliers set if it was there
			reportedOutliers.erase(entityId);

			logger->DebugFormatted("REDISTRIBUTION COMPLETE: Successfully fetched entity {} from {}'s outliers", entityId, sourcePartition);
			// Only notify proxies if entity is a player
			if (it->IsPlayer)
			{
				std::string partitionStr = getCurrentPartitionId().ToString();
				std::string authorityMsg =
					"AuthorityChange:" + std::to_string(it->ID) + " " + partitionStr;

				for (const auto &proxy : ConnectedProxies)
				{
					Interlink::Get().SendMessageRaw(proxy, std::as_bytes(std::span(authorityMsg)), InterlinkMessageSendFlag::eReliableNow);
					logger->DebugFormatted("PROXY NOTIFY: Player {} moved to {}, sent to proxy {}",
										   it->ID, partitionStr, proxy.ToString());
				}
			}
		}
		else
		{
			logger->ErrorFormatted("Could not find entity {} in {}'s outliers", entityId, sourcePartition);
		}
		break;
	}

	case AtlasNetMessageHeader::EntityOutgoing:
	{
		// Handle remove entity (format: "entityId")
		try
		{
			std::string entityIdStr = msg;

			// Trim whitespace and check for valid characters
			entityIdStr.erase(0, entityIdStr.find_first_not_of(" \t\r\n"));
			entityIdStr.erase(entityIdStr.find_last_not_of(" \t\r\n") + 1);

			if (entityIdStr.empty())
			{
				logger->ErrorFormatted("Empty entity ID in EntityOutgoing message: {}", msg);
				return;
			}

			// Check if string contains only digits
			if (entityIdStr.find_first_not_of("0123456789") != std::string::npos)
			{
				logger->ErrorFormatted("Invalid entity ID format (non-numeric): '{}' in message: {}", entityIdStr, msg);
				return;
			}

			AtlasEntityID entityId;
			try
			{
				entityId = std::stoi(entityIdStr);
			}
			catch (const std::invalid_argument &e)
			{
				logger->ErrorFormatted("Invalid entity ID format in EntityOutgoing message: '{}' - Error: {}", entityIdStr, e.what());
				return;
			}
			catch (const std::out_of_range &e)
			{
				logger->ErrorFormatted("Entity ID out of range in EntityOutgoing message: '{}' - Error: {}", entityIdStr, e.what());
				return;
			}

			// Find and remove entity from our managed entities
			auto it = std::find_if(managedEntities.begin(), managedEntities.end(),
								   [entityId](const AtlasEntity &e)
								   { return e.ID == entityId; });

			if (it != managedEntities.end())
			{
				// Get entity info before removing it
				AtlasEntity entity = *it;
				managedEntities.erase(it);
				logger->DebugFormatted("REMOVED: Removed entity {} from managed entities", entityId);

				// Update partition manifest to remove entity
				std::string partitionKey = getCurrentPartitionId().ToString();
				if (!PartitionEntityManifest::RemoveEntity(database.get(), partitionKey, entityId))
				{
					logger->ErrorFormatted("Failed to remove entity {} from partition manifest", entityId);
				}
				else
				{
					logger->DebugFormatted("PARTITION MANIFEST: Removed entity {} from partition manifest", entityId);
				}

				// Send entity info back to God for redistribution
				std::string entityInfoMsg = "EntityRemoved:" +
											InterLinkIdentifier(InterlinkType::ePartition, DockerIO::Get().GetSelfContainerName()).ToString() + ":" +
											std::to_string(entity.ID) + ":" +
											std::to_string(entity.Position.x) + ":" +
											std::to_string(entity.Position.y) + ":" +
											std::to_string(entity.Position.z);

				Interlink::Get().SendMessageRaw(InterLinkIdentifier::MakeIDGod(), std::as_bytes(std::span(entityInfoMsg)), InterlinkMessageSendFlag::eReliableNow);
				logger->DebugFormatted("REDISTRIBUTION: Sent entity info back to God: {}", entityInfoMsg);
			}
			else
			{
				logger->DebugFormatted("ENTITY NOT FOUND: Entity {} not in managed entities (may have been already redistributed)", entityId);
				// Send a dummy response to God to acknowledge the request
				// This prevents God from waiting indefinitely for a response
				std::string entityInfoMsg = "EntityRemoved:" +
											InterLinkIdentifier(InterlinkType::ePartition, DockerIO::Get().GetSelfContainerName()).ToString() + ":" +
											std::to_string(entityId) + ":0.0:0.0:0.0"; // Dummy position since entity not found

				Interlink::Get().SendMessageRaw(InterLinkIdentifier::MakeIDGod(), std::as_bytes(std::span(entityInfoMsg)), InterlinkMessageSendFlag::eReliableNow);
				logger->DebugFormatted("ACKNOWLEDGED: Sent dummy response to God for missing entity {}", entityId);
			}
		}
		catch (const std::exception &e)
		{
			logger->ErrorFormatted("Error parsing EntityOutgoing message: {} - Message: '{}'", e.what(), msg);
		}
		break;
	}

	case AtlasNetMessageHeader::FetchGridShape:
	{
		// Use persistent database connection
		if (!database)
		{
			logger->Error("Database connection is null - cannot fetch grid shape");
			return;
		}

		// The key in the database uses our partition ID
		std::string key = getCurrentPartitionId().ToString();

		// Try to fetch our grid shape data
		std::optional<std::string> gridShapeData = GridCellManifest::Fetch(database.get(), key);
		if (!gridShapeData.has_value())
		{
			logger->ErrorFormatted("No grid shape found for partition {}", key);
			return;
		}

		// Parse the grid shape data
		try
		{
			GridShape gridShape = GridCellManifest::ParseGridShape(gridShapeData.value());

			// Assign the grid shape to our member variable
			partitionGridShape = std::move(gridShape);
			logger->DebugFormatted("Successfully loaded grid shape with {} cells", partitionGridShape.cells.size());

			// Debug: Log grid cell coordinates for verification
			for (size_t i = 0; i < partitionGridShape.cells.size(); ++i)
			{
				const auto &cell = partitionGridShape.cells[i];
				logger->DebugFormatted("Grid Cell {}: ({}, {}) to ({}, {}) [row={}, col={}]",
									   i, cell.min.x, cell.min.y, cell.max.x, cell.max.y, cell.row, cell.col);
			}

			// Clear reported outliers since we have a new shape
			reportedOutliers.clear();
		}
		catch (const std::exception &e)
		{
			logger->ErrorFormatted("Error parsing grid shape data: {}", e.what());
			return;
		}
		break;
	}

	default:
	{
		logger->WarningFormatted("[Partition] Unknown header {}", static_cast<int>(header));
		break;
	}
	}
}

void Partition::checkForOutliersAndNotifyGod()
{
	// Only check if we have a valid shape and entities
	if ((partitionShape.triangles.empty() && partitionGridShape.cells.empty()) || managedEntities.empty())
	{
		return;
	}

	// Use shared geometry utility

	// Check all entities for outliers
	std::vector<AtlasEntity> outliers;
	for (const auto &entity : managedEntities)
	{
		vec2 entityPos{entity.Position.x, entity.Position.z};
		bool isInside = false;

		// Use grid cells if available (much faster), otherwise fall back to triangles
		if (!partitionGridShape.cells.empty())
		{
			// Use efficient grid cell detection
			if (partitionGridShape.contains(entityPos))
			{
				isInside = true;
				logger->DebugFormatted("Entity {} at ({}, {}) is INSIDE grid cells during outlier check", entity.ID, entityPos.x, entityPos.y);
			}
		}
		else
		{
			// Fall back to triangle-based detection
			for (const auto &triangle : partitionShape.triangles)
			{
				if (GeometryUtils::PointInTriangle(entityPos, triangle[0], triangle[1], triangle[2]))
				{
					isInside = true;
					logger->DebugFormatted("Entity {} at ({}, {}) is INSIDE triangle during outlier check", entity.ID, entityPos.x, entityPos.y);
					break;
				}
			}
		}

		// If entity is outside all shapes, it's an outlier
		if (!isInside)
		{
			// Only report if we haven't already reported this entity as an outlier
			if (reportedOutliers.find(entity.ID) == reportedOutliers.end())
			{
				outliers.push_back(entity);
				reportedOutliers.insert(entity.ID);
				logger->DebugFormatted("Entity {} at ({}, {}) is OUTSIDE all shapes - marked as outlier during check", entity.ID, entityPos.x, entityPos.y);
			}
		}
		else
		{
			// If entity is now inside, remove it from reported outliers
			reportedOutliers.erase(entity.ID);
		}
	}

	// If we found outliers, store them in database and notify God
	if (!outliers.empty())
	{
		logger->DebugFormatted("OUTLIER DETECTED: Found {} outliers out of {} entities",
							   outliers.size(), managedEntities.size());

		// Use persistent database connection
		if (!database)
		{
			logger->Error("Database connection is null - cannot store outliers");
			return;
		}

		// Test database connection before using it
		if (!database->Exists("connection_test"))
		{
			logger->Error("Database connection test failed - reconnecting");
			database = std::make_unique<RedisCacheDatabase>();
			if (!database->Connect())
			{
				logger->Error("Failed to reconnect to database");
				return;
			}
		}

		// Get our partition ID
		std::string partitionKey = getCurrentPartitionId().ToString();

		// Store outliers in database
		logger->DebugFormatted("ATTEMPTING: Trying to store {} outliers for partition {}", outliers.size(), partitionKey);
		if (EntityManifest::StoreOutliers(database.get(), partitionKey, outliers))
		{
			logger->DebugFormatted("DATABASE: Stored {} outliers for partition {}", outliers.size(), partitionKey);

			// Notify God about outliers
			std::string notifyMsg = "OutliersDetected:" + partitionKey + ":" + std::to_string(outliers.size());
			Interlink::Get().SendMessageRaw(InterLinkIdentifier::MakeIDGod(), std::as_bytes(std::span(notifyMsg)), InterlinkMessageSendFlag::eReliableNow);
			logger->DebugFormatted("GOD NOTIFICATION: Sent outlier message to God: {}", notifyMsg);

			// Update notification timer since we just notified God
			lastOutlierNotification = std::chrono::steady_clock::now();
		}
		else
		{
			logger->ErrorFormatted("DATABASE: Failed to store outliers for partition {}", partitionKey);
			// Try to debug the issue
			logger->ErrorFormatted("DEBUG: Partition key: {}, Outliers count: {}", partitionKey, outliers.size());
			for (const auto &entity : outliers)
			{
				logger->ErrorFormatted("DEBUG: Entity {} at ({}, {})", entity.ID, entity.Position.x, entity.Position.z);
			}
		}
	}
}

void Partition::notifyGodAboutOutliers()
{
	// Check if we have any outliers to report
	if (reportedOutliers.empty())
	{
		return; // No outliers to report
	}

	// Check if enough time has passed since last notification (30 seconds)
	auto now = std::chrono::steady_clock::now();
	auto timeSinceLastNotification = std::chrono::duration_cast<std::chrono::seconds>(now - lastOutlierNotification);

	if (timeSinceLastNotification.count() < 30)
	{
		return; // Not enough time has passed
	}

	// Get our partition ID
	std::string partitionKey = getCurrentPartitionId().ToString();

	// Fetch current outliers from database
	if (!database)
	{
		logger->Error("Database connection is null - cannot fetch outliers for periodic notification");
		return;
	}

	// Test database connection before using it
	if (!database->Exists("connection_test"))
	{
		logger->Error("Database connection test failed during periodic notification - reconnecting");
		database = std::make_unique<RedisCacheDatabase>();
		if (!database->Connect())
		{
			logger->Error("Failed to reconnect to database during periodic notification");
			return;
		}
	}

	std::vector<AtlasEntity> outliers = EntityManifest::FetchOutliers(database.get(), partitionKey);

	if (!outliers.empty())
	{
		logger->DebugFormatted("PERIODIC NOTIFICATION: Sending periodic outlier notification to God for {} outliers", outliers.size());

		// Notify God about outliers
		std::string notifyMsg = "OutliersDetected:" + partitionKey + ":" + std::to_string(outliers.size());
		Interlink::Get().SendMessageRaw(InterLinkIdentifier::MakeIDGod(), std::as_bytes(std::span(notifyMsg)), InterlinkMessageSendFlag::eReliableNow);
		logger->DebugFormatted("PERIODIC NOTIFICATION: Sent outlier message to God: {}", notifyMsg);

		// Update notification timer
		lastOutlierNotification = now;

		// If we've been trying to notify for a long time, be more aggressive
		if (timeSinceLastNotification.count() > 60)
		{
			logger->DebugFormatted("AGGRESSIVE NOTIFICATION: Sending additional outlier notification to ensure God gets the message");
			// Send a second notification after a short delay
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			Interlink::Get().SendMessageRaw(InterLinkIdentifier::MakeIDGod(), std::as_bytes(std::span(notifyMsg)), InterlinkMessageSendFlag::eReliableNow);
		}
	}
}

void Partition::pushManagedEntitiesSnapshot()
{
	// Push every 10 seconds if we have entities
	auto now = std::chrono::steady_clock::now();
	if (std::chrono::duration_cast<std::chrono::seconds>(now - lastEntitiesSnapshotPush).count() < 10)
	{
		return;
	}
	if (!database)
	{
		logger->Error("Database connection test failed during entities snapshot - reconnecting");
		database = std::make_unique<RedisCacheDatabase>();
		if (!database->Connect())
		{
			logger->Error("Failed to reconnect to database during entities snapshot");
			return;
		}
	}

	if (!database)
	{
		logger->Error("Database connection is null - cannot push entities snapshot");
		return;
	}

	// Ensure database is alive

	std::string partitionKey = getCurrentPartitionId().ToString();
	std::replace(partitionKey.begin(), partitionKey.end(), ' ', '_');
		if ((managedEntities.empty() &&EntityManifest::RemoveEntitiesSnapshot(database.get(),partitionKey)) || (!managedEntities.empty() && EntityManifest::StoreEntitiesSnapshot(database.get(), partitionKey, managedEntities)))
		{
			if (!managedEntities.empty())
			{
			logger->DebugFormatted("SNAPSHOT: Pushed {} managed entities to read-only snapshot for {}", managedEntities.size(), partitionKey);

			}
			else
			{
			logger->DebugFormatted("SNAPSHOT: no entities to push, snapshot cleared");

			}
		}
		else
		{
			logger->ErrorFormatted("SNAPSHOT: Failed to push managed entities snapshot for {}", partitionKey);
		}
	lastEntitiesSnapshotPush = now;
}

// ============================================================================
// Helper: ParseEntityPacket
// Validates and extracts AtlasEntities and header type
// ============================================================================
bool Partition::ParseEntityPacket(std::span<const std::byte> data,
								  AtlasNetMessageHeader &outHeader,
								  std::vector<AtlasEntity> &outEntities)
{
	if (data.size() < 1)
		return false;

	// Read header
	outHeader = static_cast<AtlasNetMessageHeader>(data[0]);
	const std::byte *payload = data.data() + 1;
	const size_t payloadSize = data.size() - 1;

	// Check alignment
	if (payloadSize == 0 || payloadSize % sizeof(AtlasEntity) != 0)
		return false;

	const size_t entityCount = payloadSize / sizeof(AtlasEntity);
	outEntities.resize(entityCount);
	std::memcpy(outEntities.data(), payload, payloadSize);

	return true;
}
