#pragma once

#include "atlasnet/core/assert.hpp"
#include "atlasnet/core/system/isystem.hpp"
#include <boost/container/flat_map.hpp>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <typeindex>
namespace AtlasNet {
class ISystem {
protected:
  static inline std::shared_mutex Mutex;
  static inline boost::container::flat_map<std::type_index,
                                           std::unique_ptr<ISystem>>
      systems;

  virtual void Shutdown() = 0;
  public:
  virtual ~ISystem() = default;
};
template <typename Type> class System : public ISystem {


protected:
  System() = default;

public:
  System(const System &) = delete;
  System &operator=(const System &) = delete;

  // Make: infer constructor args
  template <typename... Args> static Type &Init(Args &&...args) {
    static_assert(std::is_constructible_v<Type, Args...>,
                  "Type is not constructible with the given arguments");

    {

      std::unique_lock lock(Mutex);

      AN_ASSERT(!systems.contains(typeid(Type)), "System already exists");

      systems[typeid(Type)] =
          std::make_unique<Type>(std::forward<Args>(args)...);
    }
    std::shared_lock lock(Mutex);

   return static_cast<Type&>(*systems.at(typeid(Type)));
  }

  [[nodiscard]] static Type &Get() {
    std::shared_lock lock(Mutex);

    if (!systems.contains(typeid(Type))) {

      assert(false && "System not constructed. Call Init(...) first.");
    }

   return static_cast<Type&>(*systems.at(typeid(Type)));
  }
  static bool Exists() {
    std::shared_lock lock(Mutex);
    return static_cast<bool>(systems.contains(typeid(Type)));
  }
};
}; // namespace AtlasNet