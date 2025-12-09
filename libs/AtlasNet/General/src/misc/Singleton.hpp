#pragma once
#include <pch.hpp>

template <typename Type, typename ConstructorArguments = void> class Singleton
{
  private:
    static inline std::unique_ptr<Type> Instance;


  public:
    Singleton() = default;
    Singleton(const Singleton& o) = delete;  
  // if constructor arguments
    template <typename T = ConstructorArguments, std::enable_if_t<!std::is_void_v<T>, int> = 0>
    [[nodiscard]] static Type &Get()
    {
        ASSERT(Instance, "Singleton Not Set");
        return *Instance;
    }

    template <typename T = ConstructorArguments, std::enable_if_t<!std::is_void_v<T>, int> = 0>
    static Type &Make(const T &args)
    {
        ASSERT(!Instance, "Singleton already exist");
        Type *rawPtr = static_cast<Type *>(::operator new(sizeof(Type)));
        Instance.reset(rawPtr);
        new (rawPtr) Type(args);
        return *Instance;
    }

    // if no constructor arguments
    template <typename T = ConstructorArguments, std::enable_if_t<std::is_void_v<T>, int> = 0>
    [[nodiscard]] static Type &Get()
    {
        if (!Instance)
        {
            Type *rawPtr = static_cast<Type *>(::operator new(sizeof(Type)));
            Instance.reset(rawPtr);
            new (rawPtr) Type();
        }
        return *Instance;
    }

    static void Check()
    {
        Type &v = Get();
        ASSERT(Instance, "Failed check");
    }
};