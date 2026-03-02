#pragma once

#include "ComponentTypes.h"
#include "Entity.h"
#include <array>
#include <unordered_map>
#include <memory>
#include <typeindex>
#include <cassert>
#include <limits>

struct IComponentArray {
    virtual ~IComponentArray() = default;
    virtual void EntityDestroyed(Entity e) = 0;
};

template<typename T>
class ComponentArray : public IComponentArray {
public:
    ComponentArray() {
        mEntityToIndex.fill(INVALID_INDEX);
    }
    void InsertData(Entity e, T component) {
        assert(e < MAX_ENTITIES && "Entity out of range.");

        const size_t currentIndex = mEntityToIndex[e];
        assert(currentIndex == INVALID_INDEX && "Component added twice to the same entity.");

        const size_t newIndex = mSize;
        assert(newIndex < MAX_ENTITIES && "ComponentArray is full.");

        mComponentArray[newIndex] = std::move(component);
        mEntityToIndex[e] = newIndex;
        mIndexToEntity[newIndex] = e;

        ++mSize;
    }

    void RemoveData(Entity e) {
        assert(e < MAX_ENTITIES && "Entity out of range.");

        const size_t indexOfRemoved = mEntityToIndex[e];
        assert(indexOfRemoved != INVALID_INDEX && "Removing non-existent component.");

        const size_t indexOfLast = mSize - 1;

        if (indexOfRemoved != indexOfLast) {
            mComponentArray[indexOfRemoved] = std::move(mComponentArray[indexOfLast]);

            Entity lastEntity = mIndexToEntity[indexOfLast];
            mIndexToEntity[indexOfRemoved] = lastEntity;
            mEntityToIndex[lastEntity] = indexOfRemoved;
        }
        mEntityToIndex[e] = INVALID_INDEX;
        --mSize;
    }
    T& GetData(Entity e) {
        assert(e < MAX_ENTITIES && "Entity out of range.");
        const size_t index = mEntityToIndex[e];
        assert(index != INVALID_INDEX && "Retrieving non-existent component.");
        return mComponentArray[index];
    }

    void EntityDestroyed(Entity e) override {
        assert(e < MAX_ENTITIES && "Entity out of range.");
        const size_t index = mEntityToIndex[e];
        if (index != INVALID_INDEX) {
            RemoveData(e);
        }
    }

private:
    static constexpr size_t INVALID_INDEX = std::numeric_limits<size_t>::max();

    std::array<T, MAX_ENTITIES>   mComponentArray{};
    std::array<size_t, MAX_ENTITIES> mEntityToIndex{};
    std::array<Entity, MAX_ENTITIES> mIndexToEntity{};
    size_t                        mSize{ 0 };
};

class ComponentManager {
public:
    template<typename T>
    void RegisterComponent();

    template<typename T>
    ComponentType GetComponentType();

    template<typename T>
    void AddComponent(Entity e, T component);

    template<typename T>
    void RemoveComponent(Entity e);

    template<typename T>
    T& GetComponent(Entity e);

    void EntityDestroyed(Entity e);

private:
    template<typename T>
    std::shared_ptr<ComponentArray<T>> GetComponentArray();

    ComponentType                                      mNextComponentType{ 0 };
    std::unordered_map<std::type_index, ComponentType> mComponentTypes;
    std::unordered_map<std::type_index,
        std::shared_ptr<IComponentArray>>              mComponentArrays;
};

template<typename T>
void ComponentManager::RegisterComponent() {
    const auto ti = std::type_index(typeid(T));
    assert(mComponentTypes.count(ti) == 0);
    mComponentTypes[ti] = mNextComponentType++;
    mComponentArrays[ti] = std::make_shared<ComponentArray<T>>();
}

template<typename T>
ComponentType ComponentManager::GetComponentType() {
    const auto it = mComponentTypes.find(typeid(T));
    assert(it != mComponentTypes.end());
    return it->second;
}

template<typename T>
void ComponentManager::AddComponent(Entity e, T component) {
    GetComponentArray<T>()->InsertData(e, component);
}

template<typename T>
void ComponentManager::RemoveComponent(Entity e) {
    GetComponentArray<T>()->RemoveData(e);
}

template<typename T>
T& ComponentManager::GetComponent(Entity e) {
    return GetComponentArray<T>()->GetData(e);
}

template<typename T>
std::shared_ptr<ComponentArray<T>> ComponentManager::GetComponentArray() {
    const auto it = mComponentArrays.find(typeid(T));
    assert(it != mComponentArrays.end());
    return std::static_pointer_cast<ComponentArray<T>>(it->second);
}
