#include "DockerEvents.hpp"

void DockerEvents::Init(const DockerEventsInit &init)
{
  init_vars = init;

  std::signal(SIGINT, [](SignalType id)
              { init_vars.OnShutdownRequest(id); });
  std::signal(SIGTERM, [](SignalType id)
              { init_vars.OnShutdownRequest(id); });
}
