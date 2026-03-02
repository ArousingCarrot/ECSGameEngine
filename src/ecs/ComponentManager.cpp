#include "ComponentManager.h"

void ComponentManager::EntityDestroyed(Entity e) {
    for (auto& pair : mComponentArrays) {
        pair.second->EntityDestroyed(e);
    }
}