#include "GameCoordinator.hpp"
#include "pch.hpp"

int main()
{
    //std::this_thread::sleep_for(std::chrono::seconds(11));
    GameCoordinator::Get().Init();
    GameCoordinator::Get().Run();
    return 0;
}