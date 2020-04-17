#include "controller/sapien_controllable_articulation.h"
#include "manager/robot_loader.h"
#include "manager/scene_manager.h"
#include "renderer/optifuser_controller.h"
#include "renderer/optifuser_renderer.h"
#include "scene.h"
#include "simulation.h"

using namespace sapien;
void test1(int argc, char *argv[]) {
  Simulation sim;
  Renderer::OptifuserRenderer renderer;
  sim.setRenderer(&renderer);

  auto controller = Renderer::OptifuserController(&renderer);

  auto scene = sim.createScene();
  scene->setShadowLight({0, 1, -1}, {0.5, 0.5, 0.5});
  scene->setAmbientLight({0.5, 0.5, 0.5});
  controller.setCameraPosition(-0.5, 2, 0.5);
  controller.setCameraRotation(-1, 0);
  controller.setCurrentScene(scene.get());
  scene->addGround(0);
  scene->setTimestep(1.0 / 60);

  // ROS2 specified class
  rclcpp::init(argc, argv);
  ros2::SceneManager sceneManager(scene.get(), "scene1");
  ros2::RobotLoader loader(&sceneManager);
  loader.fixRootLink = true;
  auto [robot, robotManager] =
      loader.loadROS("sapien_resources", "xarm6_description/urdf/xarm6.urdf",
                     "xarm6_moveit_config/config/xarm6.srdf", "xarm6");

  robot->setRootPose({{0, 0, 0.5}, PxIdentity});
  robotManager->setDriveProperty(1000, 50, 5000, {0, 1, 2, 3, 4, 5});
  robotManager->setDriveProperty(0, 50, 50, {6, 7, 8, 9, 10, 11});

  // Test Basic Controller
  std::vector<std::string> gripperJoints = {"drive_joint",
                                            "left_finger_joint",
                                            "left_inner_knuckle_joint",
                                            "right_outer_knuckle_joint",
                                            "right_finger_joint",
                                            "right_inner_knuckle_joint"};
  robotManager->createJointPublisher(20.0f);
  auto gripperController =
      robotManager->buildJointVelocityController(gripperJoints, "gripper_joint_velocity", 0.0f);
  sceneManager.start();

  // Test IK Controller
  auto armController =
      robotManager->buildCartesianVelocityController("arm", "arm_cartesian_velocity", 0.0f);

  // Load second robot
  auto [robot2, robotManager2] = loader.load("../assets_local/robot/panda.urdf");
  robot2->setRootPose({2,0,0});

  uint32_t step = 0;
  controller.showWindow();
  while (!controller.shouldQuit()) {
    scene->step();
    scene->updateRender();
    controller.render();
    step++;

    // Balance force
    robotManager->balancePassiveForce();
    // Move arm IK
    armController->moveCartesian({0.02, 0.00, 0.02}, ros2::MoveType::WorldTranslate);

    if (step >= 500 && step < 1000) {
      gripperController->moveJoint({5, 5, 5, 5, 5, 5});
    }
    if (step == 1000) {
      gripperController->moveJoint({-0.1, -0.1, -0.1, -0.1, -0.1, -0.1}, true);
    }
  }
  //  rclcpp::shutdown();
}

int main(int argc, char *argv[]) { test1(argc, argv); }
