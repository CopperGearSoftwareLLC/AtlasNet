#include "Partition/Partition.hpp"
#include "TestUnityAPI/TestPartition.hpp"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "pch.hpp"

int main(void)
{
    //Partition part;
    //part.Run();

    TestPartition fp{};
    bool state = fp.start();

    std::cerr << "[TestPartition] Success?: " << state << "\n";

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
