#include "rendercomponent.h"
#include "render.h"
#include "../nodes/entity.h"

RenderComponent::RenderComponent(Entity *entity, std::string mesh_file): entity(entity), graphics_state{}, new_graphics_state{} {
    auto mesh = Renderer::instance().load_mesh_from_file(mesh_file);
    graphics_state = Renderer::instance().add_to_batch(*this, mesh);
};

RenderComponent::RenderComponent(Entity *entity): entity(entity), graphics_state{}, new_graphics_state{} {
    auto mesh = Cube();
    graphics_state = Renderer::instance().add_to_batch(*this, mesh);
}

void RenderComponent::remove_component() {
    Renderer::instance().remove_from_batch(*this);
}

void RenderComponent::set_cube_map_texture(Texture texture) {
    new_graphics_state.gl_texture = texture;
}

void RenderComponent::update(uint64_t delta) {
    new_graphics_state.position = entity->position;
    new_graphics_state.rotation = entity->rotation;
    new_graphics_state.scale    = entity->scale;
    // Copy all the graphics state changes
    graphics_state->position = new_graphics_state.position;
}

