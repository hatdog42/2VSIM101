#ifndef COLLIDER_H

#include <glm/glm.hpp>
#include "GameObject.h"
#include "Transform.h"

struct Collider
{	
	Collider(float radiusIn, GameObject* ownerIn) : radius(radiusIn), owner(ownerIn) {}

	float radius;
	// Center point of the collider
	GameObject* owner{ nullptr }; // The GameObject this collider belongs to

	bool isColliding(const Collider& other)
	{
		// this can be optimized by comparing the squared distance to the squared sum of the radii, 
		// to avoid the costly square root operation in glm::length
		float distance = glm::length(owner->mTransform->position - other.owner->mTransform->position);
		return distance < (radius + other.radius);
	}
};


#endif // !COLLIDER_H
