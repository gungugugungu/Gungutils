//
// Created by gungu on 8/15/25.
//

#ifndef PHYSICSHOLDER_H
#define PHYSICSHOLDER_H
#include "JoltPhysics/Jolt/Jolt.h"

// Jolt includes
#include "JoltPhysics/Jolt/RegisterTypes.h"
#include "JoltPhysics/Jolt/Core/Factory.h"
#include "JoltPhysics/Jolt/Core/TempAllocator.h"
#include "JoltPhysics/Jolt/Core/JobSystemThreadPool.h"
#include "JoltPhysics/Jolt/Physics/PhysicsSettings.h"
#include "JoltPhysics/Jolt/Physics/PhysicsSystem.h"
#include "JoltPhysics/Jolt/Physics/Collision/Shape/ConvexShape.h"
#include "JoltPhysics/Jolt/Physics/Body/BodyCreationSettings.h"
#include "JoltPhysics/Jolt/Physics/Body/BodyInterface.h"
#include "JoltPhysics/Jolt/Math/Vec3.h"
#include "JoltPhysics/Jolt/Math/Quat.h"
#include "rendering/Mesh.h"

inline Factory physicsFactory;
inline PhysicsSystem physicsSys;
inline BodyInterface& bodyInterface = physicsSys.GetBodyInterface();

class PhysicsHolder {
public:
    Mesh* assigned_mesh;
    Vec3 position;
    Quat orientation;
    Body* body;

    void assign_mesh(Mesh* mesh) {
        assigned_mesh = mesh;
        position = {mesh->position.X, mesh->position.Y, mesh->position.Z};
        orientation = {mesh->rotation.X, mesh->rotation.Y, mesh->rotation.Z, mesh->rotation.W};

        ConvexShapeSettings shape_settings = ConvexShapeSettings()

        if (mesh->vertices && mesh->vertex_count > 0) {
            std::vector<Vector3> vertices;
            vertices.reserve(mesh->vertex_count);

            HMM_Mat4 scale_matrix = HMM_Scale(mesh->scale);
            HMM_Mat4 rotation_matrix = HMM_QToM4(mesh->rotation);
            HMM_Mat4 transform_matrix = HMM_MulM4(rotation_matrix, scale_matrix);

            for (size_t i = 0; i < mesh->vertex_count; i++) {
                float x = mesh->vertices[i * 8 + 0];
                float y = mesh->vertices[i * 8 + 1];
                float z = mesh->vertices[i * 8 + 2];

                HMM_Vec4 vertex = {x, y, z, 1.0f};
                HMM_Vec4 transformed = HMM_MulM4V4(transform_matrix, vertex);

                vertices.emplace_back(transformed.X, transformed.Y, transformed.Z);
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
