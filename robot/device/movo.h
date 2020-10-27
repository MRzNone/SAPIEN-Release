//
// Created by sim on 10/14/19.
//
#pragma once

#include "controller/controller_manger.h"
#include <memory>
namespace sapien::robot {


class MOVO {
protected:
  ControllerManger *manger;
  bool reset = true;
  bool continuous = true;
  ControlMode mode = ControlMode::BODY;

  const std::vector<std::string> gripperJoints = {
      "right_gripper_finger1_joint", "right_gripper_finger2_joint", "right_gripper_finger3_joint"};
  const std::vector<std::string> headJoints = {"pan_joint", "tilt_joint"};
  const std::vector<std::string> bodyJoints = {"linear_joint"};

  PxVec3 pos = {0, 0, 0};
  PxReal angle = 0;
  float timestep;

public:
  CartesianVelocityController *right_arm_cartesian;
  JointVelocityController *right_gripper;
  JointVelocityController *body;
  JointVelocityController *head;

  float gripper_velocity = 5;
  float wheel_velocity = 0.8;
  float body_velocity = 0.2;
  float head_velocity = 2;
  float arm_cartesian_velocity = 0.25;
  float arm_cartesian_angular_velocity = 0.7;

public:
  explicit MOVO(ControllerManger *manger);
  virtual ~MOVO() = default;
  void set_arm_velocity(float v);
  void set_arm_angular_velocity(float v);
  void set_wheel_velocity(float v);
};
} // namespace sapien::robot
