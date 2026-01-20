#include "God.hpp"
#include "EntityManifest.hpp"
#include "ServerRegistry.hpp"
#include "InterlinkEnums.hpp"

// ============================================================================
// handleOutliersDetectedMessage
// Parses and handles OutliersDetected messages from partitions
// 
// Message format: "OutliersDetected:<partitionId>:<count>"
// Example: "OutliersDetected:ePartition partition1:5"
// 
// When a partition detects entities that are outside its boundaries, it sends
// this message to notify God. God then processes these outliers and redistributes
// them to the correct partitions.
// ============================================================================
void God::handleOutliersDetectedMessage(const std::string& message)
{
  // Find the last colon which separates the partition ID from the count
  size_t lastColon = message.rfind(':');
  // Message format is "OutliersDetected:" (17 chars) + partitionId + ":" + count
  // So lastColon must exist and be after the prefix
  if (lastColon == std::string::npos || lastColon == 16) {
    logger->Error("Invalid OutliersDetected message format - missing count");
    return;
  }

  // Extract partition ID: everything between "OutliersDetected:" (17 chars) and last colon
  std::string partitionId = message.substr(17, lastColon - 17);
  // Extract count: everything after the last colon
  std::string countStr = message.substr(lastColon + 1);
  
  // Validate count string is not empty
  if (countStr.empty()) {
    logger->ErrorFormatted("Empty outlier count in OutliersDetected message: {}", message);
    return;
  }
  
  // Validate count contains only digits
  if (countStr.find_first_not_of("0123456789") != std::string::npos) {
    logger->ErrorFormatted("Invalid outlier count format (non-numeric): '{}' in message: {}", countStr, message);
    return;
  }

  // Convert count string to integer with error handling
  int outlierCount;
  try {
    outlierCount = std::stoi(countStr);
  } catch (const std::invalid_argument& e) {
    logger->ErrorFormatted("Invalid outlier count format in OutliersDetected message: '{}' - Error: {}", countStr, e.what());
    return;
  } catch (const std::out_of_range& e) {
    logger->ErrorFormatted("Outlier count out of range in OutliersDetected message: '{}' - Error: {}", countStr, e.what());
    return;
  }

  logger->DebugFormatted("Received outlier notification from {}: {} outliers", partitionId, outlierCount);
  
  // Process the outliers: find correct partitions and redistribute entities
  processOutliersForPartition(partitionId);
}

