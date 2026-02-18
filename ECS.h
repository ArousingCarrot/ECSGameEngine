#pragma once

#include <memory>
#include "EntityManager.h"
#include "ComponentManager.h"
#include "SystemManager.h"

class ECS {
public:
    ECS();
    ~ECS();

    Entity CreateEntity();
    void   DestroyEntity(Entity entity);

    template<typename T>
    void RegisterComponent() {
        mComponentManager->RegisterComponent<T>();
    }

    template<typename T>
    void AddComponent(Entity entity, T component) {
        mComponentManager->AddComponent<T>(entity, component);
        auto type = mComponentManager->GetComponentType<T>();
        auto sig = mEntityManager->GetSignature(entity);
        sig.set(type);
        mEntityManager->SetSignature(entity, sig);
        mSystemManager->EntitySignatureChanged(entity, sig);
    }

    template<typename T>
    void RemoveComponent(Entity entity) {
        mComponentManager->RemoveComponent<T>(entity);
        auto type = mComponentManager->GetComponentType<T>();
        auto sig = mEntityManager->GetSignature(entity);
        sig.reset(type);
        mEntityManager->SetSignature(entity, sig);
        mSystemManager->EntitySignatureChanged(entity, sig);
    }

    template<typename T>
    T& GetComponent(Entity entity) {
        return mComponentManager->GetComponent<T>(entity);
    }

    template<typename T>
    ComponentType GetComponentType() {
        return mComponentManager->GetComponentType<T>();
    }

    template<typename T, typename... Args>
    std::shared_ptr<T> RegisterSystem(Args&&... args) {
        return mSystemManager->RegisterSystem<T>(std::forward<Args>(args)...);
    }

    template<typename T>
    void SetSystemSignature(Signature signature) {
        mSystemManager->SetSignature<T>(signature);
    }
    void Update(float dt);

    EntityManager& GetEntityManager() { return *mEntityManager; }
    ComponentManager& GetComponentManager() { return *mComponentManager; }
    SystemManager& GetSystemManager() { return *mSystemManager; }

private:
    std::unique_ptr<EntityManager>    mEntityManager;
    std::unique_ptr<ComponentManager> mComponentManager;
    std::unique_ptr<SystemManager>    mSystemManager;
};
