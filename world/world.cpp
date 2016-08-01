#include <iostream>
#include <numeric>
#include "world.h"

World::World() {}

void World::world_tick(uint32_t delta, const std::shared_ptr<Camera> camera) {
    static auto once = false;
    if (once) { return; } else { once = true; }
    /// Spawn a flat world once
    std::vector<GLfloat> x = {};
    std::vector<GLfloat> y = {-Chunk::dimension};
    std::vector<GLfloat> z = {};
    for (int i = -10; i < 10; i++) {
        x.push_back(i * Chunk::dimension);
        z.push_back(i * Chunk::dimension);
    }
    for (auto x : x) {
        for (auto y : y) {
            for (auto z : z) {
                Vec3 position = {x, y, z};
                chunks.push_back(Chunk::Chunk(position));
            }
        }
    }

//    // 1. Snap Camera/Player to the world coordinate grid using it's direction vector.
//    auto camera_world_pos = Vec3{std::round(camera->position.x + camera->direction.x), std::round(camera->position.y + camera->direction.y), std::round(camera->position.z + camera->direction.z)};
//    std::vector<GLfloat> x{camera_world_pos.x - Chunk::dimension, camera_world_pos.x, camera_world_pos.x + Chunk::dimension};
//    std::vector<GLfloat> y{-Chunk::dimension};
//    std::vector<GLfloat> z{camera_world_pos.z - Chunk::dimension, camera_world_pos.z, camera_world_pos.z + Chunk::dimension};
//    // std::cout << camera_world_pos << std::endl;
//    for (auto x : x) {
//        if ((int) x % Chunk::dimension != 0) { continue; }
//        for (auto y : y) {
//            for (auto z : z) {
//                if ((int) y % Chunk::dimension != 0) { continue; }
//                Vec3 position = {x, y, z};
//                bool chunk_exists_at_pos = std::any_of(chunks.begin(), chunks.end(), [position](Chunk &c1){ return c1.position == position; });
//                if (chunk_exists_at_pos) { continue; }
//                chunks.push_back(Chunk::Chunk(position));
//            }
//        }
//    }
}
