#include "render.h"

#include <GL/glew.h>
#include <SDL2/SDL_image.h>
#include <array>

#include "../world/world.h"
#include "camera.h"

#include "graphicsbatch.h"
#include "rendercomponent.h"
#include "../nodes/entity.h"
#include "shader.h"
#include "../util/filemonitor.h"
#include "transform.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "../include/tinyobjloader/tiny_obj_loader.h"

/// Column major - Camera combined rotation matrix (y, x) & translation matrix
Mat4<float> Renderer::FPSViewRH(Vec3<float> eye, float pitch, float yaw) {
    static constexpr float rad = M_PI / 180;
    float cosPitch = cosf(pitch * rad);
    float sinPitch = sinf(pitch * rad);
    float cosYaw = cosf(yaw * rad);
    float sinYaw = sinf(yaw * rad);
    auto xaxis = Vec3<float>{cosYaw, 0, -sinYaw};
    auto yaxis = Vec3<float>{sinYaw * sinPitch, cosPitch, cosYaw * sinPitch};
    auto zaxis = Vec3<float>{sinYaw * cosPitch, -sinPitch, cosPitch * cosYaw};
    Mat4<float> matrix;
    matrix[0][0] = xaxis.x;
    matrix[0][1] = yaxis.x;
    matrix[0][2] = zaxis.x;
    matrix[0][3] = 0.0f;
    matrix[1][0] = xaxis.y;
    matrix[1][1] = yaxis.y;
    matrix[1][2] = zaxis.y;
    matrix[1][3] = 0.0f;
    matrix[2][0] = xaxis.z;
    matrix[2][1] = yaxis.z;
    matrix[2][2] = zaxis.z;
    matrix[2][3] = 0.0f;
    matrix[3][0] = -xaxis.dot(eye);
    matrix[3][1] = -yaxis.dot(eye);
    matrix[3][2] = -zaxis.dot(eye); // GLM says no minus , other's say minus
    matrix[3][3] = 1.0f;
    return matrix;
}

/// A.k.a perspective matrix
Mat4<float> gen_projection_matrix(float z_near, float z_far, float fov, float aspect) {
    const float rad = M_PI / 180;
    float tanHalf = tanf(fov * rad / 2);
    float a = 1 / (tanHalf * aspect);
    float b = 1 / tanHalf;
    float c = -(z_far + z_near) / (z_far - z_near);
    float d = -(2 * z_far * z_near) / (z_far - z_near);
    Mat4<float> matrix;
    matrix[0] = {a, 0.0f, 0.0f, 0.0f};
    matrix[1] = {0.0f, b, 0.0f, 0.0f};
    matrix[2] = {0.0f, 0.0f, c, d};
    matrix[3] = {0.0f, 0.0f, -1.0f, 0.0f};
    return matrix;
}

