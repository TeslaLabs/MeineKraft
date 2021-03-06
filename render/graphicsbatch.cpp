#include "graphicsbatch.h"
#include "rendercomponent.h"

GraphicsBatch::GraphicsBatch(uint64_t hash_id): hash_id(hash_id), components{}, mesh{}, gl_camera_view(0),
                                                gl_models_buffer_object(0), shader_program(ShaderType::STANDARD_SHADER), gl_VAO(0) {};