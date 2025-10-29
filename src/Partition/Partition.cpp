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
	if (!database->Connect()) {
		logger->Error("Failed to connect to database in Partition constructor");
	}
	
	// Initialize notification timer
	lastOutlierNotification = std::chrono::steady_clock::now();
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
						  .OnConnectedCallback = [](const InterLinkIdentifier &Connection) {},
						  .OnMessageArrival = [](const Connection &fromWhom, std::span<const std::byte> data) {Partition::Get().MessageArrived(fromWhom,data);}}});

	// Clear any existing partition entity data to prevent stale data
	std::string partitionKey = partitionIdentifier.ToString();
	PartitionEntityManifest::ClearPartition(database.get(), partitionKey);
	
	// Send initial test message to God
	std::string testMessage = "Hello This is a test message from " + partitionIdentifier.ToString();
	Interlink::Get().SendMessageRaw(InterLinkIdentifier::MakeIDGod(), std::as_bytes(std::span(testMessage)));
	while (!ShouldShutdown)
	{
		// Continuously check for outliers and notify God if found
		checkForOutliersAndNotifyGod();
		
		// Periodically notify God about outliers (every 30 seconds)
		notifyGodAboutOutliers();
		
		Interlink::Get().Tick();
		std::this_thread::sleep_for(std::chrono::seconds(5));
	}

	logger->Debug("Shutting Down");
	Interlink::Get().Shutdown();
}

