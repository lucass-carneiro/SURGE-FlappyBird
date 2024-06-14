#ifndef SURGE_FLAPPY_BIRD_TYPE_ALIASES_HPP
#define SURGE_FLAPPY_BIRD_TYPE_ALIASES_HPP

#include "player/pv_ubo.hpp"
#include "player/sprite.hpp"
#include "player/texture.hpp"

namespace fpb {

using pvubo_t = surge::atom::pv_ubo::buffer;

using tdb_t = surge::atom::texture::database;
using sdb_t = surge::atom::sprite::database;

} // namespace fpb

#endif // SURGE_FLAPPY_BIRD_TYPE_ALIASES_HPP