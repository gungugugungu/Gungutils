//
// Created by gungu on 8/19/25.
//
#ifndef OBJECT_H
#define OBJECT_H

#include <typeindex>
#include <unordered_map>
#include <memory>
#include <vector>
#include <limits>

class Object;

using ComponentID = std::type_index;

template<typename T>
ComponentID get_component_id() {
    return std::type_index(typeid(T));
}

class Component {
public:
    virtual ~Component() = default;
    virtual void update(Object* owner) = 0;
    virtual void destroy(Object* owner) {}
    virtual std::unique_ptr<Component> clone() const = 0;
};

class Object {
public:
    HMM_Vec3 position{0, 0, 0};
    HMM_Quat rotation{0, 0, 0, 1};
    HMM_Vec3 scale{1, 1, 1};
    float opacity = 1.0f;

    Mesh* mesh = nullptr;
    std::vector<Mesh*> shape_keys;
    HMM_Vec3 bounding_rect{0, 0, 0};
    bool enable_shading = true;

    int script_id = -1;

    std::unordered_map<ComponentID, std::unique_ptr<Component>> components;

    Object() = default;

    Object(const Object& other): position(other.position), rotation(other.rotation), scale(other.scale), opacity(other.opacity), mesh(other.mesh), shape_keys(other.shape_keys), bounding_rect(other.bounding_rect), enable_shading(other.enable_shading), script_id(other.script_id) {
        for (const auto& [id, comp] : other.components) {
            if (comp) {
                components[id] = comp->clone();
            }
        }
    }

    Object& operator=(const Object& other) {
        if (this != &other) {
            for (auto& [id, comp] : components) {
                comp->destroy(this);
            }
            components.clear();

            position = other.position;
            rotation = other.rotation;
            scale = other.scale;
            opacity = other.opacity;
            mesh = other.mesh;
            shape_keys = other.shape_keys;
            bounding_rect = other.bounding_rect;
            enable_shading = other.enable_shading;
            script_id = other.script_id;

            for (const auto& [id, comp] : other.components) {
                if (comp) {
                    components[id] = comp->clone();
                }
            }
        }
        return *this;
    }

    ~Object() {
        for (auto& [id, comp] : components) {
            comp->destroy(this);
        }
    }

    template<typename T, typename... Args>
    void add_component(Args&&... args) {
        components[get_component_id<T>()] = std::make_unique<T>(std::forward<Args>(args)...);
    }

    template<typename T>
    T* get_component() {
        auto it = components.find(get_component_id<T>());
        return it != components.end() ? static_cast<T*>(it->second.get()) : nullptr;
    }

    template<typename T>
    bool has_component() const {
        return components.find(get_component_id<T>()) != components.end();
    }

    template<typename T>
    void remove_component() {
        auto id = get_component_id<T>();
        auto it = components.find(id);
        if (it != components.end()) {
            it->second->destroy(this);
            components.erase(it);
        }
    }

    void update_components() {
        for (auto& [id, comp] : components) {
            comp->update(this);
        }
    }

    void select_shape_key(int index) {
        if (index >= 0 && index < static_cast<int>(shape_keys.size())) {
            mesh = shape_keys[index];
        }
    }

    void initialize_bounds() {
        bounding_rect = {0, 0, 0};
        std::vector<Mesh*> meshes = shape_keys.empty() && mesh ? std::vector<Mesh*>{mesh} : shape_keys;
        if (meshes.empty()) return;

        float max_volume = -1.0f;
        HMM_Vec3 max_size{0, 0, 0};

        for (const auto* m : meshes) {
            if (!m || m->vertex_count == 0) continue;

            float min_x = std::numeric_limits<float>::max();
            float max_x = std::numeric_limits<float>::lowest();
            float min_y = min_x;
            float max_y = max_x;
            float min_z = min_x;
            float max_z = max_x;

            for (size_t i = 0; i < m->vertex_count; ++i) {
                float x = m->vertices[i * 8];
                float y = m->vertices[i * 8 + 1];
                float z = m->vertices[i * 8 + 2];
                min_x = std::min(min_x, x);
                max_x = std::max(max_x, x);
                min_y = std::min(min_y, y);
                max_y = std::max(max_y, y);
                min_z = std::min(min_z, z);
                max_z = std::max(max_z, z);
            }

            float dx = max_x - min_x;
            float dy = max_y - min_y;
            float dz = max_z - min_z;
            float volume = dx * dy * dz;

            if (volume > max_volume) {
                max_volume = volume;
                max_size = {dx, dy, dz};
            }
        }

        if (max_volume >= 0.0f) {
            bounding_rect = max_size;
        }
    }
};

#endif