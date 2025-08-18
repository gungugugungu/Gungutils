//
// Created by gungu on 8/15/25.
//

#ifndef PHYSICSHOLDER_H
#define PHYSICSHOLDER_H
#include <reactphysics3d/reactphysics3d.h>
#include "rendering/Mesh.h"

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <array>
#include <cstdint>
#include <cmath>
#include <limits>
#include <algorithm>

using namespace reactphysics3d;

inline PhysicsCommon physicsCommon;
inline PhysicsWorld* world = physicsCommon.createPhysicsWorld();

class PhysicsHolder {
public:
    Mesh* assigned_mesh;
    Vector3 position;
    Quaternion orientation;
    Transform transform;
    RigidBody* body = world->createRigidBody(transform);

    /*static std::pair<std::vector<Vector3>, std::vector<uint32_t>> remove_duplicate_verts_with_mapping(const std::vector<Vector3>& vertices, const std::vector<uint32_t>& original_indices, float epsilon = 1e-5f) {
        if (vertices.empty() || original_indices.empty()) {
            return {vertices, original_indices};
        }

        std::vector<uint32_t> old_to_new(vertices.size());
        std::vector<Vector3> unique_vertices;

        for (uint32_t i = 0; i < vertices.size(); ++i) {
            const Vector3& current = vertices[i];

            int32_t existing_idx = -1;
            for (uint32_t j = 0; j < unique_vertices.size(); ++j) {
                const Vector3& existing = unique_vertices[j];
                float dx = current.x - existing.x;
                float dy = current.y - existing.y;
                float dz = current.z - existing.z;
                float distSq = dx*dx + dy*dy + dz*dz;

                if (distSq < epsilon * epsilon) {
                    existing_idx = j;
                    break;
                }
            }

            if (existing_idx >= 0) {
                old_to_new[i] = existing_idx;
            } else {
                old_to_new[i] = unique_vertices.size();
                unique_vertices.push_back(current);
            }
        }

        std::vector<uint32_t> new_indices;
        new_indices.reserve(original_indices.size());

        // process triangles
        for (size_t i = 0; i + 2 < original_indices.size(); i += 3) {
            uint32_t idx0 = original_indices[i + 0];
            uint32_t idx1 = original_indices[i + 1];
            uint32_t idx2 = original_indices[i + 2];

            if (idx0 >= vertices.size() || idx1 >= vertices.size() || idx2 >= vertices.size()) {
                continue;
            }

            uint32_t new_idx0 = old_to_new[idx0];
            uint32_t new_idx1 = old_to_new[idx1];
            uint32_t new_idx2 = old_to_new[idx2];

            if (new_idx0 == new_idx1 || new_idx1 == new_idx2 || new_idx2 == new_idx0) {
                continue;
            }

            // check for zero area triangles
            const Vector3& v0 = unique_vertices[new_idx0];
            const Vector3& v1 = unique_vertices[new_idx1];
            const Vector3& v2 = unique_vertices[new_idx2];

            Vector3 edge1 = {v1.x - v0.x, v1.y - v0.y, v1.z - v0.z};
            Vector3 edge2 = {v2.x - v0.x, v2.y - v0.y, v2.z - v0.z};

            Vector3 cross = {
                edge1.y * edge2.z - edge1.z * edge2.y,
                edge1.z * edge2.x - edge1.x * edge2.z,
                edge1.x * edge2.y - edge1.y * edge2.x
            };

            float area_sq = cross.x*cross.x + cross.y*cross.y + cross.z*cross.z;
            if (area_sq < epsilon * epsilon) {
                continue; // skip zero area triangles
            }

            // VERY DAMN IMPORTANT: keep original winding order, DO NOT SORT
            new_indices.push_back(new_idx0);
            new_indices.push_back(new_idx1);
            new_indices.push_back(new_idx2);
        }

        std::vector<bool> vertex_used(unique_vertices.size(), false);
        for (uint32_t idx : new_indices) {
            vertex_used[idx] = true;
        }

        std::vector<Vector3> final_vertices;
        std::vector<uint32_t> vertex_remap(unique_vertices.size());

        for (uint32_t i = 0; i < unique_vertices.size(); ++i) {
            if (vertex_used[i]) {
                vertex_remap[i] = final_vertices.size();
                final_vertices.push_back(unique_vertices[i]);
            }
        }

        for (uint32_t& idx : new_indices) {
            idx = vertex_remap[idx];
        }

        return {std::move(final_vertices), std::move(new_indices)};
    }*/ // why'd I even care, quickhull exists lmao

