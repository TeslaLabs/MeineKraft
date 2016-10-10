#include "teapot.h"

Teapot::Teapot(): Entity(generate_entity_id()), render_comp(RenderComponent(this, "/Users/AlexanderLingtorp/Downloads/dragon.obj")) {
    // /Users/AlexanderLingtorp/Downloads/dragon.obj
    // "/Users/AlexanderLingtorp/Downloads/teapot/teapot.obj"
    scale = 200;
    render_comp.set_cube_map_texture(Texture::SKYBOX);
}

void Teapot::update(uint64_t delta, const std::shared_ptr<Camera> camera) {
    render_comp.update(delta);
}

Teapot::~Teapot() {

}


