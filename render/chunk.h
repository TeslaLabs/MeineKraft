#ifndef MEINEKRAFT_CHUNK_H
#define MEINEKRAFT_CHUNK_H

#include "../math/vector.h"
#include "primitives.h"
#include "../math/noise.h"

class Chunk {
public:
    Chunk(Vec3 world_position);
    static const uint16_t dimension = 12; // The 'width' of the chunk in number of cubes
    Vec3 position;
    uint16_t numCubes;
    std::vector<Cube> blocks;
};

#endif //MEINEKRAFT_CHUNK_H