Renderer::Renderer(): DRAW_DISTANCE(200), projection_matrix(Mat4<float>()), state{}, graphics_batches{}, shaders{},
                      shader_file_monitor(std::make_unique<FileMonitor>()), lights{} {
    glewExperimental = (GLboolean) true;
    glewInit();

    Light light{Vec3<float>{15.0, 15.0, 15.0}, Color4<float>{0.4, 0.4, 0.8, 1.0}};
    lights.push_back(light);

    Transform transform = {light.position, light.position + Vec3<float>{0.5, 0.5, 0.5}, 100000};
    transformations.push_back(transform);

    // Light uniform buffer
    glGenBuffers(1, &gl_light_uniform_buffer);
    glBindBuffer(GL_UNIFORM_BUFFER, gl_light_uniform_buffer);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(Light) * lights.size(), &lights, GL_DYNAMIC_DRAW);

    // TODO: Load all block & skybox textures
    const auto base = "/Users/AlexanderLingtorp/Google Drive/Repositories/MeineKraft/";
    std::vector<std::string> cube_faces = {base + std::string("res/blocks/grass/side.jpg"),
                                           base + std::string("res/blocks/grass/side.jpg"),
                                           base + std::string("res/blocks/grass/top.jpg"),
                                           base + std::string("res/blocks/grass/bottom.jpg"),
                                           base + std::string("res/blocks/grass/side.jpg"),
                                           base + std::string("res/blocks/grass/side.jpg")};
    // this->textures[Texture::GRASS] = load_cube_map(cube_faces, jpg);

    std::vector<std::string> skybox_faces = {base + std::string("res/sky/right.jpg"),
                                             base + std::string("res/sky/left.jpg"),
                                             base + std::string("res/sky/top.jpg"),
                                             base + std::string("res/sky/bottom.jpg"),
                                             base + std::string("res/sky/back.jpg"),
                                             base + std::string("res/sky/front.jpg")};
    // this->textures[Texture::SKYBOX] = load_cube_map(skybox_faces, jpg);

    /// Compile shaders
    // Files are truly horrible and filepaths are even worse, therefore this is not scalable
    const std::string shader_base_filepath = "/Users/AlexanderLingtorp/Google Drive/Repositories/MeineKraft/shaders/";
    std::string err_msg;
    bool success;

    const auto skybox_vert = shader_base_filepath + "skybox/vertex-shader.glsl";
    const auto skybox_frag = shader_base_filepath + "skybox/fragment-shader.glsl";
    Shader skybox_shader(skybox_vert, skybox_frag);
    std::tie(success, err_msg) = skybox_shader.compile();
    shaders.insert({ShaderType::SKYBOX_SHADER, skybox_shader});
    if (!success) { SDL_Log("%s", err_msg.c_str()); }

    const auto std_vert = shader_base_filepath + "block/vertex-shader.glsl";
    const auto std_frag = shader_base_filepath + "block/fragment-shader.glsl";
    Shader std_shader(std_vert, std_frag);
    std::tie(success, err_msg) = std_shader.compile();
    shaders.insert({ShaderType::STANDARD_SHADER, std_shader});
    if (!success) { SDL_Log("%s", err_msg.c_str()); }

    shader_file_monitor->add_file(shader_base_filepath + "skybox/vertex-shader.glsl");
    shader_file_monitor->add_file(shader_base_filepath + "skybox/fragment-shader.glsl");
    shader_file_monitor->add_file(shader_base_filepath + "block/vertex-shader.glsl");
    shader_file_monitor->add_file(shader_base_filepath + "block/fragment-shader.glsl");
    shader_file_monitor->start_monitor();

    /// Camera
    const auto position  = Vec3<float>{0.0f, 0.0f, 0.0f};  // cam position
    const auto direction = Vec3<float>{0.0f, 0.0f, -1.0f}; // position of where the cam is looking
    const auto world_up  = Vec3<float>{0.0, 1.0f, 0.0f};   // world up
    this->camera = std::make_shared<Camera>(position, direction, world_up);
}

bool Renderer::point_inside_frustrum(Vec3<float> point, std::array<Plane<float>, 6> planes) {
    const auto left_plane = planes[0]; const auto right_plane = planes[1];
    const auto top_plane  = planes[2]; const auto bot_plane   = planes[3];
    const auto near_plane = planes[4]; const auto far_plane   = planes[5];
    const auto dist_l = left_plane.distance_to_point(point);
    const auto dist_r = right_plane.distance_to_point(point);
    const auto dist_t = top_plane.distance_to_point(point);
    const auto dist_b = bot_plane.distance_to_point(point);
    const auto dist_n = near_plane.distance_to_point(point);
    const auto dist_f = far_plane.distance_to_point(point);
    if (dist_l < 0 && dist_r < 0 && dist_t < 0 && dist_b < 0 && dist_n < 0 && dist_f < 0) { return true; }
    return false;
}

