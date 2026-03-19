
#include "RTSServer.hpp"

#include <chrono>
#include <iostream>
#include <thread>

#include "AtlasNetServer.hpp"
#include "Heuristic/BoundLeaser.hpp"
int main()
{
	RTSServer::Get().Run();
}
RTSServer::RTSServer() {}
void RTSServer::Run()
{
	AtlasNet_Initialize();
	while (!BoundLeaser::Get().HasBound())
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	using Clock = std::chrono::steady_clock;

	auto lastTime = Clock::now();
	auto fpsTimer = Clock::now();

	constexpr double TargetFPS = 60.0;
	constexpr std::chrono::duration<double> TargetFrameTime(1.0 / TargetFPS);

	constexpr float MinX = -100.f;
	constexpr float MaxX = 100.f;
	constexpr float MinY = -100.f;
	constexpr float MaxY = 100.f;

	uint64_t frameCount = 0;
	double accumulatedTime = 0.0;
	while (!ShouldShutdown)
	{
		const auto frameStart = Clock::now();

		const std::chrono::duration<double> delta = frameStart - lastTime;
		lastTime = frameStart;

		const double dt = delta.count();  // seconds
		accumulatedTime += dt;
		frameCount++;

		GetCommandBus().Flush();
		// ---- FPS print every second ----
		const auto now = Clock::now();
		const std::chrono::duration<double> fpsElapsed = now - fpsTimer;

		if (fpsElapsed.count() >= 1.0)
		{
			const double avgFPS = frameCount / accumulatedTime;
			// std::cout << "Active FPS: " << avgFPS << std::endl;

			frameCount = 0;
			accumulatedTime = 0.0;
			fpsTimer = now;
		}

		// ---- Frame limiting to 60 FPS ----
		const auto frameEnd = Clock::now();
		const std::chrono::duration<double> frameTime = frameEnd - frameStart;

		if (frameTime < TargetFrameTime)
		{
			std::this_thread::sleep_for(TargetFrameTime - frameTime);
		}
	}
}
