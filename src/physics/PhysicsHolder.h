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
