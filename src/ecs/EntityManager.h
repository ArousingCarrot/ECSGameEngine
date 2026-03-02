#pragma once

#include "Entity.h"
#include "ComponentTypes.h"

#include <queue>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <bitset>

using Signature = std::bitset<MAX_COMPONENTS>;

class EntityManager {
public:
    static constexpr Entity MAX_ENTITIES = static_cast<Entity>(::MAX_ENTITIES);

    EntityManager();

    Entity  CreateEntity();
    void    DestroyEntity(Entity e);

    void    SetSignature(Entity e, Signature sig);
    Signature GetSignature(Entity e) const;

private:
    std::queue<Entity>     mAvailableEntities;
    std::array<Signature, MAX_ENTITIES> mSignatures{};
    std::array<bool, MAX_ENTITIES>      mAlive{};
    uint32_t                            mLivingEntityCount;
};
