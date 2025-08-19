//
// Created by gungu on 8/15/25.
//

#ifndef VISGROUP_H
#define VISGROUP_H
using namespace std;

class VisGroup {
public:
    string name;
    vector<Object> objects;
    bool enabled = true;
    float opacity = 1.0f;
    VisGroup(string name, vector<Object> new_objects) {
        this->name = name;
        this->objects = std::move(new_objects);
    }
};

#endif //VISGROUP_H