    // alternative: create a box collider based on mesh bounds
    void create_box_collider() const {
        if (!assigned_mesh || !assigned_mesh->vertices || assigned_mesh->vertex_count == 0) {
            return;
        }

        float minX = FLT_MAX, minY = FLT_MAX, minZ = FLT_MAX;
        float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;

        HMM_Mat4 scale_matrix = HMM_Scale(assigned_mesh->scale);

        for (size_t i = 0; i < assigned_mesh->vertex_count; i++) {
            float x = assigned_mesh->vertices[i * 8 + 0];
            float y = assigned_mesh->vertices[i * 8 + 1];
            float z = assigned_mesh->vertices[i * 8 + 2];

            HMM_Vec4 vertex = {x, y, z, 1.0f};
            HMM_Vec4 transformed = {vertex.X*assigned_mesh->scale.X, vertex.Y*assigned_mesh->scale.Y, vertex.Z*assigned_mesh->scale.Z, 1.0f};

            minX = std::min(minX, transformed.X);
            minY = std::min(minY, transformed.Y);
            minZ = std::min(minZ, transformed.Z);
            maxX = std::max(maxX, transformed.X);
            maxY = std::max(maxY, transformed.Y);
            maxZ = std::max(maxZ, transformed.Z);
        }

        Vector3 halfExtents((maxX - minX) * 0.5f, (maxY - minY) * 0.5f, (maxZ - minZ) * 0.5f);

        halfExtents.x = std::max(halfExtents.x, 0.01f);
        halfExtents.y = std::max(halfExtents.y, 0.01f);
        halfExtents.z = std::max(halfExtents.z, 0.01f);

        BoxShape* boxShape = physicsCommon.createBoxShape(halfExtents);
        Transform colliderTransform = Transform::identity();
        body->addCollider(boxShape, colliderTransform);
    }

    void assign_mesh(Mesh* mesh) {
        assigned_mesh = mesh;
        position.x = mesh->position.X;
        position.y = mesh->position.Y;
        position.z = mesh->position.Z;
        orientation.x = mesh->rotation.X;
        orientation.y = mesh->rotation.Y;
        orientation.z = mesh->rotation.Z;
        orientation.w = mesh->rotation.W;
        transform = Transform(position, orientation);

        if (body) {
            world->destroyRigidBody(body);
        }
        body = world->createRigidBody(transform);

        cout << "-----" << endl;

        if (mesh->vertices && mesh->vertex_count > 0) {
            std::vector<Vector3> vertices;
            vertices.reserve(mesh->vertex_count);

            for (size_t i = 0; i < mesh->vertex_count; i++) {
                float x = mesh->vertices[i * 8 + 0];
                float y = mesh->vertices[i * 8 + 1];
                float z = mesh->vertices[i * 8 + 2];

                HMM_Vec4 vertex = {x, y, z, 1.0f};
                HMM_Vec4 scaled = {vertex.X*assigned_mesh->scale.X, vertex.Y*assigned_mesh->scale.Y, vertex.Z*assigned_mesh->scale.Z, 1.0f};

                vertices.emplace_back(scaled.X, scaled.Y, scaled.Z);
            }

            std::vector<uint32_t> indices_vec;
            if (mesh->use_uint16_indices && mesh->indices16) {
                indices_vec.reserve(mesh->index_count);
                for (size_t i = 0; i < mesh->index_count; i++) {
                    indices_vec.push_back(static_cast<uint32_t>(mesh->indices16[i]));
                }
            } else if (mesh->indices) {
                indices_vec.assign(mesh->indices, mesh->indices + mesh->index_count);
            }

            if (indices_vec.size() % 3 != 0) {
                cout << "bruh index count is not divisible by 3: " << indices_vec.size() << endl;
                return;
            }

            for (auto idx : indices_vec) {
                if (idx >= vertices.size()) {
                    cout << "Lol, index " << idx << " exceeds vertex count " << vertices.size() << " like great job dude :|" << endl;
                    return;
                }
            }

            if (!indices_vec.empty() && vertices.size() >= 4) {

                Vector3* vertex_data = vertices.data();

                cout << "Creating PolygonVertexArray with:" << endl;
                cout << "⤷Vertices: " << vertices.size() << endl;
                cout << "⤷Indices: " << indices_vec.size() << endl;

                VertexArray vertex_array = VertexArray(vertex_data, sizeof(Vector3), vertices.size(), VertexArray::DataType::VERTEX_FLOAT_TYPE);
                std::vector<Message> messages;
                cout << "About to create convex mesh..." << endl;
                ConvexMesh* convexMesh = physicsCommon.createConvexMesh(vertex_array, messages);

                if (convexMesh) {
                    ConvexMeshShape* shape = physicsCommon.createConvexMeshShape(convexMesh);
                    Transform colliderTransform = Transform::identity();
                    body->addCollider(shape, colliderTransform);
                    cout << "Shape attached successfully" << endl;
                } else {
                    cout << "Failed to create convex mesh. Messages:" << endl;
                    for (const auto& message : messages) {
                        cout << "  " << message.text << endl;
                    }
                    cout << "Falling back to box collider..." << endl;
                    create_box_collider();
                }
            } else {
                cout << "Not enough data for convex mesh, using box collider" << endl;
                create_box_collider();
            }
        }
        body->setTransform(transform);
    }

    void update() {
        if (assigned_mesh != nullptr && body != nullptr) {
            assigned_mesh->position.X = body->getTransform().getPosition().x;
            assigned_mesh->position.Y = body->getTransform().getPosition().y;
            assigned_mesh->position.Z = body->getTransform().getPosition().z;
            assigned_mesh->rotation.X = body->getTransform().getOrientation().x;
            assigned_mesh->rotation.Y = body->getTransform().getOrientation().y;
            assigned_mesh->rotation.Z = body->getTransform().getOrientation().z;
            assigned_mesh->rotation.W = body->getTransform().getOrientation().w;
        }
    }
};

#endif //PHYSICSHOLDER_H
