#include "pch.hpp"
#include "Demigod.hpp"

int main()
{
    // delay for now, so 
    std::this_thread::sleep_for(std::chrono::seconds(5));
    Demigod demigod;
    demigod.Run();


    return 0;
}