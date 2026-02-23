#pragma once
#include <cassert>
#include <mutex>
#include <memory>
template <typename Type>
class Singleton
{
private:
    static inline std::mutex Mutex;
    static inline std::unique_ptr<Type> Instance;

protected:
    Singleton() = default;

public:
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

    // Get: constructs ONLY if default-constructible
    [[nodiscard]] static Type& Get()
    {
        std::lock_guard lock(Mutex);

        if (!Instance)
        {
            if constexpr (std::is_default_constructible_v<Type>)
            {
                Instance = std::make_unique<Type>();
            }
            else
            {
                assert(false&& "Singleton not constructed. Call Make(...) first.");
            }
        }

        return *Instance;
    }

    // Make: infer constructor args
    template <typename... Args>
    static Type& Make(Args&&... args)
    {
        static_assert(
            std::is_constructible_v<Type, Args...>,
            "Type is not constructible with the given arguments"
        );

        std::lock_guard lock(Mutex);
        assert(!Instance&&"Singleton already exists");

        Instance = std::make_unique<Type>(std::forward<Args>(args)...);
        return *Instance;
    }

    static bool Exists()
    {
        std::lock_guard lock(Mutex);
        return static_cast<bool>(Instance);
    }

    static void Destroy()
    {
        std::lock_guard lock(Mutex);
        Instance.reset();
    }
};

