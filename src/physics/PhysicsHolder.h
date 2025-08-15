//
// Created by gungu on 8/15/25.
//

#ifndef PHYSICSHOLDER_H
#define PHYSICSHOLDER_H
#include <reactphysics3d/reactphysics3d.h>
#include "rendering/Mesh.h"
using namespace reactphysics3d;

inline PhysicsCommon physicsCommon;
inline PhysicsWorld* world = physicsCommon.createPhysicsWorld();

class PhysicsHolder {
public:
    Mesh* assigned_mesh{};
    Vector3 position;
    Quaternion orientation;
    Transform transform;
    RigidBody* body = world->createRigidBody(transform);

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

        if (mesh->vertices && mesh->vertex_count > 0) {
            std::vector<Vector3> vertices;
            vertices.reserve(mesh->vertex_count);

            for (size_t i = 0; i < mesh->vertex_count; i++) {
                float x = mesh->vertices[i * 8 + 0];
                float y = mesh->vertices[i * 8 + 1];
                float z = mesh->vertices[i * 8 + 2];
                vertices.emplace_back(x, y, z);
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

            if (!indices_vec.empty()) {
                PolygonVertexArray::PolygonFace* faces = new PolygonVertexArray::PolygonFace[mesh->index_count / 3];
                for (size_t i = 0; i < mesh->index_count / 3; i++) {
                    faces[i].indexBase = i * 3;
                    faces[i].nbVertices = 3;
                }

                PolygonVertexArray polygonArray(vertices.size(), vertices.data(), sizeof(Vector3), indices_vec.data(), sizeof(uint32_t), mesh->index_count / 3, faces, PolygonVertexArray::VertexDataType::VERTEX_FLOAT_TYPE, PolygonVertexArray::IndexDataType::INDEX_INTEGER_TYPE);

                std::vector<Message> messages;
                ConvexMesh* convexMesh = physicsCommon.createConvexMesh(polygonArray, messages);

                if (convexMesh) {
                    ConvexMeshShape* shape = physicsCommon.createConvexMeshShape(convexMesh);
                    Transform colliderTransform = Transform::identity();
                    body->addCollider(shape, colliderTransform);
                }

                delete[] faces;
            }
        }
    }

    void update() {
        if (assigned_mesh) {
        }
    }
};

#endif //PHYSICSHOLDER_H
