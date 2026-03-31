#pragma once

#include "atlasnet/core/system/isystem.hpp"
#include "boost/container/flat_map.hpp"
#include <concepts>
#include <memory>
#include <typeindex>
#include <vector>
class SystemWorker {
public:
  SystemWorker() = default;
  virtual ~SystemWorker() = default;

  template <typename T, typename... Args>
    requires std::derived_from<T, ISystem>
  T &AddSystem(Args &&...args) {
    static_assert(std::is_base_of_v<ISystem, T>, "T must derive from ISystem");
    auto system = std::make_unique<T>(std::forward<Args>(args)...);
    system->Run();
    systemMap_[typeid(T)] = std::move(system);
    return *static_cast<T *>(systemMap_[typeid(T)].get());
  }

  template <typename T>
    requires std::derived_from<T, ISystem>
  T &GetSystem() {
    static_assert(std::is_base_of_v<ISystem, T>, "T must derive from ISystem");
    auto it = systemMap_.find(typeid(T));
    if (it != systemMap_.end()) {
      return *static_cast<T *>(it->second.get());
    }

    throw std::runtime_error("System not found");
  }

  void RunAllSystems() {
    for (auto &[type, system] : systemMap_) {
      system->Run();
    }
  }
  void StopAllSystems() {
    for (auto &[type, system] : systemMap_) {
      system->Stop();
    }
  }

private:
  boost::container::flat_map<std::type_index, std::unique_ptr<ISystem>>
      systemMap_;
};