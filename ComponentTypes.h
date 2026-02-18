#pragma once

#include <bitset>
#include <cstddef>
#include <cstdint>

constexpr std::size_t MAX_COMPONENTS = 32;
constexpr std::size_t MAX_ENTITIES = 100000;

using ComponentType = std::uint8_t;

using Signature = std::bitset<MAX_COMPONENTS>;
