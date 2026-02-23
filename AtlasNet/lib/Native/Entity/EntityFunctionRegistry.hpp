#pragma once

#include "Entity.hpp"
class EntityFunctionRegistry
{
    using FuncType = bool(*)(AtlasEntity&);

    template <FuncType... Fs>
    struct Registry {
        static constexpr FuncType funcs[] = {Fs...};

        template <FuncType F>
        static constexpr bool contains() {
            for(auto func : funcs) {
                if(func == F) return true;
            }
            return false;
        }

        template <FuncType F>
        static bool call(AtlasEntity& e) {
            static_assert(contains<F>(), "Function not registered for entity!");
            return F(e);
        }
    };
};