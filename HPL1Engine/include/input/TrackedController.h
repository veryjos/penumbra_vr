#pragma once

#pragma once

#include "math/Math.h"
#include "openvr.h"

namespace hpl {
  class TrackedController {
  public:
    struct ButtonState {
      ButtonState(bool valid) {
        valid_ = valid;
      };

      float touchX;
      float touchY;

      bool touchContact;
      bool touchJustContacted;
      bool touchJustReleased;

      bool padPressed;
      bool padJustPressed;
      bool padJustReleased;

      bool gripPressed;
      bool gripJustPressed;
      bool gripJustReleased;

      float triggerMargin;

      bool triggerPressed;
      bool triggerJustPressed;
      bool triggerJustReleased;

      bool menuPressed;
      bool menuJustPressed;
      bool menuJustReleased;

      bool valid_;
    };

    TrackedController();
    ~TrackedController();

    void SetMatrix(const cMatrixf& matrix);
    cMatrixf GetLastMatrix();
    cMatrixf GetMatrix();

    void SetVelocity(const cVector3f& velocity);
    cVector3f GetVelocity();

    void SetAngularVelocity(const cVector3f& angular_velocity);
    cVector3f GetAngularVelocity();

    void SetDeviceIndex(vr::TrackedDeviceIndex_t device_index);

    void UpdateButtonState();
    ButtonState GetButtonState();

  private:
    uint32_t last_packet_;
    ButtonState button_state_;
    vr::TrackedDeviceIndex_t device_index_;
    cMatrixf last_matrix_;
    cMatrixf matrix_;
    cVector3f velocity_;
    cVector3f angular_velocity_;
  };
}