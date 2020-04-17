//
// Created by sim on 10/3/19.
//

#include "cartesian_velocity_controller.h"
#include <memory>
#include <sensor_msgs/JointState.h>
namespace sapien::robot {

CartesianVelocityController::CartesianVelocityController(ControllableArticulationWrapper *wrapper,
                                                         sensor_msgs::JointState *jointState,
                                                         robot_state::RobotState *robotState,
                                                         const std::string &groupName,
                                                         float timestep, ros::NodeHandle *nh,
                                                         const std::string &robotName)
    : jointStatePtr(jointState), state(robotState), mNodeHandle(nh), time_step(timestep) {
  jointModelGroup = state->getJointModelGroup(groupName);
  jointName = jointModelGroup->getVariableNames();
  jointValue.resize(jointName.size(), 0);
  jointValueFloat.resize(jointName.size(), 0);

  eeName = jointModelGroup->getOnlyOneEndEffectorTip()->getName();
  currentPose = state->getGlobalLinkTransform(eeName);
  jointStateTopicName = "/sapien/" + robotName + "/joint_states";

  // Build relative transformation matrix cache
  transStepSize = timestep * 1;
  rotStepSize = timestep * 1;
  buildCartesianAngularVelocityCache();
  buildCartesianVelocityCache();
  toggleJumpTest(true);

  // Register queue
  mQueue = std::make_unique<ThreadSafeQueue>();
  wrapper->add_position_controller(jointName, mQueue.get());

  // Create joint state related cache
  std::vector<std::string> topicJointNames = jointStatePtr->name;
  for (auto &i : jointName) {
    auto index = std::find(topicJointNames.begin(), topicJointNames.end(), i);
    if (index == topicJointNames.end()) {
      ROS_ERROR("Joint name in controller not found in joint topic: %s", i.c_str());
    } else {
      jointIndex2Topic.push_back(index - topicJointNames.begin());
    }
  }
}
void CartesianVelocityController::updateCurrentPose() {
  // Check if the joint index has already been cached
  for (size_t i = 0; i < jointValue.size(); i++) {
    jointValue[i] = jointStatePtr->position[jointIndex2Topic[i]];
  }
  state->setJointGroupPositions(jointModelGroup, jointValue);
  currentPose = state->getGlobalLinkTransform(eeName);
}
void CartesianVelocityController::moveRelative(CartesianCommand type, bool continuous) {
  if (!continuous) {
    updateCurrentPose();
  }
  Eigen::Isometry3d newPose;
  if (type < 3 || (type < 9 && type >= 6)) {
    newPose = cartesianMatrix[type] * currentPose;
  } else {
    newPose = currentPose * cartesianMatrix[type];
  }
  bool found_ik = state->setFromIK(jointModelGroup, newPose, 0.05);
  if (!found_ik) {
    ROS_WARN("Ik not found without timeout");
    return;
  }
  if (jointJumpCheck) {
    std::vector<double> currentJointValue;
    state->copyJointGroupPositions(jointModelGroup, currentJointValue);
    if (testJointSpaceJump(currentJointValue, jointValue, 0.9)) {
      ROS_WARN("Joint space jump! Not execute");
      return;
    }
  }
  state->copyJointGroupPositions(jointModelGroup, jointValue);
  currentPose = newPose;

  // Control the physx via controllable wrapper
  jointValueFloat.assign(jointValue.begin(), jointValue.end());
  mQueue->pushValue(jointValueFloat);
}
float CartesianVelocityController::getVelocity() const { return transStepSize / time_step; }
void CartesianVelocityController::setVelocity(float v) {
  transStepSize = v * time_step;
  buildCartesianVelocityCache();
}
float CartesianVelocityController::getAngularVelocity() const { return rotStepSize / time_step; }
void CartesianVelocityController::setAngularVelocity(float v) {
  rotStepSize = time_step * v;
  buildCartesianAngularVelocityCache();
}

void CartesianVelocityController::buildCartesianVelocityCache() {
  // X, Y, Z translation
  cartesianMatrix[0](0, 3) = transStepSize;
  cartesianMatrix[1](1, 3) = transStepSize;
  cartesianMatrix[2](2, 3) = transStepSize;
  cartesianMatrix[6](0, 3) = -transStepSize;
  cartesianMatrix[7](1, 3) = -transStepSize;
  cartesianMatrix[8](2, 3) = -transStepSize;
}
void CartesianVelocityController::buildCartesianAngularVelocityCache() {
  cartesianMatrix[3].matrix() << cos(rotStepSize), sin(rotStepSize), 0, 0, -sin(rotStepSize),
      cos(rotStepSize), 0, 0, 0, 0, 1, 0, 0, 0, 0, 1;
  cartesianMatrix[4].matrix() << cos(rotStepSize), 0, -sin(rotStepSize), 0, 0, 1, 0, 0,
      sin(rotStepSize), 0, cos(rotStepSize), 0, 0, 0, 0, 1;
  cartesianMatrix[5].matrix() << cos(rotStepSize), sin(rotStepSize), 0, 0, -sin(rotStepSize),
      cos(rotStepSize), 0, 0, 0, 0, 1, 0, 0, 0, 0, 1;
  cartesianMatrix[9].matrix() << cos(-rotStepSize), sin(-rotStepSize), 0, 0, -sin(-rotStepSize),
      cos(-rotStepSize), 0, 0, 0, 0, 1, 0, 0, 0, 0, 1;
  cartesianMatrix[10].matrix() << cos(-rotStepSize), 0, -sin(-rotStepSize), 0, 0, 1, 0, 0,
      sin(-rotStepSize), 0, cos(-rotStepSize), 0, 0, 0, 0, 1;
  cartesianMatrix[11].matrix() << cos(-rotStepSize), sin(-rotStepSize), 0, 0, -sin(-rotStepSize),
      cos(-rotStepSize), 0, 0, 0, 0, 1, 0, 0, 0, 0, 1;
}
void CartesianVelocityController::toggleJumpTest(bool enable) { jointJumpCheck = enable; }
bool CartesianVelocityController::testJointSpaceJump(const std::vector<double> &q1,
                                                     const std::vector<double> &q2,
                                                     double threshold) {
  double distance = 0;
  for (size_t i = 0; i < q1.size(); ++i) {
    distance += abs(q1[i] - q2[i]);
  }
  return distance / q1.size() > threshold;
}
void CartesianVelocityController::moveRelative(const std::array<float, 3> &T, MoveType type,
                                               bool continuous) {
  if (!continuous) {
    updateCurrentPose();
  }
  Eigen::Isometry3d trans = Eigen::Isometry3d::Identity();
  Eigen::Isometry3d newPose;
  bool found_ik = false;
  switch (type) {
  case WorldTranslate: {
    Eigen::Vector3d vec(T[0], T[1], T[2]);
    vec *= transStepSize;
    trans.matrix().block<3, 1>(0, 3) = vec;
    newPose = trans * currentPose;
    found_ik = state->setFromIK(jointModelGroup, newPose, 0.05);
    break;
  }
  case WorldRotate: {
    Eigen::Vector3d vec(T[0], T[1], T[2]);
    vec *= rotStepSize;
    Eigen::Matrix3d rot;
    rot = Eigen::AngleAxisd(vec[2], Eigen::Vector3d::UnitZ()) *
          Eigen::AngleAxisd(vec[1], Eigen::Vector3d::UnitY()) *
          Eigen::AngleAxisd(vec[0], Eigen::Vector3d::UnitX());
    trans.matrix().block<3, 3>(0, 0) = rot;
    newPose = trans * currentPose;
    found_ik = state->setFromIK(jointModelGroup, newPose, 0.05);
    break;
  }
  case LocalTranslate: {
    Eigen::Vector3d vec(T[0], T[1], T[2]);
    vec *= transStepSize;
    trans.matrix().block<3, 1>(0, 3) = vec;
    newPose = currentPose * trans;
    Eigen::VectorXd v(6);
    v << T[0], T[1], T[2], 0, 0, 0;
    found_ik = state->setFromDiffIK(jointModelGroup, v, eeName, transStepSize);
    break;
  }
  case LocalRotate: {
    Eigen::Vector3d vec(T[0], T[1], T[2]);
    vec *= rotStepSize;
    Eigen::Matrix3d rot;
    rot = Eigen::AngleAxisd(vec[2], Eigen::Vector3d::UnitZ()) *
          Eigen::AngleAxisd(vec[1], Eigen::Vector3d::UnitY()) *
          Eigen::AngleAxisd(vec[0], Eigen::Vector3d::UnitX());
    trans.matrix().block<3, 3>(0, 0) = rot;
    newPose = currentPose * trans;
    Eigen::VectorXd v(6);
    v << 0, 0, 0, T[0], T[1], T[2];
    found_ik = state->setFromDiffIK(jointModelGroup, v, eeName, rotStepSize);
    break;
  }
  }
  if (!found_ik) {
    ROS_WARN("Ik not found without timeout");
    return;
  }
  if (jointJumpCheck) {
    std::vector<double> currentJointValue;
    state->copyJointGroupPositions(jointModelGroup, currentJointValue);
    if (testJointSpaceJump(currentJointValue, jointValue, 0.9)) {
      ROS_WARN("Joint space jump! Not execute");
      return;
    }
  }
  state->copyJointGroupPositions(jointModelGroup, jointValue);
  currentPose = state->getGlobalLinkTransform(eeName);

  // Control the physx via controllable wrapper
  jointValueFloat.assign(jointValue.begin(), jointValue.end());
  mQueue->pushValue(jointValueFloat);
}
} // namespace sapien::robot