void Partition::MessageArrived(const Connection &fromWhom, std::span<const std::byte> data)
{
    // Detect snapshot packets
    if (data.size() % sizeof(AtlasEntity) == 0 && data.size() > 0)
    {
        size_t entityCount = data.size() / sizeof(AtlasEntity);
        std::vector<AtlasEntity> snapshot(entityCount);

        std::memcpy(snapshot.data(), data.data(), data.size());
        // Store or apply to in-memory state
        this->CachedEntities = std::move(snapshot);

        logger->DebugFormatted("Cached {} entities from {}", entityCount, fromWhom.target.ID);

        for (const auto &entity : this->CachedEntities)
        {
            logger->DebugFormatted("Entity ID: {}, Pos: ({}, {}, {})", entity.ID, entity.Position.x, entity.Position.y, entity.Position.z);
        }

        return;
    }

	std::string msg(reinterpret_cast<const char*>(std::data(data)), std::size(data));
	
	// Debug: log all incoming messages
	logger->DebugFormatted("MESSAGE RECEIVED: '{}' (length: {})", msg, msg.length());
	
	// Handle fetch outlier entity message
	if (msg.starts_with("FetchOutlierEntity:")) {
		// Parse message format: "FetchOutlierEntity:sourcePartition:entityId"
		size_t firstColon = msg.find(':', 17); // After "FetchOutlierEntity:"
		if (firstColon == std::string::npos) {
			logger->Error("Invalid FetchOutlierEntity message format");
			return;
		}
		
		std::string sourcePartition = msg.substr(17, firstColon - 17);
		std::string entityIdStr = msg.substr(firstColon + 1);
		
		// Validate entity ID string before conversion
		if (entityIdStr.empty()) {
			logger->ErrorFormatted("Empty entity ID in FetchOutlierEntity message: {}", msg);
			return;
		}
		
		// Check if string contains only digits
		if (entityIdStr.find_first_not_of("0123456789") != std::string::npos) {
			logger->ErrorFormatted("Invalid entity ID format (non-numeric): '{}' in message: {}", entityIdStr, msg);
			return;
		}
		
		AtlasEntityID entityId;
		try {
			entityId = std::stoi(entityIdStr);
		} catch (const std::invalid_argument& e) {
			logger->ErrorFormatted("Invalid entity ID format in FetchOutlierEntity message: '{}' - Error: {}", entityIdStr, e.what());
			return;
		} catch (const std::out_of_range& e) {
			logger->ErrorFormatted("Entity ID out of range in FetchOutlierEntity message: '{}' - Error: {}", entityIdStr, e.what());
			return;
		}
		
		// Use persistent database connection
		if (!database) {
			logger->Error("Database connection is null - cannot fetch entity");
			return;
		}

		// Fetch entity from source partition's outliers
		std::vector<AtlasEntity> outliers = EntityManifest::FetchOutliers(database.get(), sourcePartition);
		
		// Find the specific entity in outliers
		auto it = std::find_if(outliers.begin(), outliers.end(),
			[entityId](const AtlasEntity& e) { return e.ID == entityId; });
			
		if (it != outliers.end()) {
			// Add to our managed entities
			managedEntities.push_back(*it);
			logger->DebugFormatted("FETCHED: Added entity {} to managed entities", entityId);
			
			// Update partition manifest with new entity
			std::string partitionKey = getCurrentPartitionId().ToString();
			if (!PartitionEntityManifest::AddEntity(database.get(), partitionKey, *it)) {
				logger->ErrorFormatted("Failed to add entity {} to partition manifest", entityId);
			} else {
				logger->DebugFormatted("PARTITION MANIFEST: Added entity {} to partition manifest", entityId);
			}
			
			// Remove from our reported outliers set if it was there
			reportedOutliers.erase(entityId);
			
			logger->DebugFormatted("REDISTRIBUTION COMPLETE: Successfully fetched entity {} from {}'s outliers", entityId, sourcePartition);
		} else {
			logger->ErrorFormatted("Could not find entity {} in {}'s outliers", entityId, sourcePartition);
		}
	}
	else if(msg.starts_with("RemoveEntity:"))
	{
		// Parse message format: "RemoveEntity:entityId"
		try {
			std::string entityIdStr = msg.substr(13); // After "RemoveEntity:" (13 characters)
			
			// Trim whitespace and check for valid characters
			entityIdStr.erase(0, entityIdStr.find_first_not_of(" \t\r\n"));
			entityIdStr.erase(entityIdStr.find_last_not_of(" \t\r\n") + 1);
			
			if (entityIdStr.empty()) {
				logger->ErrorFormatted("Empty entity ID in RemoveEntity message: {}", msg);
				return;
			}
			
			// Check if string contains only digits
			if (entityIdStr.find_first_not_of("0123456789") != std::string::npos) {
				logger->ErrorFormatted("Invalid entity ID format (non-numeric): '{}' in message: {}", entityIdStr, msg);
				return;
			}
			
			AtlasEntityID entityId;
			try {
				entityId = std::stoi(entityIdStr);
			} catch (const std::invalid_argument& e) {
				logger->ErrorFormatted("Invalid entity ID format in RemoveEntity message: '{}' - Error: {}", entityIdStr, e.what());
				return;
			} catch (const std::out_of_range& e) {
				logger->ErrorFormatted("Entity ID out of range in RemoveEntity message: '{}' - Error: {}", entityIdStr, e.what());
				return;
			}
		
		// Find and remove entity from our managed entities
		auto it = std::find_if(managedEntities.begin(), managedEntities.end(),
			[entityId](const AtlasEntity& e) { return e.ID == entityId; });
			
		if (it != managedEntities.end()) {
			// Get entity info before removing it
			AtlasEntity entity = *it;
			managedEntities.erase(it);
			logger->DebugFormatted("REMOVED: Removed entity {} from managed entities", entityId);
			
			// Update partition manifest to remove entity
			std::string partitionKey = getCurrentPartitionId().ToString();
			if (!PartitionEntityManifest::RemoveEntity(database.get(), partitionKey, entityId)) {
				logger->ErrorFormatted("Failed to remove entity {} from partition manifest", entityId);
			} else {
				logger->DebugFormatted("PARTITION MANIFEST: Removed entity {} from partition manifest", entityId);
			}
			
			// Send entity info back to God for redistribution
			std::string entityInfoMsg = "EntityRemoved:" + 
				InterLinkIdentifier(InterlinkType::ePartition, DockerIO::Get().GetSelfContainerName()).ToString() + ":" +
				std::to_string(entity.ID) + ":" +
				std::to_string(entity.Position.x) + ":" +
				std::to_string(entity.Position.y) + ":" +
				std::to_string(entity.Position.z);
			
			Interlink::Get().SendMessageRaw(InterLinkIdentifier::MakeIDGod(), std::as_bytes(std::span(entityInfoMsg)));
			logger->DebugFormatted("REDISTRIBUTION: Sent entity info back to God: {}", entityInfoMsg);
		} else {
			logger->DebugFormatted("ENTITY NOT FOUND: Entity {} not in managed entities (may have been already redistributed)", entityId);
			// Send a dummy response to God to acknowledge the request
			// This prevents God from waiting indefinitely for a response
			std::string entityInfoMsg = "EntityRemoved:" + 
				InterLinkIdentifier(InterlinkType::ePartition, DockerIO::Get().GetSelfContainerName()).ToString() + ":" +
				std::to_string(entityId) + ":0.0:0.0:0.0"; // Dummy position since entity not found
			
			Interlink::Get().SendMessageRaw(InterLinkIdentifier::MakeIDGod(), std::as_bytes(std::span(entityInfoMsg)));
			logger->DebugFormatted("ACKNOWLEDGED: Sent dummy response to God for missing entity {}", entityId);
		}
		} catch (const std::exception& e) {
			logger->ErrorFormatted("Error parsing RemoveEntity message: {} - Message: '{}'", e.what(), msg);
		}
	}
	else if(msg == "Fetch Shape")
	{
		// Use persistent database connection
		if (!database) {
			logger->Error("Database connection is null - cannot fetch shape");
			return;
		}

		// The key in the database uses our partition ID
		std::string key = getCurrentPartitionId().ToString();
		
		// Try to fetch our shape data
		std::optional<std::string> shapeData = ShapeManifest::Fetch(database.get(), key);
		if (!shapeData.has_value())
		{
			logger->ErrorFormatted("No shape found for partition {}", key);
			return;
		}

		// Parse the shape data and construct our shape
		try 
		{
			Shape shape;
			std::string data = shapeData.value();
			
			// Parse metadata first
			size_t shapeIdStart = data.find("shape_id:");
			if (shapeIdStart != std::string::npos) {
				size_t shapeIdEnd = data.find(";", shapeIdStart);
				if (shapeIdEnd != std::string::npos) {
					std::string shapeId = data.substr(shapeIdStart + 9, shapeIdEnd - (shapeIdStart + 9));
					logger->DebugFormatted("Loading shape ID: {}", shapeId);
				}
			}

			// Find the triangles section
			size_t trianglesStart = data.find("triangles:");
			if (trianglesStart == std::string::npos)
			{
				logger->Error("Invalid shape data format - no triangles section");
				return;
			}

			// Parse the triangle vertices
			size_t pos = trianglesStart + 9; // Skip "triangles:"
			while (pos < data.length())
			{
				// Find next triangle marker
				pos = data.find("triangle:", pos);
				if (pos == std::string::npos) break;
				pos += 9; // Skip "triangle:"

				Triangle triangle;
				bool validTriangle = true;
				
				// Parse 3 vertices for this triangle
				for (int i = 0; i < 3; i++)
				{
					size_t vStart = data.find("v(", pos);
					if (vStart == std::string::npos) {
						validTriangle = false;
						break;
					}
					
					size_t comma = data.find(",", vStart);
					size_t end = data.find(")", vStart);
					if (comma == std::string::npos || end == std::string::npos) {
						validTriangle = false;
						break;
					}
					
					try {
						float x = std::stof(data.substr(vStart + 2, comma - (vStart + 2)));
						float y = std::stof(data.substr(comma + 1, end - (comma + 1)));
						triangle[i] = vec2(x, y);
						logger->DebugFormatted("Parsed vertex {}: ({}, {})", i, x, y);
						pos = end + 1;
					} catch (const std::exception& e) {
						logger->ErrorFormatted("Error parsing vertex: {}", e.what());
						validTriangle = false;
						break;
					}
				}
				
				if (validTriangle) {
					shape.triangles.push_back(triangle);
					logger->Debug("Added triangle to shape");
				} else {
					logger->Error("Failed to parse triangle vertices");
				}
			}

		// Assign the shape to our member variable
		partitionShape = std::move(shape);
		logger->DebugFormatted("Successfully loaded shape with {} triangles", partitionShape.triangles.size());
		
		// Debug: Log triangle coordinates for verification
		for (size_t i = 0; i < partitionShape.triangles.size(); ++i) {
			const auto& tri = partitionShape.triangles[i];
			logger->DebugFormatted("Triangle {}: ({}, {}) ({}, {}) ({}, {})", 
				i, tri[0].x, tri[0].y, tri[1].x, tri[1].y, tri[2].x, tri[2].y);
		}

		// Clear reported outliers since we have a new shape
		reportedOutliers.clear();
		}
		catch (const std::exception& e)
		{
			logger->ErrorFormatted("Error parsing shape data: {}", e.what());
			return;
		}
	}
	else if(msg == "Fetch Grid Shape")
	{
		// Use persistent database connection
		if (!database) {
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
			for (size_t i = 0; i < partitionGridShape.cells.size(); ++i) {
				const auto& cell = partitionGridShape.cells[i];
				logger->DebugFormatted("Grid Cell {}: ({}, {}) to ({}, {}) [row={}, col={}]", 
					i, cell.min.x, cell.min.y, cell.max.x, cell.max.y, cell.row, cell.col);
			}

			// Clear reported outliers since we have a new shape
			reportedOutliers.clear();
		}
		catch (const std::exception& e)
		{
			logger->ErrorFormatted("Error parsing grid shape data: {}", e.what());
			return;
		}
	}
}

