#pragma once

#include "Entity.h"
#include <vector>

class ISystem {
public:
    virtual ~ISystem() = default;
    virtual void Update(float dt) = 0;

    std::vector<Entity> mEntities;
};
