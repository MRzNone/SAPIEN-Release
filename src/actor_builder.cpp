#include "actor_builder.h"
#include "sapien_actor.h"
#include "sapien_scene.h"
#include "simulation.h"
#include <spdlog/spdlog.h>

namespace sapien {

Simulation *ActorBuilder::getSimulation() const { return mScene->mSimulation; }

ActorBuilder::ActorBuilder(SScene *scene) : mScene(scene) {}

void ActorBuilder::removeAllShapes() { mShapeRecord.clear(); }
void ActorBuilder::removeAllVisuals() { mVisualRecord.clear(); }
int ActorBuilder::getShapeCount() const { return mShapeRecord.size(); }
int ActorBuilder::getVisualCount() const { return mVisualRecord.size(); }
void ActorBuilder::removeShapeAt(uint32_t index) {
  if (index < mShapeRecord.size()) {
    mShapeRecord.erase(mShapeRecord.begin() + index);
  }
}
void ActorBuilder::removeVisualAt(uint32_t index) {
  if (index < mVisualRecord.size()) {
    mVisualRecord.erase(mVisualRecord.begin() + index);
  }
}

void ActorBuilder::addConvexShapeFromFile(const std::string &filename, const PxTransform &pose,
                                          const PxVec3 &scale, PxMaterial *material,
                                          PxReal density, PxReal patchRadius,
                                          PxReal minPatchRadius) {
  ShapeRecord r;
  r.type = ShapeRecord::Type::SingleMesh;
  r.filename = filename;
  r.pose = pose;
  r.scale = scale;
  r.material = material;
  r.density = density;
  r.patchRadius = patchRadius;
  r.minPatchRadius = minPatchRadius;

  mShapeRecord.push_back(r);
}

void ActorBuilder::addMultipleConvexShapesFromFile(const std::string &filename,
                                                   const PxTransform &pose, const PxVec3 &scale,
                                                   PxMaterial *material, PxReal density,
                                                   PxReal patchRadius, PxReal minPatchRadius) {

  ShapeRecord r;
  r.type = ShapeRecord::Type::MultipleMeshes;
  r.filename = filename;
  r.pose = pose;
  r.scale = scale;
  r.material = material;
  r.density = density;
  r.patchRadius = patchRadius;
  r.minPatchRadius = minPatchRadius;

  mShapeRecord.push_back(r);
}

void ActorBuilder::addBoxShape(const PxTransform &pose, const PxVec3 &size, PxMaterial *material,
                               PxReal density, PxReal patchRadius, PxReal minPatchRadius) {
  ShapeRecord r;
  r.type = ShapeRecord::Type::Box;
  r.pose = pose;
  r.scale = size;
  r.material = material;
  r.density = density;
  r.patchRadius = patchRadius;
  r.minPatchRadius = minPatchRadius;

  mShapeRecord.push_back(r);
}

void ActorBuilder::addCapsuleShape(const PxTransform &pose, PxReal radius, PxReal halfLength,
                                   PxMaterial *material, PxReal density, PxReal patchRadius,
                                   PxReal minPatchRadius) {
  ShapeRecord r;
  r.type = ShapeRecord::Type::Capsule;
  r.pose = pose;
  r.radius = radius;
  r.length = halfLength;
  r.material = material;
  r.density = density;
  r.patchRadius = patchRadius;
  r.minPatchRadius = minPatchRadius;

  mShapeRecord.push_back(r);
}

void ActorBuilder::addSphereShape(const PxTransform &pose, PxReal radius, PxMaterial *material,
                                  PxReal density, PxReal patchRadius, PxReal minPatchRadius) {
  ShapeRecord r;
  r.type = ShapeRecord::Type::Sphere;
  r.pose = pose;
  r.radius = radius;
  r.material = material;
  r.density = density;
  r.patchRadius = patchRadius;
  r.minPatchRadius = minPatchRadius;

  mShapeRecord.push_back(r);
}

void ActorBuilder::addBoxVisualWithMaterial(const PxTransform &pose, const PxVec3 &size,
                                            const Renderer::PxrMaterial &material,
                                            std::string const &name) {
  VisualRecord r;
  r.type = VisualRecord::Type::Box;
  r.pose = pose;
  r.scale = size;
  r.material = material;
  r.name = name;

  mVisualRecord.push_back(r);
}

void ActorBuilder::addCapsuleVisualWithMaterial(const PxTransform &pose, PxReal radius,
                                                PxReal halfLength,
                                                const Renderer::PxrMaterial &material,
                                                std::string const &name) {
  VisualRecord r;
  r.type = VisualRecord::Type::Capsule;
  r.pose = pose;
  r.radius = radius;
  r.length = halfLength;
  r.material = material;
  r.name = name;

  mVisualRecord.push_back(r);
}

void ActorBuilder::addSphereVisualWithMaterial(const PxTransform &pose, PxReal radius,
                                               const Renderer::PxrMaterial &material,
                                               std::string const &name) {
  VisualRecord r;
  r.type = VisualRecord::Type::Sphere;
  r.pose = pose;
  r.radius = radius;
  r.material = material;
  r.name = name;

  mVisualRecord.push_back(r);
}

void ActorBuilder::addVisualFromFile(const std::string &filename, const PxTransform &pose,
                                     const PxVec3 &scale, std::string const &name) {

  VisualRecord r;
  r.type = VisualRecord::Type::Mesh;
  r.pose = pose;
  r.scale = scale;
  r.filename = filename;
  r.name = name;

  mVisualRecord.push_back(r);
}

void ActorBuilder::setMassAndInertia(PxReal mass, PxTransform const &cMassPose,
                                     PxVec3 const &inertia) {
  mUseDensity = false;
  mMass = mass;
  mCMassPose = cMassPose;
  mInertia = inertia;
}

void ActorBuilder::buildShapes(std::vector<PxShape *> &shapes,
                               std::vector<PxReal> &densities) const {
  for (auto &r : mShapeRecord) {
    auto material = r.material ? r.material : mScene->getDefaultMaterial();

    switch (r.type) {
    case ShapeRecord::Type::SingleMesh: {
      PxConvexMesh *mesh = getSimulation()->getMeshManager().loadMesh(r.filename);
      if (!mesh) {
        spdlog::get("SAPIEN")->error("Failed to load convex mesh for actor");
        continue;
      }
      PxShape *shape = getSimulation()->mPhysicsSDK->createShape(
          PxConvexMeshGeometry(mesh, PxMeshScale(r.scale)), *material, true);
      shape->setContactOffset(mScene->mDefaultContactOffset);
      if (!shape) {
        spdlog::get("SAPIEN")->critical("Failed to create shape");
        throw std::runtime_error("Failed to create shape");
      }
      shape->setLocalPose(r.pose);
      shape->setTorsionalPatchRadius(r.patchRadius);
      shape->setMinTorsionalPatchRadius(r.minPatchRadius);
      shapes.push_back(shape);
      densities.push_back(r.density);
      break;
    }

    case ShapeRecord::Type::MultipleMeshes: {
      auto meshes = getSimulation()->getMeshManager().loadMeshGroup(r.filename);
      for (auto mesh : meshes) {
        if (!mesh) {
          spdlog::get("SAPIEN")->error("Failed to load part of the convex mesh for actor");
          continue;
        }
        PxShape *shape = getSimulation()->mPhysicsSDK->createShape(
            PxConvexMeshGeometry(mesh, PxMeshScale(r.scale)), *material, true);
        shape->setContactOffset(mScene->mDefaultContactOffset);
        if (!shape) {
          spdlog::get("SAPIEN")->critical("Failed to create shape");
          throw std::runtime_error("Failed to create shape");
        }
        shape->setLocalPose(r.pose);
        shape->setTorsionalPatchRadius(r.patchRadius);
        shape->setMinTorsionalPatchRadius(r.minPatchRadius);
        shapes.push_back(shape);
        densities.push_back(r.density);
      }
      break;
    }

    case ShapeRecord::Type::Box: {
      PxShape *shape =
          getSimulation()->mPhysicsSDK->createShape(PxBoxGeometry(r.scale), *material, true);
      shape->setContactOffset(mScene->mDefaultContactOffset);
      if (!shape) {
        spdlog::get("SAPIEN")->critical("Failed to build box with scale {}, {}, {}", r.scale.x,
                                        r.scale.y, r.scale.z);
        throw std::runtime_error("Failed to create shape");
      }
      shape->setLocalPose(r.pose);
      shape->setTorsionalPatchRadius(r.patchRadius);
      shape->setMinTorsionalPatchRadius(r.minPatchRadius);
      shapes.push_back(shape);
      densities.push_back(r.density);
      break;
    }

    case ShapeRecord::Type::Capsule: {
      PxShape *shape = getSimulation()->mPhysicsSDK->createShape(
          PxCapsuleGeometry(r.radius, r.length), *material, true);
      shape->setContactOffset(mScene->mDefaultContactOffset);
      if (!shape) {
        spdlog::get("SAPIEN")->critical("Failed to build capsule with radius {}, length {}",
                                        r.radius, r.length);
        throw std::runtime_error("Failed to create shape");
      }
      shape->setLocalPose(r.pose);
      shape->setTorsionalPatchRadius(r.patchRadius);
      shape->setMinTorsionalPatchRadius(r.minPatchRadius);
      shapes.push_back(shape);
      densities.push_back(r.density);
      break;
    }

    case ShapeRecord::Type::Sphere: {
      PxShape *shape =
          getSimulation()->mPhysicsSDK->createShape(PxSphereGeometry(r.radius), *material, true);
      shape->setContactOffset(mScene->mDefaultContactOffset);
      if (!shape) {
        spdlog::get("SAPIEN")->critical("Failed to build sphere with radius {}", r.radius);
        throw std::runtime_error("Failed to create shape");
      }
      shape->setLocalPose(r.pose);
      shape->setTorsionalPatchRadius(r.patchRadius);
      shape->setMinTorsionalPatchRadius(r.minPatchRadius);
      shapes.push_back(shape);
      densities.push_back(r.density);
      break;
    }
    }
  }
}

void ActorBuilder::buildVisuals(std::vector<Renderer::IPxrRigidbody *> &renderBodies,
                                std::vector<physx_id_t> &renderIds) const {

  auto rScene = mScene->getRendererScene();
  if (!rScene) {
    return;
  }
  for (auto &r : mVisualRecord) {
    Renderer::IPxrRigidbody *body;
    switch (r.type) {
    case VisualRecord::Type::Box:
      body = rScene->addRigidbody(PxGeometryType::eBOX, r.scale, r.material);
      break;
    case VisualRecord::Type::Sphere:
      body = rScene->addRigidbody(PxGeometryType::eSPHERE, {r.radius, r.radius, r.radius},
                                  r.material);
      break;
    case VisualRecord::Type::Capsule:
      body = rScene->addRigidbody(PxGeometryType::eCAPSULE, {r.length, r.radius, r.radius},
                                  r.material);
      break;
    case VisualRecord::Type::Mesh:
      body = rScene->addRigidbody(r.filename, r.scale);
      break;
    }
    if (body) {
      physx_id_t newId = mScene->mRenderIdGenerator.next();

      renderIds.push_back(newId);
      body->setUniqueId(newId);
      body->setInitialPose(r.pose);
      renderBodies.push_back(body);

      body->setName(r.name);
    }
  }
}

void ActorBuilder::setCollisionGroup(uint32_t g1, uint32_t g2, uint32_t g3) {
  mCollisionGroup.w0 = g1;
  mCollisionGroup.w1 = g2;
  mCollisionGroup.w2 = g3;
}

void ActorBuilder::addCollisionGroup(uint32_t g1, uint32_t g2, uint32_t g3) {
  if (g1) {
    mCollisionGroup.w0 |= 1 << (g1 - 1);
  }
  if (g2) {
    mCollisionGroup.w1 |= 1 << (g2 - 1);
  }
  if (g3) {
    mCollisionGroup.w2 |= 1 << (g3 - 1);
  }
}

void ActorBuilder::resetCollisionGroup() {
  mCollisionGroup.w0 = 1;
  mCollisionGroup.w1 = 1;
  mCollisionGroup.w2 = 0;
  mCollisionGroup.w3 = 0;
}

void ActorBuilder::buildCollisionVisuals(std::vector<Renderer::IPxrRigidbody *> &collisionBodies,
                                         std::vector<PxShape *> &shapes) const {
  auto rendererScene = mScene->getRendererScene();
  if (!rendererScene) {
    return;
  }
  for (auto shape : shapes) {
    Renderer::IPxrRigidbody *cBody;
    switch (shape->getGeometryType()) {
    case PxGeometryType::eBOX: {
      PxBoxGeometry geom;
      shape->getBoxGeometry(geom);
      cBody = rendererScene->addRigidbody(PxGeometryType::eBOX, geom.halfExtents,
                                                   PxVec3{0, 0, 1});
      break;
    }
    case PxGeometryType::eSPHERE: {
      PxSphereGeometry geom;
      shape->getSphereGeometry(geom);
      cBody = rendererScene->addRigidbody(
          PxGeometryType::eSPHERE, {geom.radius, geom.radius, geom.radius}, PxVec3{0, 0, 1});
      break;
    }
    case PxGeometryType::eCAPSULE: {
      PxCapsuleGeometry geom;
      shape->getCapsuleGeometry(geom);
      cBody = rendererScene->addRigidbody(
          PxGeometryType::eCAPSULE, {geom.halfHeight, geom.radius, geom.radius}, PxVec3{0, 0, 1});
      break;
    }
    case PxGeometryType::eCONVEXMESH: {
      PxConvexMeshGeometry geom;
      shape->getConvexMeshGeometry(geom);

      std::vector<PxVec3> vertices;
      std::vector<PxVec3> normals;
      std::vector<uint32_t> triangles;

      PxConvexMesh *convexMesh = geom.convexMesh;
      const PxVec3 *convexVerts = convexMesh->getVertices();
      const PxU8 *indexBuffer = convexMesh->getIndexBuffer();
      PxU32 nbPolygons = convexMesh->getNbPolygons();
      PxU32 offset = 0;
      for (PxU32 i = 0; i < nbPolygons; i++) {
        PxHullPolygon face;
        convexMesh->getPolygonData(i, face);

        const PxU8 *faceIndices = indexBuffer + face.mIndexBase;
        for (PxU32 j = 0; j < face.mNbVerts; j++) {
          vertices.push_back(convexVerts[faceIndices[j]]);
          normals.push_back(PxVec3(face.mPlane[0], face.mPlane[1], face.mPlane[2]));
        }

        for (PxU32 j = 2; j < face.mNbVerts; j++) {
          triangles.push_back(offset);
          triangles.push_back(offset + j - 1);
          triangles.push_back(offset + j);
        }

        offset += face.mNbVerts;
      }
      cBody = rendererScene->addRigidbody(vertices, normals, triangles, geom.scale.scale,
                                                   PxVec3{0, 1, 0});
      break;
    }
    default:
      spdlog::get("SAPIEN")->error(
          "Failed to create collision shape rendering: unrecognized geometry type.");
      continue;
    }

    if (cBody) {
      cBody->setInitialPose(shape->getLocalPose());
      cBody->setVisible(false);
      cBody->setRenderMode(1);
      collisionBodies.push_back(cBody);
    }
  }
}

SActor *ActorBuilder::build(bool isKinematic, std::string const &name) const {
  physx_id_t linkId = mScene->mLinkIdGenerator.next();

  std::vector<PxShape *> shapes;
  std::vector<PxReal> densities;
  buildShapes(shapes, densities);

  std::vector<physx_id_t> renderIds;
  std::vector<Renderer::IPxrRigidbody *> renderBodies;
  buildVisuals(renderBodies, renderIds);
  for (auto body : renderBodies) {
    body->setSegmentationId(linkId);
  }

  std::vector<Renderer::IPxrRigidbody *> collisionBodies;
  buildCollisionVisuals(collisionBodies, shapes);
  for (auto body : collisionBodies) {
    body->setSegmentationId(linkId);
  }

  PxFilterData data;
  data.word0 = mCollisionGroup.w0;
  data.word1 = mCollisionGroup.w1;
  data.word2 = mCollisionGroup.w2;
  data.word3 = 0;

  PxRigidDynamic *actor =
      getSimulation()->mPhysicsSDK->createRigidDynamic(PxTransform(PxIdentity));
  actor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, isKinematic);
  for (size_t i = 0; i < shapes.size(); ++i) {
    actor->attachShape(*shapes[i]);
    shapes[i]->setSimulationFilterData(data);
    shapes[i]->release(); // this shape is now reference counted by the actor
  }
  if (shapes.size() && mUseDensity) {
    PxRigidBodyExt::updateMassAndInertia(*actor, densities.data(), shapes.size());
  } else {
    actor->setMass(mMass);
    actor->setCMassLocalPose(mCMassPose);
    actor->setMassSpaceInertiaTensor(mInertia);
  }

