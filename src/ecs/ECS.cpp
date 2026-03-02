#include "ECS.h"
#include "SystemManager.h"

ECS::ECS()
    : mEntityManager(std::make_unique<EntityManager>())
    , mComponentManager(std::make_unique<ComponentManager>())
    , mSystemManager(std::make_unique<SystemManager>())
{
}

ECS::~ECS() = default;

Entity ECS::CreateEntity() {
    return mEntityManager->CreateEntity();
}

void ECS::DestroyEntity(Entity entity) {
    mEntityManager->DestroyEntity(entity);
    mComponentManager->EntityDestroyed(entity);
    mSystemManager->EntityDestroyed(entity);
}

void ECS::Update(float dt)
{
    if (mSystemManager) {
        mSystemManager->UpdateAll(dt);
    }
}
