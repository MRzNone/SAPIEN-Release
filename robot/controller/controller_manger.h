//
// Created by sim on 10/5/19.
//
#pragma once

#include "cartesian_velocity_controller.h"
#include "controllable_articulation_wrapper.h"
#include "group_planner.h"
#include "joint_pub_node.h"
#include "joint_trajectory_controller.h"
#include "velocity_control_service.h"
#include <map>
#include <moveit/robot_model/robot_model.h>
#include <moveit/robot_model_loader/robot_model_loader.h>

namespace sapien::robot {
enum ControlMode { ARM_WORLD, ARM_LOCAL, BODY };

class ControllerManger {
protected:
  // Hold all the controller related to a manager

  // Name and handle
  std::string robotName;
  std::vector<std::string> jointName;
  std::unique_ptr<ros::NodeHandle> nh;

  // Robot and joint state
  std::unique_ptr<robot_model_loader::RobotModelLoader> loader = nullptr;
  robot_model::RobotModelPtr kinematicModel;
  std::unique_ptr<robot_state::RobotState> robotState=nullptr;
  sensor_msgs::JointState *jointState = nullptr;

  // Spinner and callback management
  ros::AsyncSpinner spinner;

public:
  // Controllers
  std::unique_ptr<JointPubNode> jointPubNode = nullptr;
  std::map<std::string, std::unique_ptr<CartesianVelocityController>>
      name2CartesianVelocityController;
  std::map<std::string, std::unique_ptr<JointVelocityController>> name2JointVelocityController;
  std::map<std::string, std::unique_ptr<GroupControllerNode>> name2GroupTrajectoryController;
  std::map<std::string, std::unique_ptr<MoveGroupPlanner>> name2MoveGroupPlanner;
  ControllableArticulationWrapper *wrapper;

public:
  float timeStep;
  ControllerManger(std::string robotName, ControllableArticulationWrapper *wrapper);

  // Function to add controllers
  void createJointPubNode(double pubFrequency);
  CartesianVelocityController *createCartesianVelocityController(const std::string &groupName);
  JointVelocityController *
  createJointVelocityController(const std::vector<std::string> &jointNames,
                                const std::string &serviceName);
  void addGroupTrajectoryController(const std::string &groupName);

  // Function to add planner and some basic function to control the robot
  MoveGroupPlanner *createGroupPlanner(const std::string &groupName);

  // Manage controller manager handle
  [[nodiscard]] ros::NodeHandle *getHandle() const;
  [[nodiscard]] std::string getRobotName() const;
  void start();
  void stop();
  void removeController(const std::string &);
  void moveBase(const PxTransform &T);
};
} // namespace sapien::robot

// TODO: do something when timestep change
