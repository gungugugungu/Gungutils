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
        if (vertices.empty() || original_indices.empty()) {
            return {vertices, original_indices};
        }

        struct Key {
            long long ix, iy, iz;
            bool operator==(const Key& o) const noexcept {
                return ix == o.ix && iy == o.iy && iz == o.iz;
            }
        };
        struct KeyHasher {
            size_t operator()(const Key& k) const noexcept {
                // Mix 3x int64 into size_t
                auto mix = [](uint64_t h, uint64_t v) {
                    h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
                    return h;
                };
                uint64_t h = static_cast<uint64_t>(k.ix);
                h = mix(h, static_cast<uint64_t>(k.iy));
                h = mix(h, static_cast<uint64_t>(k.iz));
                return static_cast<size_t>(h);
            }
        };
        auto quantize = [&](const Vector3& v) -> Key {
            const double inv = 1.0 / static_cast<double>(epsilon);
            auto q = [&](double f) -> long long {
                return static_cast<long long>(std::llround(f * inv));
            };
            return Key{ q(v.x), q(v.y), q(v.z) };
        };
        auto sub = [](const Vector3& a, const Vector3& b) -> Vector3 {
            return {a.x - b.x, a.y - b.y, a.z - b.z};
        };
        auto sqrLen = [&](const Vector3& v) -> float {
            return v.x*v.x + v.y*v.y + v.z*v.z;
        };

        std::vector<uint8_t> used(vertices.size(), 0);
        for (uint32_t idx : original_indices) {
            if (idx < vertices.size()) used[idx] = 1;
        }

        std::vector<Vector3> unique_vertices;
        unique_vertices.reserve(vertices.size());

        std::unordered_map<Key, uint32_t, KeyHasher> key_to_new;
        key_to_new.reserve(vertices.size());

        const uint32_t INVALID = std::numeric_limits<uint32_t>::max();
        std::vector<uint32_t> old2new(vertices.size(), INVALID);

        for (uint32_t i = 0; i < vertices.size(); ++i) {
            if (!used[i]) continue;
            const auto& v = vertices[i];
            const Key k = quantize(v);
            auto it = key_to_new.find(k);
            if (it != key_to_new.end()) {
                old2new[i] = it->second;
            } else {
                uint32_t ni = static_cast<uint32_t>(unique_vertices.size());
                key_to_new.emplace(k, ni);
                unique_vertices.push_back(v);
                old2new[i] = ni;
            }
        }

        std::vector<uint32_t> remapped_indices;
        remapped_indices.reserve(original_indices.size());

        struct TriKeyHasher {
            size_t operator()(const std::array<uint32_t,3>& t) const noexcept {
                uint64_t a = t[0], b = t[1], c = t[2];
                uint64_t h = a * 0x9e3779b1u;
                h ^= b + 0x9e3779b97f4a7c15ULL + (h<<6) + (h>>2);
                h ^= c + 0x9e3779b97f4a7c15ULL + (h<<6) + (h>>2);
                return static_cast<size_t>(h);
            }
        };
        std::unordered_set<std::array<uint32_t,3>, TriKeyHasher> tri_seen;
        tri_seen.reserve(original_indices.size() / 3);

        auto valid_old = [&](uint32_t idx) -> bool {
            return idx < vertices.size() && old2new[idx] != INVALID;
        };

        const float eps2 = epsilon * epsilon;

        for (size_t i = 0; i + 2 < original_indices.size(); i += 3) {
            uint32_t ia = original_indices[i + 0];
            uint32_t ib = original_indices[i + 1];
            uint32_t ic = original_indices[i + 2];

            if (!valid_old(ia) || !valid_old(ib) || !valid_old(ic)) {
                continue;
            }

            uint32_t a = old2new[ia];
            uint32_t b = old2new[ib];
            uint32_t c = old2new[ic];

            if (a == b || b == c || c == a) {
                continue;
            }

            const Vector3& va = unique_vertices[a];
            const Vector3& vb = unique_vertices[b];
            const Vector3& vc = unique_vertices[c];
            if (sqrLen(sub(va, vb)) <= eps2 ||
                sqrLen(sub(vb, vc)) <= eps2 ||
                sqrLen(sub(vc, va)) <= eps2) {
                continue;
                }

            std::array<uint32_t,3> canon = {a, b, c};
            std::sort(canon.begin(), canon.end());
            if (!tri_seen.insert(canon).second) {
                continue;
            }

            remapped_indices.push_back(a);
            remapped_indices.push_back(b);
            remapped_indices.push_back(c);
        }

        if (remapped_indices.empty()) {
            return {{}, {}};
        }

        std::vector<uint32_t> new2packed(unique_vertices.size(),
                                         std::numeric_limits<uint32_t>::max());
        std::vector<Vector3> packed_vertices;
        packed_vertices.reserve(unique_vertices.size());

        uint32_t next = 0;
        for (uint32_t& idx : remapped_indices) {
            uint32_t& p = new2packed[idx];
            if (p == std::numeric_limits<uint32_t>::max()) {
                p = next++;
                packed_vertices.push_back(unique_vertices[idx]);
            }
            idx = p;
        }

        return { std::move(packed_vertices), std::move(remapped_indices) };
    }

    // alternative: create a box collider based on mesh bounds
    void create_box_collider() {
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

            HMM_Mat4 scale_matrix = HMM_Scale(mesh->scale);

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

            cout << "Original vertices: " << vertices.size() << endl;
            cout << "Original indices: " << indices_vec.size() << endl;
            auto [uniqueVertices, remappedIndices] = remove_duplicate_verts_with_mapping(vertices, indices_vec);
            cout << "Unique vertices: " << uniqueVertices.size() << endl;
            cout << "Remapped indices: " << remappedIndices.size() << endl;

            vertices = uniqueVertices;
            indices_vec = remappedIndices;

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
                cout << "⤷Vertices: " << vertices.size() << endl;
                cout << "⤷Indices: " << indices_vec.size() << endl;
                cout << "⤷Faces: " << num_faces << endl;

                PolygonVertexArray polygonArray(
                    vertices.size(),            // nbVertices
                    vertex_data,                // verticesArray
                    sizeof(Vector3),            // verticesStride
                    index_data,                 // indicesArray
                    sizeof(uint32_t),           // indicesStride
                    num_faces,                  // nbFaces
                    face_data,                  // facesArray
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
