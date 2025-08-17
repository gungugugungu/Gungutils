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
    Mesh* assigned_mesh;
    Vector3 position;
    Quaternion orientation;
    Transform transform;
    RigidBody* body = world->createRigidBody(transform);

    /*std::vector<Vector3> remove_duplicate_verts(const std::vector<Vector3>& vertices, float epsilon = 1e-5f) {
        std::vector<Vector3> uniqueVertices;

        for (const auto& vertex : vertices) {
            bool isDuplicate = false;
            for (const auto& unique : uniqueVertices) {
                float dx = vertex.x - unique.x;
                float dy = vertex.y - unique.y;
                float dz = vertex.z - unique.z;
                float distanceSq = dx*dx + dy*dy + dz*dz;

                if (distanceSq < epsilon * epsilon) {
                    isDuplicate = true;
                    break;
                }
            }

            if (!isDuplicate) {
                uniqueVertices.push_back(vertex);
            }
        }

        return uniqueVertices;
    }*/ // keep this function for later, might need it

    std::pair<std::vector<Vector3>, std::vector<uint32_t>> remove_duplicate_verts_with_mapping(const std::vector<Vector3>& vertices, const std::vector<uint32_t>& original_indices, float epsilon = 1e-5f) {
        std::vector<Vector3> uniqueVertices;
        std::vector<uint32_t> vertexMapping(vertices.size()); // maps old index to new index

        // ceate mapping from old vertex indices to new indices
        for (size_t i = 0; i < vertices.size(); i++) {
            bool isDuplicate = false;
            uint32_t duplicateIndex = 0;

            for (size_t j = 0; j < uniqueVertices.size(); j++) {
                float dx = vertices[i].x - uniqueVertices[j].x;
                float dy = vertices[i].y - uniqueVertices[j].y;
                float dz = vertices[i].z - uniqueVertices[j].z;
                float distanceSq = dx*dx + dy*dy + dz*dz;

                if (distanceSq < epsilon * epsilon) {
                    isDuplicate = true;
                    duplicateIndex = j;
                    break;
                }
            }

            if (!isDuplicate) {
                vertexMapping[i] = uniqueVertices.size();
                uniqueVertices.push_back(vertices[i]);
            } else {
                vertexMapping[i] = duplicateIndex;
            }
        }

        // remap the indices
        std::vector<uint32_t> newIndices;
        newIndices.reserve(original_indices.size());

        for (uint32_t oldIndex : original_indices) {
            if (oldIndex < vertexMapping.size()) {
                newIndices.push_back(vertexMapping[oldIndex]);
            } else {
                cout << "Error: Original index " << oldIndex << " exceeds vertex count " << vertices.size() << endl;
                return {uniqueVertices, {}};
            }
        }

        return {uniqueVertices, newIndices};
    }

    // alternative: create a box collider based on mesh bounds
    void create_box_collider() {
        if (!assigned_mesh || !assigned_mesh->vertices || assigned_mesh->vertex_count == 0) {
            return;
        }

        float minX = FLT_MAX, minY = FLT_MAX, minZ = FLT_MAX;
        float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;

        HMM_Mat4 scale_matrix = HMM_Scale(assigned_mesh->scale);
        HMM_Mat4 rotation_matrix = HMM_QToM4(assigned_mesh->rotation);
        HMM_Mat4 transform_matrix = HMM_MulM4(rotation_matrix, scale_matrix);

        for (size_t i = 0; i < assigned_mesh->vertex_count; i++) {
            float x = assigned_mesh->vertices[i * 8 + 0];
            float y = assigned_mesh->vertices[i * 8 + 1];
            float z = assigned_mesh->vertices[i * 8 + 2];

            HMM_Vec4 vertex = {x, y, z, 1.0f};
            HMM_Vec4 transformed = HMM_MulM4V4(transform_matrix, vertex);

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

            cout << "Original vertices: " << vertices.size() << endl;
            cout << "Original indices: " << indices_vec.size() << endl;
            auto [uniqueVertices, remappedIndices] = remove_duplicate_verts_with_mapping(vertices, indices_vec);
            cout << "Unique vertices: " << uniqueVertices.size() << endl;
            cout << "Remapped indices: " << remappedIndices.size() << endl;

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
                size_t num_faces = indices_vec.size() / 3;

                std::vector<PolygonVertexArray::PolygonFace> faces_vec(num_faces);
                for (size_t i = 0; i < num_faces; i++) {
                    faces_vec[i].indexBase = i * 3;
                    faces_vec[i].nbVertices = 3;
                }

                Vector3* vertex_data = vertices.data();
                uint32_t* index_data = indices_vec.data();
                PolygonVertexArray::PolygonFace* face_data = faces_vec.data();

                cout << "Creating PolygonVertexArray with:" << endl;
                cout << "  Vertices: " << vertices.size() << endl;
                cout << "  Indices: " << indices_vec.size() << endl;
                cout << "  Faces: " << num_faces << endl;

                PolygonVertexArray polygonArray(
                    vertices.size(),           // nbVertices
                    vertex_data,               // verticesArray
                    sizeof(Vector3),           // verticesStride
                    index_data,                // indicesArray
                    sizeof(uint32_t),          // indicesStride
                    num_faces,                 // nbFaces
                    face_data,                 // facesArray
                    PolygonVertexArray::VertexDataType::VERTEX_FLOAT_TYPE,
                    PolygonVertexArray::IndexDataType::INDEX_INTEGER_TYPE
                );

                std::vector<Message> messages;
                messages.reserve(10);

                cout << "About to create convex mesh..." << endl;
                ConvexMesh* convexMesh = physicsCommon.createConvexMesh(polygonArray, messages);

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
