#pragma once

#include "ISystem.h"
#include "ComponentTypes.h"
#include "Entity.h"

#include <memory>
#include <unordered_map>
#include <vector>
#include <typeindex>
#include <cassert>

class SystemManager {
public:
    SystemManager() = default;

    template<typename T, typename... Args>
    std::shared_ptr<T> RegisterSystem(Args&&... args) {
        const std::type_index ti{ typeid(T) };
        assert(mSystems.find(ti) == mSystems.end() && "Registering system more than once.");
        auto system = std::make_shared<T>(std::forward<Args>(args)...);
        mSystems.emplace(ti, system);

        // Record deterministic update order: order of registration
        mUpdateOrder.push_back(ti);

        return system;
    }

    template<typename T>
    std::shared_ptr<T> GetSystem() {
        const std::type_index ti{ typeid(T) };
        auto it = mSystems.find(ti);
        assert(it != mSystems.end() && "System used before registered.");
        return std::static_pointer_cast<T>(it->second);
    }

    template<typename T>
    void SetSignature(const Signature& signature) {
        const std::type_index ti{ typeid(T) };
        assert(mSystems.find(ti) != mSystems.end() && "System used before registered.");
        mSignatures[ti] = signature;
    }

    void EntityDestroyed(Entity e);
    void EntitySignatureChanged(Entity e, const Signature& entitySignature);
    void UpdateAll(float dt);

private:
    std::unordered_map<std::type_index, Signature>                mSignatures;
    std::unordered_map<std::type_index, std::shared_ptr<ISystem>> mSystems;
    std::vector<std::type_index>                                  mUpdateOrder;
};