void Partition::checkForOutliersAndNotifyGod()
{
	// Only check if we have a valid shape and entities
	if ((partitionShape.triangles.empty() && partitionGridShape.cells.empty()) || managedEntities.empty()) {
		return;
	}

	// Use shared geometry utility

	// Check all entities for outliers
	std::vector<AtlasEntity> outliers;
	for (const auto& entity : managedEntities) {
		vec2 entityPos{entity.Position.x, entity.Position.z};
		bool isInside = false;
		
		// Use grid cells if available (much faster), otherwise fall back to triangles
		if (!partitionGridShape.cells.empty()) {
			// Use efficient grid cell detection
			if (partitionGridShape.contains(entityPos)) {
				isInside = true;
				logger->DebugFormatted("Entity {} at ({}, {}) is INSIDE grid cells during outlier check", entity.ID, entityPos.x, entityPos.y);
			}
		} else {
			// Fall back to triangle-based detection
			for (const auto& triangle : partitionShape.triangles) {
				if (GeometryUtils::PointInTriangle(entityPos, triangle[0], triangle[1], triangle[2])) {
					isInside = true;
					logger->DebugFormatted("Entity {} at ({}, {}) is INSIDE triangle during outlier check", entity.ID, entityPos.x, entityPos.y);
					break;
				}
			}
		}
		
		// If entity is outside all shapes, it's an outlier
		if (!isInside) {
			// Only report if we haven't already reported this entity as an outlier
			if (reportedOutliers.find(entity.ID) == reportedOutliers.end()) {
				outliers.push_back(entity);
				reportedOutliers.insert(entity.ID);
				logger->DebugFormatted("Entity {} at ({}, {}) is OUTSIDE all shapes - marked as outlier during check", entity.ID, entityPos.x, entityPos.y);
			}
		} else {
			// If entity is now inside, remove it from reported outliers
			reportedOutliers.erase(entity.ID);
		}
	}

	// If we found outliers, store them in database and notify God
	if (!outliers.empty()) {
		logger->DebugFormatted("OUTLIER DETECTED: Found {} outliers out of {} entities", 
			outliers.size(), managedEntities.size());
		
		// Use persistent database connection
		if (!database) {
			logger->Error("Database connection is null - cannot store outliers");
			return;
		}
		
		// Test database connection before using it
		if (!database->Exists("connection_test")) {
			logger->Error("Database connection test failed - reconnecting");
			database = std::make_unique<RedisCacheDatabase>();
			if (!database->Connect()) {
				logger->Error("Failed to reconnect to database");
				return;
			}
		}

		// Get our partition ID
		std::string partitionKey = getCurrentPartitionId().ToString();

		// Store outliers in database
		logger->DebugFormatted("ATTEMPTING: Trying to store {} outliers for partition {}", outliers.size(), partitionKey);
		if (EntityManifest::StoreOutliers(database.get(), partitionKey, outliers)) {
			logger->DebugFormatted("DATABASE: Stored {} outliers for partition {}", outliers.size(), partitionKey);
			
			// Notify God about outliers
			std::string notifyMsg = "OutliersDetected:" + partitionKey + ":" + std::to_string(outliers.size());
			Interlink::Get().SendMessageRaw(InterLinkIdentifier::MakeIDGod(), std::as_bytes(std::span(notifyMsg)));
			logger->DebugFormatted("GOD NOTIFICATION: Sent outlier message to God: {}", notifyMsg);
			
			// Update notification timer since we just notified God
			lastOutlierNotification = std::chrono::steady_clock::now();
		} else {
			logger->ErrorFormatted("DATABASE: Failed to store outliers for partition {}", partitionKey);
			// Try to debug the issue
			logger->ErrorFormatted("DEBUG: Partition key: {}, Outliers count: {}", partitionKey, outliers.size());
			for (const auto& entity : outliers) {
				logger->ErrorFormatted("DEBUG: Entity {} at ({}, {})", entity.ID, entity.Position.x, entity.Position.z);
			}
		}
	}
	// No need to log when no outliers are found - this is the normal case
}

