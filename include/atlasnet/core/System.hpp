#pragma once

#include "atlasnet/core/assert.hpp"
#include "atlasnet/core/system/isystem.hpp"
#include <boost/container/flat_map.hpp>
#include <cstddef>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <typeindex>
namespace AtlasNet
{
class ISystem
{
protected:
  static inline std::shared_mutex Mutex;
  static inline boost::container::flat_map<std::type_index,
                                           std::unique_ptr<ISystem>>
      systems;

  virtual void Shutdown() = 0;

public:
  static void ShutdownAll()
  {
    std::unique_lock lock(Mutex);
    for (auto& [type, system] : systems)
    {
      system->Shutdown();
    }
    systems.clear();
  }
  virtual ~ISystem() = default;
};
template <typename Type> class System : public ISystem
{

protected:
  System() = default;

public:
  System(const System&) = delete;
  System& operator=(const System&) = delete;

  // Make: infer constructor args
  template <typename... Args> static Type& Init(Args&&... args)
  {
    static_assert(std::is_constructible_v<Type, Args...>,
                  "Type is not constructible with the given arguments");

    // Construct outside lock to avoid re-entrant deadlock via Get()/Init() in
    // ctor.

    Type* data = reinterpret_cast<Type*>(new std::byte[sizeof(Type)]);
    std::unique_ptr<Type> instance = std::unique_ptr<Type>(data);
    {
      std::unique_lock lock(Mutex);

      if (systems.contains(typeid(Type)))
      {
        assert(false && "System already initialized");
        return *static_cast<Type*>(systems.at(typeid(Type)).get());
      }
      auto [it, inserted] =
          systems.try_emplace(typeid(Type), std::move(instance));
    }
    std::construct_at(data, std::forward<Args>(args)...);
    return *data;
  }

  [[nodiscard]] static Type& Get()
  {
    std::shared_lock lock(Mutex);

    if (!systems.contains(typeid(Type)))
    {

      assert(false && "System not constructed. Call Init(...) first.");
    }

    return static_cast<Type&>(*systems.at(typeid(Type)));
  }
  static bool Exists()
  {
    std::shared_lock lock(Mutex);
    return static_cast<bool>(systems.contains(typeid(Type)));
  }
};
}; // namespace AtlasNet