void Renderer::render(uint32_t delta) {
    /// Reset render stats
    state = RenderState();

    /// Check shader files for modifications
    if (shader_file_monitor->files_modfied) {
        for (auto &kv : shaders) {
            auto shader_key = kv.first;
            auto shader     = kv.second;
            std::string err_msg;
            bool success;
            std::tie(success, err_msg) = shader.recompile();
            if (!success) { SDL_Log("%s", err_msg.c_str()); continue; }
            for (auto &batch : graphics_batches) {
                if (batch.shader_program == shader_key) {
                    link_batch(batch); // relink the batch
                }
            }
        }
        shader_file_monitor->clear_all_modification_flags();
    }

    /// Frustrum planes
    auto camera_view = FPSViewRH(camera->position, camera->pitch, camera->yaw);
    auto frustrum_view = camera_view * projection_matrix;
    std::array<Plane<float>, 6> planes = extract_planes(frustrum_view.transpose());
    auto left_plane = planes[0]; auto right_plane = planes[1];
    auto top_plane  = planes[2]; auto bot_plane   = planes[3];
    auto near_plane = planes[4]; auto far_plane   = planes[5];

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);

    for (auto &transform : transformations) { transform.update(delta); } // TODO: Move this kind of comp. into seperate thread or something

    lights[0] = transformations[0].current_position; // FIXME: Transforms are not updating their Entities..

    // TODO: Update number of lights in the scene
    // TODO: Cull the lights

    for (auto &batch : graphics_batches) {
        glBindVertexArray(batch.gl_VAO);
        glUseProgram(shaders.at(batch.shader_program).gl_program);
        glUniformMatrix4fv(batch.gl_camera_view, 1, GL_FALSE, camera_view.data());
        glUniform3fv(batch.gl_camera_position, 1, (const GLfloat *) &camera->position);

        // TODO: Set the defaults in GraphicsState too
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        /// Update Light data for the batch
        glBindBuffer(GL_UNIFORM_BUFFER, gl_light_uniform_buffer);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(Light) * lights.size(), lights.data(), GL_DYNAMIC_DRAW);

        std::vector<Mat4<float>> buffer{};
        for (auto &component : batch.components) {
            // Draw distance
            auto camera_to_entity = camera->position - component->entity->position;
            if (camera_to_entity.length() >= DRAW_DISTANCE) { continue; }

            // Frustrum cullling
            if (point_inside_frustrum(component->entity->position, planes)) { continue; }

            // Dream:
            // GL_TEXTURE_CUBE_MAP
            // glBindTexture(component->graphics_state.texture.gl_texture_type, component->graphics_state.texture.gl_texture);

            // Reality
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(batch.mesh.texture.gl_texture_type, batch.mesh.texture.gl_texture);
            glUniform1i(glGetUniformLocation(shaders.at(batch.shader_program).gl_program, "diffuse_sampler"), 0);

            Mat4<float> model{};
            model = model.translate(component->entity->position);
            model = model.scale(component->entity->scale);
            buffer.push_back(model.transpose());
        }
        glBindBuffer(GL_ARRAY_BUFFER, batch.gl_models_buffer_object);
        glBufferData(GL_ARRAY_BUFFER, buffer.size() * sizeof(Mat4<float>), buffer.data(), GL_DYNAMIC_DRAW);
        glDrawElementsInstanced(GL_TRIANGLES, batch.mesh.indices.size(), GL_UNSIGNED_INT, 0, buffer.size());

        /// Update render stats
        state.entities += buffer.size();
    }
    state.graphic_batches = graphics_batches.size();
}

Renderer::~Renderer() {
    // TODO: Clear up all the GraphicsObjects
    shader_file_monitor->end_monitor();
}

/// Returns the planes from the frustrum matrix in order; {left, right, bottom, top, near, far}
/// See: http://gamedevs.org/uploads/fast-extraction-viewing-frustum-planes-from-world-view-projection-matrix.pdf
std::array<Plane<float>, 6> Renderer::extract_planes(Mat4<float> mat) {
    auto planes = std::array<Plane<float>, 6>();
    auto left_plane  = Plane<float>(mat[3][0] + mat[0][0],
                                    mat[3][1] + mat[0][1],
                                    mat[3][2] + mat[0][2],
                                    mat[3][3] + mat[0][3]);

    auto right_plane = Plane<float>(mat[3][0] - mat[0][0],
                                    mat[3][1] - mat[0][1],
                                    mat[3][2] - mat[0][2],
                                    mat[3][3] - mat[0][3]);

    auto bot_plane   = Plane<float>(mat[3][0] + mat[1][0],
                                    mat[3][1] + mat[1][1],
                                    mat[3][2] + mat[1][2],
                                    mat[3][3] + mat[1][3]);

    auto top_plane   = Plane<float>(mat[3][0] - mat[1][0],
                                    mat[3][1] - mat[1][1],
                                    mat[3][2] - mat[1][2],
                                    mat[3][3] - mat[1][3]);

    auto near_plane  = Plane<float>(mat[3][0] + mat[2][0],
                                    mat[3][1] + mat[2][1],
                                    mat[3][2] + mat[2][2],
                                    mat[3][3] + mat[2][3]);

    auto far_plane   = Plane<float>(mat[3][0] - mat[2][0],
                                    mat[3][1] - mat[2][1],
                                    mat[3][2] - mat[2][2],
                                    mat[3][3] - mat[2][3]);
    planes[0] = left_plane; planes[1] = right_plane; planes[2] = bot_plane;
    planes[3] = top_plane;  planes[4] = near_plane;  planes[5] = far_plane;
    return planes;
}

