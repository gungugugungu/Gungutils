//
// Created by gungu on 8/15/25.
//

#ifndef VISGROUP_H
#define VISGROUP_H
using namespace std;

class VisGroup {
public:
    string name;
    vector<Mesh> meshes;
    bool enabled = true;
    float opacity = 1.0f;
    VisGroup(string name, vector<Mesh> new_meshes) {
        this->name = name;
        this->meshes = std::move(new_meshes);
    }
};

#endif //VISGROUP_H
