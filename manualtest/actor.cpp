#include "actor_builder.h"
#include "renderer/optifuser_controller.h"
#include "renderer/optifuser_renderer.h"
#include "sapien_actor.h"
#include "sapien_scene.h"
#include "simulation.h"

using namespace sapien;

int main() {
  Simulation sim;
  Renderer::OptifuserRenderer renderer;
  sim.setRenderer(&renderer);
  Renderer::OptifuserController controller(&renderer);

  controller.showWindow();

  auto s0 = sim.createScene();
  s0->addGround(-1);
  s0->setTimestep(1 / 60.f);

  auto s1 = sim.createScene();
  s1->addGround(-1);

  auto builder = s0->createActorBuilder();
  builder->addConvexShapeFromFile("/home/fx/source/sapien/assets/bottle/model.obj");
  builder->addVisualFromFile("/home/fx/source/sapien/assets/bottle/model.obj");
  auto actor = builder->build();
  actor->setPose({{0, 0, 2}, PxIdentity});
  auto shapes = actor->getCollisionShapes();
  for (auto &s : shapes) {
    auto scale = static_cast<SConvexMeshGeometry*>(s->geometry.get())->scale;
    std::cout << scale.x << " " << scale.y << " " << scale.z << std::endl;
  }

  auto r0 = static_cast<Renderer::OptifuserScene *>(s0->getRendererScene());
  r0->setAmbientLight({0.3, 0.3, 0.3});
  r0->setShadowLight({0, -1, -1}, {.5, .5, 0.4});
  controller.setCameraPosition(-5, 0, 0);

  controller.setCurrentScene(s0.get());

  while (!controller.shouldQuit()) {
    s0->updateRender();
    s0->step();

    controller.render();
  }

  return 0;
}
