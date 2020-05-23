#include "articulation/articulation_builder.h"
#include "articulation/sapien_kinematic_articulation.h"
#include "articulation/sapien_kinematic_joint.h"
#include "articulation/urdf_loader.h"
#include "renderer/optifuser_controller.h"
#include "renderer/optifuser_renderer.h"
#include "sapien_scene.h"
#include "simulation.h"
#include <iostream>

using namespace sapien;

int main() {
  Simulation sim;
  Renderer::OptifuserRenderer renderer;
  sim.setRenderer(&renderer);
  Renderer::OptifuserController controller(&renderer);

  controller.showWindow();

  auto s0 = sim.createScene();
  s0->setTimestep(1 / 480.f);

  auto loader = s0->createURDFLoader();
  loader->fixRootLink = 0;
  auto a = loader->loadKinematic("../assets/6808/mobility.urdf");
  a->setRootPose({{0, 0, -1}, PxIdentity});

  s0->setAmbientLight({0.3, 0.3, 0.3});
  s0->setShadowLight({1, -1, -1}, {.5, .5, 0.4});

  for (auto j : a->getBaseJoints()) {
    static_cast<SKJoint *>(j)->setDriveProperties(1, 1, 1);
  }

  controller.setCameraPosition(-5, 0, 0);
  controller.setCurrentScene(s0.get());

  while (!controller.shouldQuit()) {
    for (int i = 0; i < 8; ++i) {
      s0->step();
    }
    s0->updateRender();
    controller.render();
  }

  return 0;
}
