#include "CollisionChecker.h"
#include "PhysBodyProxy.h"

#include <boglfw/World.h>
#include <boglfw/math/math3D.h>

#include <bullet/BulletCollision/BroadphaseCollision/btBroadphaseProxy.h>
#include <bullet/BulletCollision/BroadphaseCollision/btDispatcher.h>
#include <bullet/BulletCollision/CollisionDispatch/btCollisionDispatcher.h>
#include <bullet/BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <bullet/BulletDynamics/Dynamics/btRigidBody.h>

void checkCollision(PhysBodyProxy* pA, PhysBodyProxy* pB, btPersistentManifold* contactManifold, bool reverse) {
	CollisionEvent ev;
	ev.pThisObj = contactManifold->getBody0();
	ev.pOtherObj = contactManifold->getBody1();
	if (reverse)
		xchg(ev.pThisObj, ev.pOtherObj);
	ev.pThisMeta = pA;
	ev.pOtherMeta = pB;
	ev.numContacts = min(contactManifold->getNumContacts(), (int)CollisionEvent::maxNumberContacts);
	// Check all contacts points
	for (int i=0; i<ev.numContacts; i++) {
		btManifoldPoint& pt = contactManifold->getContactPoint(i);
		if (pt.getDistance() < 0.f) {
			btVector3 ptA = pt.getPositionWorldOnA();
			btVector3 ptB = pt.getPositionWorldOnB();
			if (reverse)
				xchg(ptA, ptB);
			//const btVector3& normalOnB = reverse ? pt.m_normalWorldOnB;
		}
	}
	if (ev.numContacts)
		pA->onCollision.trigger(ev);
}

void CollisionChecker::update(float dt) {
	auto physWld = World::getGlobal<btDiscreteDynamicsWorld>();
	// Browse all collision pairs
	int numManifolds = physWld->getDispatcher()->getNumManifolds();
	for (int i=0; i<numManifolds; i++) {
		btPersistentManifold* contactManifold = physWld->getDispatcher()->getManifoldByIndexInternal(i);
		PhysBodyProxy* pMetaA = (PhysBodyProxy*)contactManifold->getBody0()->getUserPointer();
		PhysBodyProxy* pMetaB = (PhysBodyProxy*)contactManifold->getBody1()->getUserPointer();

		if (pMetaA == nullptr || pMetaB == nullptr) {
			assertDbg(false && "some btCollisionObject in the world doesn't contain a PhysBodyProxy user pointer");
			continue;
		}

		// each one of objects A and B treats its collision events differently
		if (pMetaA->collisionCfg[pMetaB->entityType])
			checkCollision(pMetaA, pMetaB, contactManifold, false);
		if (pMetaB->collisionCfg[pMetaA->entityType])
			checkCollision(pMetaB, pMetaA, contactManifold, true);
	}
}
