#pragma once

#include "SandboxEntity.hpp"
#include "boost/container/small_vector.hpp"

class SandboxWorld
{
    boost::container::small_vector<SandboxEntity,20> entities;


    public:
    void Update();
};