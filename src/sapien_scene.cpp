#include "sapien_scene.h"
#include "actor_builder.h"
#include "articulation/articulation_builder.h"
#include "articulation/sapien_articulation.h"
#include "articulation/sapien_joint.h"
#include "articulation/sapien_kinematic_articulation.h"
#include "articulation/sapien_kinematic_joint.h"
#include "articulation/sapien_link.h"
#include "articulation/urdf_loader.h"
#include "renderer/render_interface.h"
#include "sapien_actor.h"
#include "sapien_contact.h"
#include "sapien_drive.h"
#include "simulation.h"
#include <algorithm>
#include <spdlog/spdlog.h>

#ifdef _PROFILE
#include <easy/profiler.h>
#endif

namespace sapien {

SScene::SScene(Simulation *sim, PxScene *scene, SceneConfig const &config)
    : mSimulation(sim), mPxScene(scene), mRendererScene(nullptr), mSimulationCallback(this) {
  mDefaultMaterial = sim->createPhysicalMaterial(config.static_friction, config.dynamic_friction,
                                                 config.restitution);
  mDefaultContactOffset = config.contactOffset;
  mDefaultSleepThreshold = config.sleepThreshold;
  mDefaultSolverIterations = config.solverIterations;
  mDefaultSolverVelocityIterations = config.solverVelocityIterations;

  auto renderer = sim->getRenderer();
  if (renderer) {
    mRendererScene = renderer->createScene();
  }
  mPxScene->setSimulationEventCallback(&mSimulationCallback);
}

SScene::~SScene() {
  mDefaultMaterial->release();
  if (mRendererScene) {
    mSimulation->getRenderer()->removeScene(mRendererScene);
  }
  if (mPxScene) {
    mPxScene->release();
  }
}

void SScene::addActor(std::unique_ptr<SActorBase> actor) {
  mPxScene->addActor(*actor->getPxActor());
  mLinkId2Actor[actor->getId()] = actor.get();
  mActors.push_back(std::move(actor));
}

void SScene::addArticulation(std::unique_ptr<SArticulation> articulation) {
  for (auto link : articulation->getBaseLinks()) {
    mLinkId2Link[link->getId()] = link;
  }
  mPxScene->addArticulation(*articulation->getPxArticulation());
  mArticulations.push_back(std::move(articulation));
}

void SScene::addKinematicArticulation(std::unique_ptr<SKArticulation> articulation) {
  for (auto link : articulation->getBaseLinks()) {
    mLinkId2Link[link->getId()] = link;
    mPxScene->addActor(*link->getPxActor());
  }
  mKinematicArticulations.push_back(std::move(articulation));
}

void SScene::removeCleanUp() {
  if (mRequiresRemoveCleanUp) {
    mRequiresRemoveCleanUp = false;
    // release actors
    for (auto &a : mActors) {
      if (a->isBeingDestroyed()) {
        a->getPxActor()->release();
      }
    }
    mActors.erase(std::remove_if(mActors.begin(), mActors.end(),
                                 [](auto &a) { return a->isBeingDestroyed(); }),
                  mActors.end());

    // release articulation
    for (auto &a : mArticulations) {
      if (a->isBeingDestroyed()) {
        a->getPxArticulation()->release();
      }
    }
    mArticulations.erase(std::remove_if(mArticulations.begin(), mArticulations.end(),
                                        [](auto &a) { return a->isBeingDestroyed(); }),
                         mArticulations.end());

    // release kinematic articulation
    for (auto &a : mKinematicArticulations) {
      if (a->isBeingDestroyed()) {
        for (auto l : a->getBaseLinks()) {
          l->getPxActor()->release();
        }
      }
    }
    mKinematicArticulations.erase(std::remove_if(mKinematicArticulations.begin(),
                                                 mKinematicArticulations.end(),
                                                 [](auto &a) { return a->isBeingDestroyed(); }),
                                  mKinematicArticulations.end());
  }
}

void SScene::removeActor(SActorBase *actor) {
  mRequiresRemoveCleanUp = true;
  // predestroy event
  EventActorPreDestroy e;
  e.actor = actor;
  actor->EventEmitter<EventActorPreDestroy>::emit(e);

  mLinkId2Actor.erase(actor->getId());

  // remove drives
  for (auto drive : actor->getDrives()) {
    drive->destroy();
  }

  // remove camera
  removeMountedCameraByMount(actor);

  // remove render bodies
  for (auto body : actor->getRenderBodies()) {
    body->destroy();
  }

  // remove collision bodies
  for (auto body : actor->getCollisionBodies()) {
    body->destroy();
  }

  // remove physical bodies
  mPxScene->removeActor(*actor->getPxActor());

  actor->markDestroyed();
  // actor->getPxActor()->release();
}

void SScene::removeArticulation(SArticulation *articulation) {
  mRequiresRemoveCleanUp = true;

  EventArticulationPreDestroy e;
  e.articulation = articulation;
  articulation->EventEmitter<EventArticulationPreDestroy>::emit(e);

  for (auto link : articulation->getBaseLinks()) {
    // predestroy event
    EventActorPreDestroy e;
    e.actor = link;
    link->EventEmitter<EventActorPreDestroy>::emit(e);

    // remove drives
    for (auto drive : link->getDrives()) {
      drive->destroy();
    }

    // remove camera
    removeMountedCameraByMount(link);

    // remove render bodies
    for (auto body : link->getRenderBodies()) {
      body->destroy();
    }

    // remove reference
    mLinkId2Link.erase(link->getId());
  }

  // remove physical bodies
  mPxScene->removeArticulation(*articulation->getPxArticulation());

  // mark removed
  articulation->markDestroyed();

  // articulation->getPxArticulation()->release();

  // // remove sapien articulation
  // mArticulations.erase(std::remove_if(mArticulations.begin(), mArticulations.end(),
  //                                     [articulation](auto &a) { return a.get() == articulation;
  //                                     }),
  //                      mArticulations.end());
}

void SScene::removeKinematicArticulation(SKArticulation *articulation) {
  mRequiresRemoveCleanUp = true;

  EventArticulationPreDestroy e;
  e.articulation = articulation;
  articulation->EventEmitter<EventArticulationPreDestroy>::emit(e);

  for (auto link : articulation->getBaseLinks()) {
    // predestroy event
    EventActorPreDestroy e;
    e.actor = link;
    link->EventEmitter<EventActorPreDestroy>::emit(e);

    // remove drives
    for (auto drive : link->getDrives()) {
      drive->destroy();
    }

    // remove camera
    removeMountedCameraByMount(link);

    // remove render bodies
    for (auto body : link->getRenderBodies()) {
      body->destroy();
    }

    // remove reference
    mLinkId2Link.erase(link->getId());

    // remove actor
    mPxScene->removeActor(*link->getPxActor());
    // link->getPxActor()->release();
  }

  articulation->markDestroyed();
  // mKinematicArticulations.erase(
  //     std::remove_if(mKinematicArticulations.begin(), mKinematicArticulations.end(),
  //                    [articulation](auto &a) { return a.get() == articulation; }),
  //     mKinematicArticulations.end());
}

void SScene::removeDrive(SDrive *drive) {
  if (drive->mScene != this) {
    spdlog::get("SAPIEN")->error("Failed to remove drive: drive is not in this scene.");
  }
  drive->mJoint->release();
  if (drive->mActor1) {
    drive->mActor1->removeDrive(drive);
    if (drive->mActor1->getType() == EActorType::DYNAMIC) {
      static_cast<PxRigidDynamic *>(drive->getActor1()->getPxActor())->wakeUp();
    } else if (drive->mActor1->getType() == EActorType::ARTICULATION_LINK) {
      static_cast<PxArticulationLink *>(drive->getActor1()->getPxActor())
          ->getArticulation()
          .wakeUp();
    }
  }
  if (drive->mActor2) {
    drive->mActor2->removeDrive(drive);
    if (drive->mActor2->getType() == EActorType::DYNAMIC) {
      static_cast<PxRigidDynamic *>(drive->getActor2()->getPxActor())->wakeUp();
    } else if (drive->mActor2->getType() == EActorType::ARTICULATION_LINK) {
      static_cast<PxArticulationLink *>(drive->getActor2()->getPxActor())
          ->getArticulation()
          .wakeUp();
    }
  }
  mDrives.erase(std::remove_if(mDrives.begin(), mDrives.end(),
                               [drive](auto &d) { return d.get() == drive; }),
                mDrives.end());
}

SActorBase *SScene::findActorById(physx_id_t id) const {
  auto it = mLinkId2Actor.find(id);
  if (it == mLinkId2Actor.end()) {
    return nullptr;
  }
  return it->second;
}

SLinkBase *SScene::findArticulationLinkById(physx_id_t id) const {
  auto it = mLinkId2Link.find(id);
  if (it == mLinkId2Link.end()) {
    return nullptr;
  }
  return it->second;
}

std::unique_ptr<ActorBuilder> SScene::createActorBuilder() {
  return std::make_unique<ActorBuilder>(this);
}

std::unique_ptr<ArticulationBuilder> SScene::createArticulationBuilder() {
  return std::make_unique<ArticulationBuilder>(this);
}

std::unique_ptr<URDF::URDFLoader> SScene::createURDFLoader() {
  return std::make_unique<URDF::URDFLoader>(this);
}

std::vector<Renderer::ICamera *> SScene::getMountedCameras() {
  std::vector<Renderer::ICamera *> cameras;
  cameras.reserve(mCameras.size());
  for (auto &mCamera : mCameras) {
    cameras.push_back(mCamera.camera);
  }
  return cameras;
}

std::vector<SActorBase *> SScene::getMountedActors() {
  std::vector<SActorBase *> actors;
  actors.reserve(mCameras.size());
  for (auto &mCamera : mCameras) {
    actors.push_back(mCamera.actor);
  }
  return actors;
}
Renderer::ICamera *SScene::addMountedCamera(std::string const &name, SActorBase *actor,
                                            PxTransform const &pose, uint32_t width,
                                            uint32_t height, float fovx, float fovy, float near,
                                            float far) {
  if (!mRendererScene) {
    spdlog::get("SAPIEN")->error("Failed to add camera: renderer is not added to simulation.");
    return nullptr;
  }
  auto cam = mRendererScene->addCamera(name, width, height, fovx, fovy, near, far);
  cam->setInitialPose(pose * PxTransform({0, 0, 0}, {-0.5, 0.5, 0.5, -0.5}));
  mCameras.push_back({actor, cam});
  return cam;
}

void SScene::removeMountedCamera(Renderer::ICamera *cam) {
  mRendererScene->removeCamera(cam);
  mCameras.erase(std::remove_if(mCameras.begin(), mCameras.end(),
                                [cam](MountedCamera &mc) { return mc.camera == cam; }),
                 mCameras.end());
}

Renderer::ICamera *SScene::findMountedCamera(std::string const &name, SActorBase const *actor) {
  auto it = std::find_if(mCameras.begin(), mCameras.end(), [name, actor](MountedCamera &cam) {
    return (actor == nullptr || cam.actor == actor) && cam.camera->getName() == name;
  });
  if (it != mCameras.end()) {
    return it->camera;
  } else {
    return nullptr;
  }
}

void SScene::setShadowLight(PxVec3 const &direction, PxVec3 const &color) {
  if (!mRendererScene) {
    spdlog::get("SAPIEN")->error("Failed to add light: renderer is not added to simulation.");
    return;
  }
  mRendererScene->setShadowLight({direction.x, direction.y, direction.z},
                                 {color.x, color.y, color.z});
}
void SScene::addPointLight(PxVec3 const &position, PxVec3 const &color) {
  if (!mRendererScene) {
    spdlog::get("SAPIEN")->error("Failed to add light: renderer is not added to simulation.");
    return;
  }
  mRendererScene->addPointLight({position.x, position.y, position.z}, {color.x, color.y, color.z});
}
void SScene::setAmbientLight(PxVec3 const &color) {
  if (!mRendererScene) {
    spdlog::get("SAPIEN")->error("Failed to add light: renderer is not added to simulation.");
    return;
  }
  mRendererScene->setAmbientLight({color.x, color.y, color.z});
}
void SScene::addDirectionalLight(PxVec3 const &direction, PxVec3 const &color) {
  if (!mRendererScene) {
    spdlog::get("SAPIEN")->error("Failed to add light: renderer is not added to simulation.");
    return;
  }
  mRendererScene->addDirectionalLight({direction.x, direction.y, direction.z},
                                      {color.x, color.y, color.z});
}

void SScene::step() {
#ifdef _PROFILE
  EASY_BLOCK("Pre-step processing", profiler::colors::Blue);
#endif

  for (auto &a : mActors) {
    if (!a->isBeingDestroyed())
      a->prestep();
  }
  for (auto &a : mArticulations) {
    if (!a->isBeingDestroyed())
      a->prestep();
  }
  for (auto &a : mKinematicArticulations) {
    if (!a->isBeingDestroyed())
      a->prestep();
  }

#ifdef _PROFILE
  EASY_END_BLOCK;
  EASY_BLOCK("PhysX scene Step", profiler::colors::Red);
#endif

  mPxScene->simulate(mTimestep);
  while (!mPxScene->fetchResults(true)) {
  }

#ifdef _PROFILE
  EASY_END_BLOCK;
#endif

  removeCleanUp();

  EventStep event;
  event.timeStep = getTimestep();
  emit(event);
}

void SScene::stepAsync() {
  for (auto &a : mActors) {
    if (!a->isBeingDestroyed())
      a->prestep();
  }
  for (auto &a : mArticulations) {
    if (!a->isBeingDestroyed())
      a->prestep();
  }
  for (auto &a : mKinematicArticulations) {
    if (!a->isBeingDestroyed())
      a->prestep();
  }
  mPxScene->simulate(mTimestep);
}

void SScene::stepWait() {
  while (!mPxScene->fetchResults(true)) {
  }

  removeCleanUp();

  EventStep event;
  event.timeStep = getTimestep();
  emit(event);
}

void SScene::updateRender() {
#ifdef _PROFILE
  EASY_FUNCTION("Update Render", profiler::colors::Magenta);
#endif
  if (!mRendererScene) {
    spdlog::get("SAPIEN")->error("Failed to update render: renderer is not added.");
    return;
  }
  for (auto &actor : mActors) {
    actor->updateRender(actor->getPxActor()->getGlobalPose());
  }

  for (auto &articulation : mArticulations) {
    for (auto &link : articulation->getBaseLinks()) {
      link->updateRender(link->getPxActor()->getGlobalPose());
    }
  }

  for (auto &articulation : mKinematicArticulations) {
    for (auto &link : articulation->getBaseLinks()) {
      link->updateRender(link->getPxActor()->getGlobalPose());
    }
  }

  for (auto &cam : mCameras) {
    cam.camera->setPose(cam.actor->getPxActor()->getGlobalPose());
  }
}

void SScene::addGround(PxReal altitude, bool render, PxMaterial *material,
                       Renderer::PxrMaterial const &renderMaterial) {
  createActorBuilder()->buildGround(altitude, render, material, renderMaterial, "ground");
}

void SScene::updateContact(PxShape *shape1, PxShape *shape2, std::unique_ptr<SContact> contact) {
  auto pair = std::make_pair(shape1, shape2);
  if (contact->starts) {
    if (mContacts.find(pair) != mContacts.end()) {
      spdlog::get("SAPIEN")->error("Error adding contact pair: it already exists");
    }
    mContacts[pair] = std::move(contact);
  } else if (contact->persists) {
    auto it = mContacts.find(pair);
    if (it == mContacts.end()) {
      spdlog::get("SAPIEN")->error("Error updating contact pair: it has not started");
    }
    it->second = std::move(contact);
  } else if (contact->ends) {
    auto it = mContacts.find(pair);
    if (it == mContacts.end()) {
      spdlog::get("SAPIEN")->error("Error updating contact pair: it has not started");
      return;
    }
    mContacts.erase(it);
  }
}

std::vector<SContact *> SScene::getContacts() const {
  std::vector<SContact *> contacts{};
  for (auto &it : mContacts) {
    contacts.push_back(it.second.get());
  }
  return contacts;
}

SDrive *SScene::createDrive(SActorBase *actor1, PxTransform const &pose1, SActorBase *actor2,
                            PxTransform const &pose2) {
  mDrives.push_back(std::unique_ptr<SDrive>(new SDrive(this, actor1, pose1, actor2, pose2)));
  auto drive = mDrives.back().get();
  if (actor1) {
    actor1->addDrive(drive);
    if (actor1->getType() == EActorType::DYNAMIC) {
      static_cast<PxRigidDynamic *>(actor1->getPxActor())->wakeUp();
    } else if (actor1->getType() == EActorType::ARTICULATION_LINK) {
      static_cast<PxArticulationLink *>(actor1->getPxActor())->getArticulation().wakeUp();
    }
  }
  if (actor2) {
    actor2->addDrive(drive);
    if (actor2->getType() == EActorType::DYNAMIC) {
      static_cast<PxRigidDynamic *>(actor2->getPxActor())->wakeUp();
    } else if (actor2->getType() == EActorType::ARTICULATION_LINK) {
      static_cast<PxArticulationLink *>(actor2->getPxActor())->getArticulation().wakeUp();
    }
  }
  return drive;
}

void SScene::removeMountedCameraByMount(SActorBase *actor) {
  for (auto &cam : mCameras) {
    if (cam.actor == actor) {
      mRendererScene->removeCamera(cam.camera);
    }
  }
  mCameras.erase(std::remove_if(mCameras.begin(), mCameras.end(),
                                [actor](MountedCamera &mc) { return mc.actor == actor; }),
                 mCameras.end());
}

std::vector<SActorBase *> SScene::getAllActors() const {
  std::vector<SActorBase *> output;
  for (auto &actor : mActors) {
    output.push_back(actor.get());
  }
  return output;
}
std::vector<SArticulationBase *> SScene::getAllArticulations() const {
  std::vector<SArticulationBase *> output;
  for (auto &articulation : mArticulations) {
    output.push_back(articulation.get());
  }
  for (auto &articulation : mKinematicArticulations) {
    output.push_back(articulation.get());
  }
  return output;
}

std::map<physx_id_t, std::string> SScene::findRenderId2VisualName() const {
  std::map<physx_id_t, std::string> result;
  for (auto &actor : mActors) {
    for (auto &v : actor->getRenderBodies()) {
      result[v->getUniqueId()] = v->getName();
    }
  }
  for (auto &articulation : mArticulations) {
    for (auto &actor : articulation->getBaseLinks()) {
      for (auto &v : actor->getRenderBodies()) {
        result[v->getUniqueId()] = v->getName();
      }
    }
  }
  for (auto &articulation : mKinematicArticulations) {
    for (auto &actor : articulation->getBaseLinks()) {
      for (auto &v : actor->getRenderBodies()) {
        result[v->getUniqueId()] = v->getName();
      }
    }
  }
  return result;
}

}; // namespace sapien