// ============================================================================
// handleEntityRemovedMessage
// Parses and handles EntityRemoved messages from partitions
// 
// Message format: "EntityRemoved:<sourcePartition>:<entityId>:<x>:<y>:<z>"
// Example: "EntityRemoved:ePartition partition1:42:0.5:0.0:0.7"
// 
// When God requests a partition to remove an entity (for redistribution), the
// partition sends this message back to confirm removal and provide the entity's
// final position. God then redistributes the entity to the correct partition.
// ============================================================================
void God::handleEntityRemovedMessage(const std::string& message)
{
  // Find all colon positions in the message to parse the delimited format
  std::vector<size_t> colonPositions;
  size_t pos = 0;
  while ((pos = message.find(':', pos + 1)) != std::string::npos) {
    colonPositions.push_back(pos);
  }

  logger->DebugFormatted("DEBUG: EntityRemoved message: '{}'", message);
  logger->DebugFormatted("DEBUG: Found {} colons at positions: ", colonPositions.size());
  for (size_t i = 0; i < colonPositions.size(); ++i) {
    logger->DebugFormatted("DEBUG: Colon {} at position {}", i, colonPositions[i]);
  }

  // Message format requires 5 colons: after "EntityRemoved:", partition, entityId, x, y, z
  if (colonPositions.size() < 5) {
    logger->ErrorFormatted("Invalid EntityRemoved message format - not enough colons (found {}, need 5)", colonPositions.size());
    return;
  }

  // Extract positions of each colon from the end (they're in order: partition, entityId, x, y, z)
  size_t zColon = colonPositions[colonPositions.size() - 1]; // Last colon before z coordinate
  size_t yColon = colonPositions[colonPositions.size() - 2]; // Second-to-last colon before y coordinate
  size_t xColon = colonPositions[colonPositions.size() - 3]; // Third-to-last colon before x coordinate
  size_t entityIdColon = colonPositions[colonPositions.size() - 4]; // Fourth-to-last colon before entityId
  size_t partitionIdColon = colonPositions[colonPositions.size() - 5]; // Fifth-to-last colon before partition ID

  // Extract source partition: everything between "EntityRemoved:" (14 chars) and partitionIdColon
  std::string sourcePartition = message.substr(14, partitionIdColon - 14);

  // Extract entity ID: everything between entityIdColon and xColon
  std::string entityIdStr = message.substr(entityIdColon + 1, xColon - (entityIdColon + 1));
  if (entityIdStr.empty()) {
    logger->ErrorFormatted("Empty entity ID in EntityRemoved message: {}", message);
    return;
  }
  // Validate entity ID contains only digits
  if (entityIdStr.find_first_not_of("0123456789") != std::string::npos) {
    logger->ErrorFormatted("Invalid entity ID format (non-numeric): '{}' in message: {}", entityIdStr, message);
    return;
  }

  // Convert entity ID string to integer with error handling
  AtlasEntityID entityId;
  try {
    entityId = std::stoi(entityIdStr);
  } catch (const std::invalid_argument& e) {
    logger->ErrorFormatted("Invalid entity ID format in EntityRemoved message: '{}' - Error: {}", entityIdStr, e.what());
    return;
  } catch (const std::out_of_range& e) {
    logger->ErrorFormatted("Entity ID out of range in EntityRemoved message: '{}' - Error: {}", entityIdStr, e.what());
    return;
  }

  // Lambda function to parse float coordinates (x, y, z) with validation
  // Validates that the coordinate string contains only digits, decimal point, and minus sign
  auto parseFloatCoordinate = [&](const std::string& coordStr, const std::string& coordName) -> std::optional<float> {
    if (coordStr.empty()) {
      logger->ErrorFormatted("Empty {} coordinate in EntityRemoved message: {}", coordName, message);
      return std::nullopt;
    }
    // Check for invalid characters (only allow digits, decimal point, minus sign)
    if (coordStr.find_first_not_of("0123456789.-") != std::string::npos) {
      logger->ErrorFormatted("Invalid {} coordinate format: '{}' in message: {}", coordName, coordStr, message);
      return std::nullopt;
    }
    try {
      return std::stof(coordStr);
    } catch (const std::invalid_argument& e) {
      logger->ErrorFormatted("Invalid {} coordinate format in EntityRemoved message: '{}' - Error: {}", coordName, coordStr, e.what());
      return std::nullopt;
    } catch (const std::out_of_range& e) {
      logger->ErrorFormatted("{} coordinate out of range in EntityRemoved message: '{}' - Error: {}", coordName, coordStr, e.what());
      return std::nullopt;
    }
  };

  // Extract coordinate strings from the message
  std::string xStr = message.substr(xColon + 1, yColon - (xColon + 1));
  std::string yStr = message.substr(yColon + 1, zColon - (yColon + 1));
  std::string zStr = message.substr(zColon + 1);

  // Parse all three coordinates
  auto xOpt = parseFloatCoordinate(xStr, "X");
  auto yOpt = parseFloatCoordinate(yStr, "Y");
  auto zOpt = parseFloatCoordinate(zStr, "Z");
  
  // Ensure all coordinates were parsed successfully
  if (!xOpt || !yOpt || !zOpt) {
    logger->ErrorFormatted("Failed to parse coordinates in EntityRemoved message: {}", message);
    return;
  }

  float x = *xOpt;
  float y = *yOpt;
  float z = *zOpt;

  logger->DebugFormatted("Received entity removal confirmation: Entity {} from {} at ({}, {}, {})", 
    entityId, sourcePartition, x, y, z);

  // Reconstruct the entity with its position for redistribution
  AtlasEntity entity;
  entity.ID = entityId;
  entity.Position = vec3{x, y, z};

  // Redistribute the entity to the correct partition based on its position
  redistributeEntityToCorrectPartition(entity, sourcePartition);
}

// ============================================================================
// handleOutlierMessage
// Routes outlier-related messages to the appropriate handler
// 
// This is the entry point for all messages related to entity outliers and
// redistribution. It checks the message prefix and routes to the correct
// handler function.
// 
// Supported message types:
// - "OutliersDetected:..." -> handleOutliersDetectedMessage
// - "EntityRemoved:..." -> handleEntityRemovedMessage
// ============================================================================
void God::handleOutlierMessage(const std::string& message)
{
  // Route to appropriate handler based on message prefix
  if (message.starts_with("OutliersDetected:")) {
    handleOutliersDetectedMessage(message);
  } else if (message.starts_with("EntityRemoved:")) {
    handleEntityRemovedMessage(message);
  }
}


