#pragma once

#include <rclcpp/rclcpp.hpp>
#include <rclcpp/time_source.hpp>
#include <rosgraph_msgs/msg/clock.hpp>
#include <utility>

#include "controller/sapien_controllable_articulation.h"
#include "device/PS3_publisher.h"
#include "event_system/event_system.h"
#include "robot_manager.h"
#include "robot_loader.h"
#include "sapien_scene.h"

namespace sapien {
class SScene;
namespace ros2 {

class RobotManager;
class RobotLoader;

class SceneManager : public IEventListener<EventStep> {
  friend RobotLoader;
  friend RobotManager;

protected:
  SScene *mScene;
  rclcpp::Node::SharedPtr mNode;
  std::string mNameSpace;

  // Cache
  std::vector<std::unique_ptr<RobotManager>> mRobotManagers;
  std::vector<std::unique_ptr<SControllableArticulationWrapper>> mArticulationWrappers;

  // Clock and Time Manage
  rclcpp::TimeSource mTS;
  rclcpp::Clock::SharedPtr mClock;
  rclcpp::Time mTime;
  rclcpp::Publisher<rosgraph_msgs::msg::Clock>::SharedPtr mClockPub;
  float mTimeStep = 0;

  // Thread and spin
  std::thread mThread;
  rclcpp::executors::SingleThreadedExecutor mExecutor;

  // PS3
  std::unique_ptr<PS3Publisher> ps3Publisher = nullptr;

protected:
public:
  explicit SceneManager(SScene *scene, const std::string &name) : mScene(scene), mExecutor() {
    // Namespace of scene manager should be the same as the scene
    if (!scene->getName().empty() && scene->getName() != name)
      RCLCPP_WARN(mNode->get_logger(), "Name of scene will be updated, original: %s, now: %s",
                  scene->getName().c_str(), name.c_str());
    mNameSpace = "/" + name;
    scene->setName(name);
    mNode = rclcpp::Node::make_shared(name);
    // Set the use_sim_time
    auto parametersClient = rclcpp::SyncParametersClient(mNode.get());
    using namespace std::chrono_literals;
    parametersClient.wait_for_service(0.1s);

    auto set_parameters_results =
        parametersClient.set_parameters({rclcpp::Parameter("use_sim_time", true)});
    for (auto &result : set_parameters_results) {
      assert(result.successful);
    }
    rclcpp::spin_some(mNode->shared_from_this());

    // ROS Time Clock which is different wall time and steady time, given by /clock topic
    mTS.attachNode(mNode);
    mTime = rclcpp::Time(0, 0, RCL_ROS_TIME);
    mClock = rclcpp::Clock::make_shared(RCL_ROS_TIME);
    mTS.attachClock(mClock);

    mClockPub = mNode->create_publisher<rosgraph_msgs::msg::Clock>("/clock", 10);
    if (!mClock->ros_time_is_active()) {
      RCLCPP_WARN(mNode->get_logger(), "ROS Time is not active for scene with name %s",
                  scene->getName().c_str());
    } else {
      RCLCPP_INFO(mNode->get_logger(), "Current time of scene %s is %u \n",
                  mScene->getName().c_str(), mClock->now().nanoseconds());
    }

    // Register Simulation Callback
    mScene->registerListener(*this);

    // Add it to executor
    mExecutor.add_node(mNode);
  };

  ~SceneManager() { mScene->unregisterListener(*this); };

  RobotManager *buildRobotManager(SArticulation *articulation, const std::string &robotName) {
    auto robotNameSpace = mNameSpace;
    auto wrapper = std::make_unique<SControllableArticulationWrapper>(articulation, mClock);
    auto robotManager =
        std::make_unique<RobotManager>(wrapper.get(), robotNameSpace, robotName, mClock);
    auto robotMangerWeakPtr = robotManager.get();
    robotManager->mSceneManager = this;

    // Register Scene Call Back
    mScene->registerListener(*wrapper);

    // Maintain Cache and add to executor
    mExecutor.add_node(robotManager->mNode->shared_from_this());
    mRobotManagers.push_back(std::move(robotManager));
    mArticulationWrappers.push_back(std::move(wrapper));

    return robotMangerWeakPtr;
  };

  void start() {
    for (auto &mRobotManager : mRobotManagers) {
      mRobotManager->start();
    }
    mThread = std::thread(&rclcpp::executors::SingleThreadedExecutor::spin, &mExecutor);
  }

  std::unique_ptr<RobotLoader> createRobotLoader() { return std::make_unique<RobotLoader>(this); };

  void createPS3Publisher(double pubFrequency) {
    if (ps3Publisher) {
      RCLCPP_WARN(mNode->get_logger(),
                  "PS3 publisher Node has already been created for this Scene Manager");
      RCLCPP_WARN(mNode->get_logger(), "Scene Manager will use the original PS3 pub node");
      return;
    }

    ps3Publisher = std::make_unique<PS3Publisher>(mNameSpace, mNode->shared_from_this(), mClock,
                                                  pubFrequency);
  }

  rclcpp::Time now() { return mClock->now(); };

protected:
  void onEvent(EventStep &event) override {
    // Update time step and publish the /clock message
    float currentTimeStep = event.timeStep;
    mTime = mTime + rclcpp::Duration(rcl_duration_value_t(currentTimeStep * 1e9));
    rosgraph_msgs::msg::Clock clockMessage;
    clockMessage.set__clock(mTime);
    mClockPub->publish(clockMessage);

    // Fetch current information and add control signal to system for each robot
    // Update each robot if time step change
    bool changeTimeStep = false;
    if (abs(currentTimeStep - mTimeStep) > 1e-7) {
      changeTimeStep = true;
      mTimeStep = currentTimeStep;
    }
    for (auto &mRobotManager : mRobotManagers) {
      mRobotManager->step(changeTimeStep, currentTimeStep);
    }
  };
};

} // namespace ros2
} // namespace sapien