void Renderer::update_projection_matrix(float fov) {
    int height, width;
    SDL_GL_GetDrawableSize(this->window, &width, &height);
    float aspect = (float) width / (float) height;
    this->projection_matrix = gen_projection_matrix(1, -10, fov, aspect);
    glViewport(0, 0, width, height); // Update OpenGL viewport
    /// Update all shader programs projection matrices to the new one
    for (auto shader_program : shaders) {
        glUseProgram(shader_program.second.gl_program);
        GLuint projection = glGetUniformLocation(shader_program.second.gl_program, "projection");
        glUniformMatrix4fv(projection, 1, GL_FALSE, projection_matrix.data());
    }
}

void Renderer::add_to_batch(RenderComponent *component, Mesh mesh) {
    for (auto &batch : graphics_batches) {
        if (batch.hash_id == component->entity->hash_id) {
            batch.components.push_back(component);
            return;
        }
    }

    GraphicsBatch batch{component->entity->hash_id};
    batch.mesh = mesh;
    batch.shader_program = ShaderType::STANDARD_SHADER;
    link_batch(batch);

    batch.components.push_back(component);
    graphics_batches.push_back(batch);
}

Mesh Renderer::load_mesh_from_file(std::string filepath, std::string directory_filepath) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;
    auto success = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, filepath.c_str(), directory_filepath.c_str(), true);
    if (!success) { SDL_Log("Failed loading mesh %s: %s", filepath.c_str(), err.c_str()); return Mesh(); }
    if (err.size() > 0) { SDL_Log("%s", err.c_str()); }

    std::unordered_map<Vertex<float>, size_t> unique_vertices{};
    Mesh mesh{};
    for (const auto &shape : shapes) { // Shapes
        for (const auto &idx : shape.mesh.indices) { // Faces
            Vertex<float> vertex{};
            float vx = attrib.vertices[3 * idx.vertex_index + 0];
            float vy = attrib.vertices[3 * idx.vertex_index + 1];
            float vz = attrib.vertices[3 * idx.vertex_index + 2];
            vertex.position = {vx, vy, vz};

            float tx = attrib.texcoords[2 * idx.texcoord_index + 0];
            float ty = attrib.texcoords[2 * idx.texcoord_index + 1];
            vertex.texCoord = {tx, 1.0f - ty}; // .obj format has flipped y-axis compared to OpenGL

            float nx = attrib.normals[3 * idx.normal_index + 0];
            float ny = attrib.normals[3 * idx.normal_index + 1];
            float nz = attrib.normals[3 * idx.normal_index + 2];
            vertex.normal = Vec3<float>{nx, ny, nz}.normalize();

            if (unique_vertices.count(vertex) == 0) {
                unique_vertices[vertex] = mesh.vertices.size();
                mesh.indices.push_back(mesh.vertices.size());
                mesh.vertices.push_back(vertex);
            } else {
                mesh.indices.push_back(unique_vertices.at(vertex));
            }
        }
    }

    std::unordered_map<std::string, uint64_t> loaded_textures{};
    for (const auto &material : materials) {
        /// Color map, a.k.a diffuse map
        if (!loaded_textures[directory_filepath + material.diffuse_texname]) {
            Texture diffuse_texture{};
            diffuse_texture.load(material.diffuse_texname, directory_filepath);
            if (diffuse_texture.loaded_succesfully) {
                mesh.texture = diffuse_texture;
                loaded_textures[material.diffuse_texname] = diffuse_texture.gl_texture;
            }
        }
    }

    SDL_Log("Number of vertices: %lu for model %s", mesh.vertices.size(), filepath.c_str());
    return mesh;
}

