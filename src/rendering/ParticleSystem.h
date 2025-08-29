//
// Created by gungu on 8/27/25.
//

#ifndef PARTICLESYSTEM_H
#define PARTICLESYSTEM_H

float quad_vertices[] = {
    -1.0f, -1.0f, 0.0f,  0.0f, 0.0f,
     1.0f, -1.0f, 0.0f,  1.0f, 0.0f,
    -1.0f,  1.0f, 0.0f,  0.0f, 1.0f,
     1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
};

unsigned int quad_indices[] = {
    0, 1, 2,
    1, 3, 2
};

struct Particle {
    HMM_Vec3 position{0.0f, 0.0f, 0.0f};
    HMM_Vec3 velocity{0.0f, 0.0f, 0.0f};
    float size = 1;
};

class ParticleSystem {
public:
    vector<Particle> particles;

    sg_buffer vertex_buffer{.id = SG_INVALID_ID};
    sg_buffer index_buffer{.id = SG_INVALID_ID};
    sg_image image{.id = SG_INVALID_ID};
    sg_sampler sampler{.id = SG_INVALID_ID};

    float gravity = 0;

    void initialize(Surface *surface, int particle_amount, HMM_Vec3 pos, HMM_Vec3 initial_vel, float size, float random_offset_size) {
        sg_image_desc image_desc = {};
        image_desc.width = surface->pixels[0].size();
        image_desc.height = surface->pixels.size();
        image_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
        image_desc.usage.immutable = true;
        image_desc.data = surface->get_sokol_image_data();
        image_desc.label = "particle-image";
        image = sg_make_image(&image_desc);

        sg_sampler_desc sampler_desc = {};
        sampler_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
        sampler_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
        sampler_desc.min_filter = SG_FILTER_LINEAR;
        sampler_desc.mag_filter = SG_FILTER_LINEAR;
        sampler_desc.label = "particle-sampler";
        sampler = sg_make_sampler(&sampler_desc);

        sg_buffer_desc vertex_buffer_desc = {};
        vertex_buffer_desc.usage.vertex_buffer = true;
        vertex_buffer_desc.size = sizeof(quad_vertices);
        vertex_buffer_desc.data = SG_RANGE(quad_vertices);
        vertex_buffer = sg_make_buffer(&vertex_buffer_desc);

        sg_buffer_desc index_buffer_desc = {};
        index_buffer_desc.usage.vertex_buffer = false;
        index_buffer_desc.usage.index_buffer = true;
        index_buffer_desc.usage.immutable = true;
        index_buffer_desc.size = sizeof(quad_indices);
        index_buffer_desc.data = SG_RANGE(quad_indices);
        index_buffer = sg_make_buffer(&index_buffer_desc);

        for (int i = 0; i<particle_amount; i++) {
            Particle particle;
            float r1 = static_cast <float> (rand()) / (static_cast <float> (RAND_MAX/random_offset_size));
            float r2 = static_cast <float> (rand()) / (static_cast <float> (RAND_MAX/random_offset_size));
            float r3 = static_cast <float> (rand()) / (static_cast <float> (RAND_MAX/random_offset_size));
            particle.position = {pos.X+r1, pos.Y+r2, pos.Z+r3};
            particle.velocity = initial_vel;
            particle.size = size;
            particles.push_back(particle);
        }
    }

    void update_one(Particle *particle, float dt) {
        particle->velocity.Y += gravity * dt;
        particle->position.X += particle->velocity.X * dt;
        particle->position.Y += particle->velocity.Y * dt;
        particle->position.Z += particle->velocity.Z * dt;
    }

    void draw_particles(sg_pipeline pipeline, float dt, HMM_Mat4 projection, HMM_Mat4 view) {
        sg_bindings bind = {};
        bind.vertex_buffers[0] = vertex_buffer;
        bind.index_buffer = index_buffer;

        sg_view_desc view_desc = {};
        view_desc.texture.image = image;
        view_desc.label = "particle-image-view";
        sg_view image_view = sg_make_view(&view_desc);
        bind.views[0] = image_view;
        bind.samplers[0] = sampler;
        if (sampler.id == SG_INVALID_ID) {
            cout << "sampler is invalid" << endl;
        }
        if (image_view.id == SG_INVALID_ID) {
            cout << "view is invalid" << endl;
        }
        if (image.id == SG_INVALID_ID) {
            cout << "image is invalid" << endl;
        }
        if (pipeline.id == SG_INVALID_ID) {
            cout << "pipeline is invalid" << endl;
        }
        if (vertex_buffer.id == SG_INVALID_ID) {
            cout << "vertex buffer is invalid" << endl;
        }

        sg_apply_bindings(bind);
        sg_apply_pipeline(pipeline);

        particle_vs_params_t vs_params = {};
        vs_params.projection = projection;
        vs_params.view = view;

        for (auto& particle : particles) {
            update_one(&particle, dt);

            HMM_Mat4 translate_mat = HMM_Translate(particle.position);
            HMM_Mat4 rot_mat = HMM_QToM4(HMM_Quat{0.0f, 0.0f, 0.0f, 1.0f});
            HMM_Mat4 scale_mat = HMM_Scale({particle.size, particle.size, particle.size});
            HMM_Mat4 model = HMM_MulM4(translate_mat, HMM_MulM4(rot_mat, scale_mat));
            vs_params.model = model;
            vs_params.size = particle.size;

            sg_apply_uniforms(UB_particle_vs_params, SG_RANGE(vs_params));

            sg_draw(0, 6, 1);
        }

        sg_destroy_view(image_view);
    }

    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;

    ParticleSystem(ParticleSystem&& other) noexcept {
        particles = std::move(other.particles);
        vertex_buffer = other.vertex_buffer;
        index_buffer = other.index_buffer;
        image = other.image;
        sampler = other.sampler;
        gravity = other.gravity;

        other.vertex_buffer.id = SG_INVALID_ID;
        other.index_buffer.id = SG_INVALID_ID;
        other.image.id = SG_INVALID_ID;
        other.sampler.id = SG_INVALID_ID;
    }

    ParticleSystem(Surface *surface, int particle_amount, HMM_Vec3 pos, HMM_Vec3 initial_vel, float size, float random_offset_size) {
        initialize(surface, particle_amount, pos, initial_vel, size, random_offset_size);
    }

    ParticleSystem& operator=(ParticleSystem&& other) noexcept {
        if (this != &other) {
            sg_destroy_buffer(vertex_buffer);
            sg_destroy_buffer(index_buffer);
            sg_destroy_image(image);
            sg_destroy_sampler(sampler);

            particles = std::move(other.particles);
            vertex_buffer = other.vertex_buffer;
            index_buffer = other.index_buffer;
            image = other.image;
            sampler = other.sampler;
            gravity = other.gravity;

            other.vertex_buffer.id = SG_INVALID_ID;
            other.index_buffer.id = SG_INVALID_ID;
            other.image.id = SG_INVALID_ID;
            other.sampler.id = SG_INVALID_ID;
        }
        return *this;
    }

    ~ParticleSystem() {
        if (vertex_buffer.id != SG_INVALID_ID) sg_destroy_buffer(vertex_buffer);
        if (index_buffer.id != SG_INVALID_ID) sg_destroy_buffer(index_buffer);
        if (image.id != SG_INVALID_ID) sg_destroy_image(image);
        if (sampler.id != SG_INVALID_ID) sg_destroy_sampler(sampler);
    }
};

#endif //PARTICLESYSTEM_H
