#ifndef GAMEOBJECT_H
#define GAMEOBJECT_H

class GameObject
{
public:
    GameObject() {};

    // Virtual update function that can be overridden by derived classes for specific behavior
    virtual void update() {};

    struct Transform* mTransform{nullptr};
    int mMesh{0};
    int mTexture{0};
	class Collider* mCollider{ nullptr };
};

#endif // GAMEOBJECT_H