void Renderer::link_batch(GraphicsBatch &batch) {
    auto batch_shader_program = shaders.at(batch.shader_program).gl_program;

    glGenVertexArrays(1, &batch.gl_VAO);
    glBindVertexArray(batch.gl_VAO);
    batch.gl_camera_view = glGetUniformLocation(batch_shader_program, "camera_view");
    batch.gl_camera_position = glGetUniformLocation(batch_shader_program, "camera_position");

    GLuint gl_VBO;
    glGenBuffers(1, &gl_VBO);
    glBindBuffer(GL_ARRAY_BUFFER, gl_VBO);
    auto vertices = batch.mesh.to_floats();
    glBufferData(GL_ARRAY_BUFFER, batch.mesh.byte_size_of_vertices(), vertices.data(), GL_DYNAMIC_DRAW);

    // Then set our vertex attributes pointers, only doable AFTER linking
    GLint positionAttrib = glGetAttribLocation(batch_shader_program, "position");
    glVertexAttribPointer(positionAttrib, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex<float>), (const void *)offsetof(Vertex<float>, position));
    glEnableVertexAttribArray(positionAttrib);

    GLint colorAttrib = glGetAttribLocation(batch_shader_program, "vColor");
    glVertexAttribPointer(colorAttrib, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex<float>), (const void *)offsetof(Vertex<float>, color));
    glEnableVertexAttribArray(colorAttrib);

    GLint normalAttrib = glGetAttribLocation(batch_shader_program, "normal");
    glVertexAttribPointer(normalAttrib, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex<float>), (const void *)offsetof(Vertex<float>, normal));
    glEnableVertexAttribArray(normalAttrib);

    GLint texCoordAttrib = glGetAttribLocation(batch_shader_program, "vTexCoord");
    glVertexAttribPointer(texCoordAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex<float>), (const void *)offsetof(Vertex<float>, texCoord));
    glEnableVertexAttribArray(texCoordAttrib);

    GLint texture_sampler = glGetUniformLocation(batch_shader_program, "diffuse_sampler");
    batch.mesh.texture.gl_texture_location = texture_sampler;

    // Lights UBO
    auto block_index = glGetUniformBlockIndex(batch_shader_program, "lights_block");
    glBindBuffer(GL_UNIFORM_BUFFER, gl_light_uniform_buffer);
    glBindBufferBase(GL_UNIFORM_BUFFER, block_index, gl_light_uniform_buffer);

    // Buffer for all the model matrices
    glGenBuffers(1, &batch.gl_models_buffer_object);
    glBindBuffer(GL_ARRAY_BUFFER, batch.gl_models_buffer_object);

    GLuint modelAttrib = glGetAttribLocation(batch_shader_program, "model");
    for (int i = 0; i < 4; i++) {
        glVertexAttribPointer(modelAttrib + i, 4, GL_FLOAT, GL_FALSE, sizeof(Mat4<float>), (const void *) (sizeof(float) * i * 4));
        glEnableVertexAttribArray(modelAttrib + i);
        glVertexAttribDivisor(modelAttrib + i, 1);
    }

    GLuint EBO;
    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, batch.mesh.byte_size_of_indices(), batch.mesh.indices.data(), GL_STATIC_DRAW);

    GLuint projection = glGetUniformLocation(batch_shader_program, "projection");
    glUniformMatrix4fv(projection, 1, GL_FALSE, projection_matrix.data());
}

void Renderer::remove_from_batch(RenderComponent *component) {
    for (auto &batch : graphics_batches) {
        for (auto &comp : batch.components) {
            if (comp == component) {
                auto &vec = batch.components;
                vec.erase(std::remove(vec.begin(), vec.end(), component), vec.end());
            }
        }
    }
}


