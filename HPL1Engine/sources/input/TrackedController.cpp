#include "input/TrackedController.h"
#include "game\Game.h"

using namespace hpl;
using namespace vr;

namespace hpl {
  extern cGame* gGame;
}

TrackedController::TrackedController() : device_index_(-1), button_state_(true), last_packet_(0) {
  memset(&button_state_, 0, sizeof(ButtonState));
}

TrackedController::~TrackedController() {
}

void TrackedController::SetMatrix(const cMatrixf& matrix) {
  last_matrix_ = matrix_;
  matrix_ = matrix;
}

cMatrixf TrackedController::GetLastMatrix() {
  return last_matrix_;
}

cMatrixf TrackedController::GetMatrix() {
  return matrix_;
}

void TrackedController::SetVelocity(const cVector3f& velocity) {
  velocity_ = velocity;
}

cVector3f TrackedController::GetVelocity() {
  return velocity_;
}
void TrackedController::SetAngularVelocity(const cVector3f& angular_velocity) {
  angular_velocity_ = angular_velocity;
}

cVector3f TrackedController::GetAngularVelocity() {
  return angular_velocity_;
}

void TrackedController::SetDeviceIndex(vr::TrackedDeviceIndex_t index) {
  device_index_ = index;
}

void TrackedController::UpdateButtonState() {
  if (device_index_ == -1) {
    memset(&button_state_, 0, sizeof(ButtonState));

    button_state_.touchX = 0.0f;
    button_state_.touchY = 0.0f;
    button_state_.triggerMargin = 0.0f;

    return;
  }

  auto hmd = gGame->vr_hmd;

  vr::VRControllerState_t state;

  if (!hmd->GetControllerState(device_index_, &state)) {
    button_state_.valid_ = false;
    return;
  }

  button_state_.valid_ = true;

  // Grip button
  button_state_.gripJustPressed = false;
  button_state_.gripJustReleased = false;

  if (state.ulButtonPressed & (1ULL << ((int)k_EButton_Grip)))
    button_state_.gripJustPressed = !button_state_.gripPressed;
  else
    button_state_.gripJustReleased = button_state_.gripPressed;

  button_state_.gripPressed = (state.ulButtonPressed & (1ULL << ((int)k_EButton_Grip))) > 0;

  // Pad button
  button_state_.padJustPressed = false;
  button_state_.padJustReleased = false;

  if ((state.ulButtonPressed & (1ULL << ((int)k_EButton_SteamVR_Touchpad))) > 0)
    button_state_.padJustPressed = !button_state_.padPressed;
  else
    button_state_.padJustReleased = !button_state_.padPressed;

  button_state_.padPressed = (state.ulButtonPressed & (1ULL << ((int)k_EButton_SteamVR_Touchpad))) > 0;

  // Pad touched
  button_state_.touchJustContacted = false;
  button_state_.touchJustReleased = false;

  if (state.ulButtonTouched & (1ULL << ((int)k_EButton_SteamVR_Touchpad)))
    button_state_.touchJustContacted = !button_state_.touchContact;
  else
    button_state_.touchJustReleased = button_state_.touchContact;

  button_state_.touchContact = (state.ulButtonTouched & (1ULL << ((int)k_EButton_SteamVR_Touchpad))) > 0;

  // Touchpad coordinates
  button_state_.touchX = state.rAxis[0].x;
  button_state_.touchY = state.rAxis[0].y;

  // Trigger button
  button_state_.triggerJustPressed = false;
  button_state_.triggerJustReleased = false;

  if (state.ulButtonPressed & (1ULL << ((int)k_EButton_SteamVR_Trigger)))
    button_state_.triggerJustPressed = !button_state_.triggerPressed;
  else
    button_state_.triggerJustReleased = button_state_.triggerPressed;

  button_state_.triggerPressed = (state.ulButtonPressed & (1ULL << ((int)k_EButton_SteamVR_Trigger))) > 0;

  // Trigger margin
  button_state_.triggerMargin = state.rAxis[1].x;

  // Menu button
  button_state_.menuJustPressed = false;
  button_state_.menuJustReleased = false;

  if (state.ulButtonPressed & (1ULL << ((int)k_EButton_ApplicationMenu)))
    button_state_.menuJustPressed = !button_state_.menuPressed;
  else
    button_state_.menuJustReleased = button_state_.menuPressed;

  button_state_.menuPressed = (state.ulButtonPressed & (1ULL << ((int)k_EButton_ApplicationMenu))) > 0;
}

TrackedController::ButtonState TrackedController::GetButtonState() {
  return button_state_;
}