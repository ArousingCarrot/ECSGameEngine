#include "EntityManager.h"

EntityManager::EntityManager()
    : mLivingEntityCount(0)
{
    // Initially, no entity is alive
    mAlive.fill(false);

    // Fill the queue with all possible IDs
    for (Entity e = 0; e < MAX_ENTITIES; ++e) {
        mAvailableEntities.push(e);
    }
}

Entity EntityManager::CreateEntity() {
    if (mLivingEntityCount >= MAX_ENTITIES) {
        throw std::runtime_error("Too many entities in existence.");
    }

    if (mAvailableEntities.empty()) {
        throw std::runtime_error("No available entity IDs.");
    }

    // Grab the next free ID
    Entity id = mAvailableEntities.front();
    mAvailableEntities.pop();

    if (mAlive[id]) {
        throw std::logic_error("EntityManager: CreateEntity returned an ID that is already alive.");
    }

    mAlive[id] = true;
    mSignatures[id].reset();
    ++mLivingEntityCount;
    return id;
}

void EntityManager::DestroyEntity(Entity e) {
    if (e >= MAX_ENTITIES) {
        throw std::out_of_range("Entity out of range.");
    }
    if (!mAlive[e]) {
        throw std::logic_error("EntityManager::DestroyEntity called on a dead entity.");
    }

    // Reset signature and mark dead
    mSignatures[e].reset();
    mAlive[e] = false;

    // Recycle the ID
    mAvailableEntities.push(e);
    --mLivingEntityCount;
}

void EntityManager::SetSignature(Entity e, Signature sig) {
    if (e >= MAX_ENTITIES) {
        throw std::out_of_range("Entity out of range.");
    }
    if (!mAlive[e]) {
        throw std::logic_error("EntityManager::SetSignature called on a dead entity.");
    }
    mSignatures[e] = sig;
}

Signature EntityManager::GetSignature(Entity e) const {
    if (e >= MAX_ENTITIES) {
        throw std::out_of_range("Entity out of range.");
    }
    if (!mAlive[e]) {
        throw std::logic_error("EntityManager::GetSignature called on a dead entity.");
    }
    return mSignatures[e];
}