void Partition::notifyGodAboutOutliers()
{
	// Check if we have any outliers to report
	if (reportedOutliers.empty()) {
		return; // No outliers to report
	}
	
	// Check if enough time has passed since last notification (30 seconds)
	auto now = std::chrono::steady_clock::now();
	auto timeSinceLastNotification = std::chrono::duration_cast<std::chrono::seconds>(now - lastOutlierNotification);
	
	if (timeSinceLastNotification.count() < 30) {
		return; // Not enough time has passed
	}
	
	// Get our partition ID
	std::string partitionKey = getCurrentPartitionId().ToString();
	
	// Fetch current outliers from database
	if (!database) {
		logger->Error("Database connection is null - cannot fetch outliers for periodic notification");
		return;
	}
	
	// Test database connection before using it
	if (!database->Exists("connection_test")) {
		logger->Error("Database connection test failed during periodic notification - reconnecting");
		database = std::make_unique<RedisCacheDatabase>();
		if (!database->Connect()) {
			logger->Error("Failed to reconnect to database during periodic notification");
			return;
		}
	}
	
	std::vector<AtlasEntity> outliers = EntityManifest::FetchOutliers(database.get(), partitionKey);
	
	if (!outliers.empty()) {
		logger->DebugFormatted("PERIODIC NOTIFICATION: Sending periodic outlier notification to God for {} outliers", outliers.size());
		
		// Notify God about outliers
		std::string notifyMsg = "OutliersDetected:" + partitionKey + ":" + std::to_string(outliers.size());
		Interlink::Get().SendMessageRaw(InterLinkIdentifier::MakeIDGod(), std::as_bytes(std::span(notifyMsg)));
		logger->DebugFormatted("PERIODIC NOTIFICATION: Sent outlier message to God: {}", notifyMsg);
		
		// Update notification timer
		lastOutlierNotification = now;
		
		// If we've been trying to notify for a long time, be more aggressive
		if (timeSinceLastNotification.count() > 60) {
			logger->DebugFormatted("AGGRESSIVE NOTIFICATION: Sending additional outlier notification to ensure God gets the message");
			// Send a second notification after a short delay
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			Interlink::Get().SendMessageRaw(InterLinkIdentifier::MakeIDGod(), std::as_bytes(std::span(notifyMsg)));
		}
	}
}
