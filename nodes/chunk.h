#ifndef MEINEKRAFT_CHUNK_H
#define MEINEKRAFT_CHUNK_H

#include "../math/vector.h"
#include "../render/primitives.h"
#include "block.h"

class Noise;

class Chunk {
public:
    Chunk(Vec3<float> world_position, const Noise &noise);
    static const uint16_t dimension = 32; // The 'width' of the chunk in number of cubes
    Vec3<float> position;
    Vec3<float> center_position;
    uint16_t num_blocks;
    std::vector<std::shared_ptr<Block>> blocks;
};

#endif //MEINEKRAFT_CHUNK_H
