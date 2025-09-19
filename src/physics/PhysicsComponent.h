//
// Created by gungu on 8/15/25.
//

#ifndef PHYSICSHOLDER_H
#define PHYSICSHOLDER_H

inline reactphysics3d::PhysicsCommon physicsCommon;
inline reactphysics3d::PhysicsWorld* world = physicsCommon.createPhysicsWorld();

class PhysicsComponent : public Component {
public:
    reactphysics3d::Vector3 position;
    reactphysics3d::Quaternion orientation;
    reactphysics3d::Transform transform;
    reactphysics3d::RigidBody* body = world->createRigidBody(transform);

    // alternative: create a box collider based on mesh bounds
    void create_box_collider(Object* assigned_object) const {
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

        reactphysics3d::Vector3 halfExtents((maxX - minX) * 0.5f, (maxY - minY) * 0.5f, (maxZ - minZ) * 0.5f);

        halfExtents.x = std::max(halfExtents.x, 0.01f);
        halfExtents.y = std::max(halfExtents.y, 0.01f);
        halfExtents.z = std::max(halfExtents.z, 0.01f);

        reactphysics3d::BoxShape* boxShape = physicsCommon.createBoxShape(halfExtents);
        reactphysics3d::Transform colliderTransform = reactphysics3d::Transform::identity();
        body->addCollider(boxShape, colliderTransform);
    }

    void init(Object* owner) override {
        position.x = owner->position.X;
        position.y = owner->position.Y;
        position.z = owner->position.Z;
        orientation.x = owner->rotation.X;
        orientation.y = owner->rotation.Y;
        orientation.z = owner->rotation.Z;
        orientation.w = owner->rotation.W;
        transform = reactphysics3d::Transform(position, orientation);

        if (body) {
            world->destroyRigidBody(body);
        }
        body = world->createRigidBody(transform);

        cout << "-----" << endl;

        if (owner->mesh->vertices && owner->mesh->vertex_count > 0) {
            std::vector<reactphysics3d::Vector3> vertices;
            vertices.reserve(owner->mesh->vertex_count);

            for (size_t i = 0; i < owner->mesh->vertex_count; i++) {
                float x = owner->mesh->vertices[i * 8 + 0];
                float y = owner->mesh->vertices[i * 8 + 1];
                float z = owner->mesh->vertices[i * 8 + 2];

                HMM_Vec4 vertex = {x, y, z, 1.0f};
                HMM_Vec4 scaled = {vertex.X*owner->scale.X, vertex.Y*owner->scale.Y, vertex.Z*owner->scale.Z, 1.0f};

                vertices.emplace_back(scaled.X, scaled.Y, scaled.Z);
            }

            std::vector<uint32_t> indices_vec;
            if (owner->mesh->use_uint16_indices && owner->mesh->indices16) {
                indices_vec.reserve(owner->mesh->index_count);
                for (size_t i = 0; i < owner->mesh->index_count; i++) {
                    indices_vec.push_back(static_cast<uint32_t>(owner->mesh->indices16[i]));
                }
            } else if (owner->mesh->indices) {
                indices_vec.assign(owner->mesh->indices, owner->mesh->indices + owner->mesh->index_count);
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

                reactphysics3d::Vector3* vertex_data = vertices.data();

                cout << "Creating PolygonVertexArray with:" << endl;
                cout << "⤷Vertices: " << vertices.size() << endl;
                cout << "⤷Indices: " << indices_vec.size() << endl;

                reactphysics3d::VertexArray vertex_array = reactphysics3d::VertexArray(vertex_data, sizeof(reactphysics3d::Vector3), vertices.size(), reactphysics3d::VertexArray::DataType::VERTEX_FLOAT_TYPE);
                std::vector<reactphysics3d::Message> messages;
                cout << "About to create convex mesh..." << endl;
                reactphysics3d::ConvexMesh* convexMesh = physicsCommon.createConvexMesh(vertex_array, messages);

                if (convexMesh) {
                    reactphysics3d::ConvexMeshShape* shape = physicsCommon.createConvexMeshShape(convexMesh);
                    reactphysics3d::Transform colliderTransform = reactphysics3d::Transform::identity();
                    body->addCollider(shape, colliderTransform);
                    cout << "Shape attached successfully" << endl;
                } else {
                    cout << "Failed to create convex mesh. Messages:" << endl;
                    for (const auto& message : messages) {
                        cout << "  " << message.text << endl;
                    }
                    cout << "Falling back to box collider..." << endl;
                    create_box_collider(owner);
                }
            } else {
                cout << "Not enough data for convex mesh, using box collider" << endl;
                create_box_collider(owner);
            }
        }
        body->setTransform(transform);
    }

    void update(Object* owner) override {
        if (body != nullptr) {
            owner->position.X = body->getTransform().getPosition().x;
            owner->position.Y = body->getTransform().getPosition().y;
            owner->position.Z = body->getTransform().getPosition().z;
            owner->rotation.X = body->getTransform().getOrientation().x;
            owner->rotation.Y = body->getTransform().getOrientation().y;
            owner->rotation.Z = body->getTransform().getOrientation().z;
            owner->rotation.W = body->getTransform().getOrientation().w;
        }
    }
    std::unique_ptr<Component> clone() const override {
        return std::make_unique<PhysicsComponent>(*this);
    }
};

#endif //PHYSICSHOLDER_H
