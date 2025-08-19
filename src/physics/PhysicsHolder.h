//
// Created by gungu on 8/15/25.
//

#ifndef PHYSICSHOLDER_H
#define PHYSICSHOLDER_H

using namespace reactphysics3d;

inline PhysicsCommon physicsCommon;
inline PhysicsWorld* world = physicsCommon.createPhysicsWorld();

class PhysicsHolder {
public:
    Object* assigned_object;
    Vector3 position;
    Quaternion orientation;
    Transform transform;
    RigidBody* body = world->createRigidBody(transform);

    // alternative: create a box collider based on mesh bounds
    void create_box_collider() const {
        if (!assigned_object || !assigned_object->mesh->vertices || assigned_object->mesh->vertex_count == 0) {
            return;
        }

        float minX = FLT_MAX, minY = FLT_MAX, minZ = FLT_MAX;
        float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;

        HMM_Mat4 scale_matrix = HMM_Scale(assigned_object->scale);

        for (size_t i = 0; i < assigned_object->mesh->vertex_count; i++) {
            float x = assigned_object->mesh->vertices[i * 8 + 0];
            float y = assigned_object->mesh->vertices[i * 8 + 1];
            float z = assigned_object->mesh->vertices[i * 8 + 2];

            HMM_Vec4 vertex = {x, y, z, 1.0f};
            HMM_Vec4 transformed = {vertex.X*assigned_object->scale.X, vertex.Y*assigned_object->scale.Y, vertex.Z*assigned_object->scale.Z, 1.0f};

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

    void assign_mesh(Object* obj) {
        assigned_object = obj;
        position.x = obj->position.X;
        position.y = obj->position.Y;
        position.z = obj->position.Z;
        orientation.x = obj->rotation.X;
        orientation.y = obj->rotation.Y;
        orientation.z = obj->rotation.Z;
        orientation.w = obj->rotation.W;
        transform = Transform(position, orientation);

        if (body) {
            world->destroyRigidBody(body);
        }
        body = world->createRigidBody(transform);

        cout << "-----" << endl;

        if (obj->mesh->vertices && obj->mesh->vertex_count > 0) {
            std::vector<Vector3> vertices;
            vertices.reserve(obj->mesh->vertex_count);

            for (size_t i = 0; i < obj->mesh->vertex_count; i++) {
                float x = obj->mesh->vertices[i * 8 + 0];
                float y = obj->mesh->vertices[i * 8 + 1];
                float z = obj->mesh->vertices[i * 8 + 2];

                HMM_Vec4 vertex = {x, y, z, 1.0f};
                HMM_Vec4 scaled = {vertex.X*assigned_object->scale.X, vertex.Y*assigned_object->scale.Y, vertex.Z*assigned_object->scale.Z, 1.0f};

                vertices.emplace_back(scaled.X, scaled.Y, scaled.Z);
            }

            std::vector<uint32_t> indices_vec;
            if (obj->mesh->use_uint16_indices && obj->mesh->indices16) {
                indices_vec.reserve(obj->mesh->index_count);
                for (size_t i = 0; i < obj->mesh->index_count; i++) {
                    indices_vec.push_back(static_cast<uint32_t>(obj->mesh->indices16[i]));
                }
            } else if (obj->mesh->indices) {
                indices_vec.assign(obj->mesh->indices, obj->mesh->indices + obj->mesh->index_count);
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
        if (assigned_object != nullptr && body != nullptr) {
            assigned_object->position.X = body->getTransform().getPosition().x;
            assigned_object->position.Y = body->getTransform().getPosition().y;
            assigned_object->position.Z = body->getTransform().getPosition().z;
            assigned_object->rotation.X = body->getTransform().getOrientation().x;
            assigned_object->rotation.Y = body->getTransform().getOrientation().y;
            assigned_object->rotation.Z = body->getTransform().getOrientation().z;
            assigned_object->rotation.W = body->getTransform().getOrientation().w;
        }
    }
};

#endif //PHYSICSHOLDER_H
