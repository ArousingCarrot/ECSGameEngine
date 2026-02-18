#include "SystemManager.h"
#include <algorithm>

void SystemManager::EntityDestroyed(Entity e) {
    for (auto& [ti, system] : mSystems) {
        auto& vec = system->mEntities;
        vec.erase(std::remove(vec.begin(), vec.end(), e), vec.end());
    }
}

void SystemManager::EntitySignatureChanged(Entity e, const Signature& entitySignature) {
    for (auto& [ti, system] : mSystems) {
        const auto sigIt = mSignatures.find(ti);
        if (sigIt == mSignatures.end()) {
            continue;
        }

        const Signature& systemSignature = sigIt->second;
        const bool matches = (entitySignature & systemSignature) == systemSignature;

        auto& vec = system->mEntities;
        auto it = std::find(vec.begin(), vec.end(), e);

        if (matches) {
            if (it == vec.end()) vec.push_back(e);
        }
        else {
            if (it != vec.end()) vec.erase(it);
        }
    }
}

void SystemManager::UpdateAll(float dt) {
    // Deterministic update: respect registration order
    for (const auto& ti : mUpdateOrder) {
        auto it = mSystems.find(ti);
        if (it != mSystems.end() && it->second) {
            it->second->Update(dt);
        }
    }
}