  auto sActor =
      std::unique_ptr<SActor>(new SActor(actor, linkId, mScene, renderBodies, collisionBodies));
  sActor->setName(name);

  sActor->mCol1 = mCollisionGroup.w0;
  sActor->mCol2 = mCollisionGroup.w1;
  sActor->mCol3 = mCollisionGroup.w2;

  actor->userData = sActor.get();

  actor->setSleepThreshold(mScene->mDefaultSleepThreshold);
  actor->setSolverIterationCounts(mScene->mDefaultSolverIterations,
                                  mScene->mDefaultSolverVelocityIterations);

  auto result = sActor.get();
  mScene->addActor(std::move(sActor));

  return result;
}

SActorStatic *ActorBuilder::buildStatic(std::string const &name) const {
  physx_id_t linkId = mScene->mLinkIdGenerator.next();

  std::vector<PxShape *> shapes;
  std::vector<PxReal> densities;
  buildShapes(shapes, densities);

  std::vector<physx_id_t> renderIds;
  std::vector<Renderer::IPxrRigidbody *> renderBodies;
  buildVisuals(renderBodies, renderIds);
  for (auto body : renderBodies) {
    body->setSegmentationId(linkId);
  }

  std::vector<Renderer::IPxrRigidbody *> collisionBodies;
  buildCollisionVisuals(collisionBodies, shapes);
  for (auto body : collisionBodies) {
    body->setSegmentationId(linkId);
  }

  PxFilterData data;
  data.word0 = mCollisionGroup.w0;
  data.word1 = mCollisionGroup.w1;
  data.word2 = mCollisionGroup.w2;
  data.word3 = 0;

  PxRigidStatic *actor = getSimulation()->mPhysicsSDK->createRigidStatic(PxTransform(PxIdentity));
  for (size_t i = 0; i < shapes.size(); ++i) {
    actor->attachShape(*shapes[i]);
    shapes[i]->setSimulationFilterData(data);
    shapes[i]->release(); // this shape is now reference counted by the actor
  }

  auto sActor = std::unique_ptr<SActorStatic>(
      new SActorStatic(actor, linkId, mScene, renderBodies, collisionBodies));
  sActor->setName(name);

  sActor->mCol1 = mCollisionGroup.w0;
  sActor->mCol2 = mCollisionGroup.w1;
  sActor->mCol3 = mCollisionGroup.w2;

  actor->userData = sActor.get();

  auto result = sActor.get();
  mScene->addActor(std::move(sActor));

  return result;
}

