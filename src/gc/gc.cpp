#include <algorithm>
#include <cstddef>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>
using std::string;
template<typename T>
using vec = std::vector<T>;
template<typename T>
using uset = std::unordered_set<T>;

class Object;

class GC {
public:
    void addRoot(Object *obj) {
        roots.insert(obj);
    }
    void removeRoot(Object *obj) {
        roots.erase(obj);
    }
    vec<Object *> &getObjects() {
        return objects;
    }
    uset<Object *> &getRoots() {
        return roots;
    }
    void collect();

private:
    vec<Object *> objects{};
    uset<Object *> roots{};
};

GC &getGC() {
    static GC gc;
    return gc;
}

class Object {
public:
    Object() : is_marked(false) {
        getGC().getObjects().push_back(this);
    }

    virtual ~Object() {}
    virtual void print() const = 0;

    void addReference(Object *obj) {
        references.push_back(obj);
    }
    vec<Object *> &getReferences() {
        return references;
    }
    bool isMarked() const {
        return is_marked;
    }
    void setMark(bool mark) {
        is_marked = mark;
    }

private:
    bool is_marked;
    vec<Object *> references;
};

class Node : public Object {
public:
    Node(const string &name) : name(name) {}
    void print() const override {
        std::cout << name;
    }

private:
    string name;
};


void GC::collect() {
    std::cout << "gc begin" << std::endl;

    vec<Object *> stacks{roots.begin(), roots.end()};
    size_t index = 0;

    while (index < stacks.size()) {
        auto object = stacks[index++];
        if (!object->isMarked()) {
            object->setMark(true);

            for (auto &ref: object->getReferences()) {
                if (ref && !ref->isMarked()) {
                    stacks.push_back(ref);
                }
            }
        }
    }

    auto it = objects.begin();
    while (it != objects.end()) {
        Object *obj = *it;
        if (!obj->isMarked()) {
            std::cout << "free:";
            obj->print();
            std::cout << std::endl;
            it = objects.erase(it);
        } else {
            obj->setMark(false);
            it++;
        }
    }

    std::cout << "gc end" << std::endl;
}

int main() {

    GC &gc = getGC();

    Node *nodeA = new Node("nodeA");
    Node *nodeB = new Node("nodeB");
    Node *nodeC = new Node("nodeC");
    Node *nodeD = new Node("nodeD");

    nodeA->addReference(nodeB);
    nodeB->addReference(nodeC);
    nodeC->addReference(nodeA);
    nodeD->addReference(nodeB);
    gc.addRoot(nodeA);
    gc.addRoot(nodeD);
    gc.collect();

    gc.removeRoot(nodeA);
    gc.collect();

    gc.removeRoot(nodeD);
    gc.collect();

    return 0;
}