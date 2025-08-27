//
// Created by gungu on 8/27/25.
//

#ifndef PARTICLESYSTEM_H
#define PARTICLESYSTEM_H

static float quad_vertices[] = {
    -1.0f, -1.0f, 0.0f,  0.0f, 0.0f,
     1.0f, -1.0f, 0.0f,  1.0f, 0.0f,
    -1.0f,  1.0f, 0.0f,  0.0f, 1.0f,
     1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
};

sg_pipeline particle_pipeline{.id = SG_INVALID_ID};

struct Particle {
    HMM_Vec3 position{0.0f, 0.0f, 0.0f};
    HMM_Vec3 velocity{0.0f, 0.0f, 0.0f};
    float size = 1;
};

class ParticleSystem {
public:
    vector<Particle> particles;

    sg_buffer vertex_buffer{.id = SG_INVALID_ID};
    sg_image image{.id = SG_INVALID_ID};
    sg_sampler sampler{.id = SG_INVALID_ID};

    float gravity = 0;

    void initialize(Surface *surface, int particle_amount, HMM_Vec3 pos, HMM_Vec3 initial_vel, float size) {
        sg_image_desc image_desc = {};
        image_desc.height = surface->pixels.size();
        image_desc.width = surface->pixels[0].size();
        image_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
        image_desc.usage.color_attachment = true;
        image_desc.usage.immutable = true;
        image_desc.data = surface->get_sokol_image_data();
        image = sg_make_image(&image_desc);

        sg_sampler_desc sampler_desc = {};
        sampler_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
        sampler_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
        sampler_desc.min_filter = SG_FILTER_LINEAR;
        sampler_desc.mag_filter = SG_FILTER_LINEAR;
        sampler = sg_make_sampler(&sampler_desc);

        sg_buffer_desc vertex_buffer_desc = {};
        vertex_buffer_desc.usage.vertex_buffer = true;
        vertex_buffer_desc.usage.immutable = true;
        vertex_buffer_desc.size = sizeof(quad_vertices);
        vertex_buffer_desc.data = SG_RANGE(quad_vertices);
        vertex_buffer = sg_make_buffer(&vertex_buffer_desc);

        for (int i; i<particle_amount; i++) {
            Particle particle;
            particle.position = pos;
            particle.velocity = initial_vel;
            particle.size = size;
            particles.push_back(particle);
        }
    }

    void draw_particles(float dt, HMM_Mat4 projection, HMM_Mat4 view) {
        sg_bindings bind = {};
        bind.vertex_buffers[0] = vertex_buffer;

        sg_view_desc view_desc = {};
        view_desc.texture.image = image;
        sg_view image_view = sg_make_view(&view_desc);
        bind.views[0] = image_view;
        bind.samplers[0] = sampler;

        sg_apply_bindings(bind);
        sg_apply_pipeline(particle_pipeline);

        particle_vs_params_t vs_params = {};
        vs_params.projection = projection;
        vs_params.view = view;

        for (auto& particle : particles) {
            particle.velocity.Y += gravity * dt;
            particle.position.X += particle.velocity.X * dt;
            particle.position.Y += particle.velocity.Y * dt;
            particle.position.Z += particle.velocity.Z * dt;

            HMM_Mat4 translate_mat = HMM_Translate(particle.position);
            HMM_Mat4 rot_mat = HMM_QToM4(HMM_Quat{0.0f, 0.0f, 0.0f, 1.0f});
            HMM_Mat4 scale_mat = HMM_Scale({particle.size, particle.size, particle.size});
            HMM_Mat4 model = HMM_MulM4(translate_mat, HMM_MulM4(rot_mat, scale_mat));
            vs_params.model = model;

            sg_apply_uniforms(UB_particle_vs_params, SG_RANGE(vs_params));

            sg_draw(0, 6, 1);
            cout << "particle drawn at " << particle.position.X << " " << particle.position.Y << " " << particle.position.Z << endl;
        }

        sg_destroy_view(image_view);
    }

    ~ParticleSystem() {
        sg_destroy_image(image);
        sg_destroy_sampler(sampler);
        sg_destroy_buffer(vertex_buffer);
    }
};

#endif //PARTICLESYSTEM_H
