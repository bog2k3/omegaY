#ifndef __BSP_H__
#define __BSP_H__

#include <boglfw/math/aabb.h>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <vector>
#include <map>

// Scroll down to BSPTree main class for the main interface.

// ----------------------------- Helper declarations here --------------------------------------------//

template<class ObjectType>
class AABBGeneratorInterface {
public:
	virtual ~AABBGeneratorInterface() {}
	// return the axis-aligned bounding box for a given object
	virtual AABB getAABB(ObjectType const&) = 0;
};

struct BSPConfig {
	AABB targetVolume;		// [targetVolume] is an AABB that encompasses all of the objects (the entire space that will be partitioned).
							// if [targetVolume] is empty, then BSPTree will compute it automatically as the reunion of the AABBs of all objects.
	glm::ivec3 maxDepth;	// [maxDepth] specifies the maximum number of divisions in each spatial direction
							// a value of 0 for an axis means unlimitted depth in that direction.
							// If a split would result in depth increasing beyond maxDepth, the split is not performed.
	glm::vec3 minCellSize;	// [minCellSize] specifies the minimum cell size for each spatial direction
							// a value of 0.f for an axis means no limit on the cell size on that axis. If a split would result in cells
							// less than the minCellSize, then the split is not performed.
	unsigned minObjects;	// number of objects in a cell above which splits are enabled. A split will only be performed on the cell
							// if the number of objects within is strictly greater than [minObjects].
							// There can exist cells with less than the minObjects number of objects.
	bool dynamic = false;	// This parameter enables the ability to dynamically add to and remove objects from the BSPTree after it has been created.
							// Only use it when you really need it, because it implies a performance cost.
};

template<class ObjectType>
class BSPTree;

template<class ObjectType>
class BSPNode;

class BSPDebugDraw;

// --------------------------------- BSPNode -----------------------------------------------//

template<class ObjectType>
class BSPNode {
public:
	using node_type = BSPNode<ObjectType>;
	using object_type = ObjectType;

	const std::vector<object_type>& objects() { return objects_; }

protected:
	friend class BSPTree<object_type>;
	friend class BSPDebugDraw;

	BSPNode(AABBGeneratorInterface<object_type>* aabbGenerator, node_type* parent, AABB aabb, std::vector<object_type> &&objects);
	void split(BSPConfig const& config);

	AABBGeneratorInterface<object_type>* aabbGenerator_ = nullptr;
	AABB aabb_;
	std::vector<object_type> objects_;
	node_type *negative_ = nullptr;
	node_type *positive_ = nullptr;
	node_type *parent_ = nullptr;
	glm::ivec3 depth_ {1, 1, 1};
	glm::vec4 splitPlane_ {0.f};	//x=a, y=b, z=c, w=d
};


// -------------------------------------------------------------------------------------------------//
// ----------------------------- BSPTree main class ------------------------------------------------//
// -------------------------------------------------------------------------------------------------//

// BSPTree that contains objects of type ObjectType.
// You must provide an implementation of AABBGeneratorInterface that returns axis-aligned bounding boxes for your objects.
template<class ObjectType>
class BSPTree {
public:
	using object_type = ObjectType;
	using node_type = BSPNode<object_type>;

	// The aabbGenerator object must exist throughout the lifetime of this BSPTree; the caller is responsible for this.
	BSPTree(BSPConfig const& config, AABBGeneratorInterface<object_type>* aabbGenerator, std::vector<object_type> &&objects);

	// Returns a list of leaf nodes that are intersected by or contained within a given AABB
	// If one or more vertices of the AABB lie strictly on the boundary of some nodes, those nodes are not returned.
	std::vector<node_type*> getNodesAABB(AABB const& aabb_) const;

	// Returns the leaf node that contains the point p. If the point is on the boundary between two nodes,
	// the "positive" node (on the positive side of the separation plane) is chosen by convention.
	// Separation planes are always oriented with the positive side in the positive direction of the corresponding world axis.
	node_type* getNodeAtPoint(glm::vec3 const& p) const;

private:
	friend class BSPDebugDraw;

	node_type *root_ = nullptr;
	std::map<object_type, node_type*> valueNodes_; // maps objects back to the nodes they belong to (only for dynamic usage)
};

#include "BSP-impl.h"

#endif // __BSP_H__
