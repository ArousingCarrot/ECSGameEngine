#pragma once
#include <cstdint>
#include <limits>

using Entity = std::uint32_t;

inline constexpr Entity INVALID_ENTITY = std::numeric_limits<Entity>::max();