SActorStatic *ActorBuilder::buildGround(PxReal altitude, bool render, PxMaterial *material,
                                        Renderer::PxrMaterial const &renderMaterial,
                                        std::string const &name) {
  physx_id_t linkId = mScene->mLinkIdGenerator.next();
  material = material ? material : mScene->mDefaultMaterial;
  PxRigidStatic *ground =
      PxCreatePlane(*getSimulation()->mPhysicsSDK, PxPlane(0.f, 0.f, 1.f, -altitude), *material);
  PxShape *shape;
  ground->getShapes(&shape, 1);

  PxFilterData data;
  data.word0 = mCollisionGroup.w0;
  data.word1 = mCollisionGroup.w1;
  data.word2 = mCollisionGroup.w2;
  data.word3 = 0;

  shape->setSimulationFilterData(data);

  std::vector<Renderer::IPxrRigidbody *> renderBodies;
  if (render && mScene->getRendererScene()) {
    auto body =
        mScene->mRendererScene->addRigidbody(PxGeometryType::ePLANE, {10, 10, 10}, renderMaterial);
    body->setInitialPose(PxTransform({0, 0, altitude}, PxIdentity));
    renderBodies.push_back(body);

    physx_id_t newId = mScene->mRenderIdGenerator.next();
    body->setSegmentationId(linkId);
    body->setUniqueId(newId);
  }

  auto sActor =
      std::unique_ptr<SActorStatic>(new SActorStatic(ground, linkId, mScene, renderBodies, {}));
  sActor->setName(name);

  sActor->mCol1 = mCollisionGroup.w0;
  sActor->mCol2 = mCollisionGroup.w1;
  sActor->mCol3 = mCollisionGroup.w2;

  ground->userData = sActor.get();

  auto result = sActor.get();
  mScene->addActor(std::move(sActor));

  return result;
}

} // namespace sapien
