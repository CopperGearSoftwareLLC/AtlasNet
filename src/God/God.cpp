#include "God.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
#include "curl/curl.h"
#include <csignal>

God::God() 
{
  // Register signal handlers
  std::signal(SIGINT, handleSignal);
  std::signal(SIGTERM, handleSignal);
  std::signal(SIGKILL, handleSignal);
  std::cout << "[God] Signal handlers registered." << std::endl;
}

God::~God()
{
  //cleanupPartitions();
}

// Signal handler function
void God::handleSignal(int32 signum)
{
  God::Get().cleanupPartitions();
  std::exit(0);
}

std::set<int32> God::getPartitionIDs()
{
    return partitionIds;
}

void* God::getPartition(int32_t id) 
{
    return nullptr;
}

bool God::spawnPartition(int32_t id, int32_t port) 
{
    // Build the shell command to invoke Start.sh
    std::string cmd = "./Start.sh Partition " + std::to_string(id) + " " + std::to_string(port);

    std::cout << "Running: " << cmd << std::endl;

    // Run the script
    int result = std::system(cmd.c_str());
    if (result != 0) {
        std::cerr << "-----------------------" << "Failed to spawn partition " << id << " on port " << port << "-----------------------" << std::endl;
        return false;
    }

    std::cout << "+++++++++++++++++++++++" << "Partition " << id << " started on port " << port << "+++++++++++++++++++++++" << std::endl;
    return true;
}

bool God::removePartition(int32_t id) 
{
    std::string containerName = "partition_" + std::to_string(id);
    std::string cmd = "docker rm -f " + containerName;

    std::cout << "Running: " << cmd << std::endl;

    int result = std::system(cmd.c_str());
    if (result != 0) {
        std::cerr << "-----------------------" << "Failed to remove partition " << id << "-----------------------" << std::endl;
        return false;
    }

    std::cout << "+++++++++++++++++++++++" << "Partition " << id << " removed successfully"<< "+++++++++++++++++++++++" << std::endl;
    return true;
}

bool God::cleanupPartitions() 
{
  std::cout << "[God] Attempting cleanup." << std::endl;
    int result = std::system("docker rm -f $(docker ps -aq --filter name=partition_)");
    if (result != 0) {
        std::cerr << "-----------------------" << "Failed to clean up partitions\n" << "-----------------------" << std::endl;
        return false;
    }
    return true;
}
