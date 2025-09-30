#include "Globals.hpp"
#include "God/God.hpp"
#include "Partition/Partition.hpp"
#include "pch.hpp"
int main(int argc, char** argv) {
    std::cerr << "Hello\n";
    std::cerr << "From\n";
    std::cerr << "God\n";
    for (int i = 0; i < argc; i++) {
	std::cerr << argv[i] << std::endl;
    }

    God& god = God::Get();

    int32 port = 7000;
    // Example: spawn 4 partitions
    for (int32 i = 1; i <= 4; i++) {
	++port;
	god.spawnPartition(i, port);
    }

    // std::this_thread::sleep_for(std::chrono::seconds(4));
    god.removePartition(4);

    for (size_t i = 4; i < 8; i++) {
	++port;
	god.spawnPartition(i, port);
    }
    return 0;
}

/*
int main(int argc, char **argv) {
    std::cerr << "Hello\n";
    std::cerr << "From\n";
    std::cerr << "God\n";

    for (int i = 0; i < argc; i++) {
	std::cerr << argv[i] << std::endl;
    }


}
*/