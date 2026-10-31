#pragma once

#include "memory/process.hpp"
#include "sdk/entity.hpp"

namespace aim {

// Hold mouse back (XBUTTON1). Tap = head + soft; spray = chest + RCS + faster tracking.
struct SoftAimConfig {
    float fov_degrees = 7.0f;
    float smooth = 0.22f;
    float max_step_degrees = 1.25f;
    float head_z_offset = -2.0f;
    float screen_fov_pixels = 220.0f;
    // Multi-target: angular pick + sticky lock (reduces flick between near/far enemies).
    float target_switch_hysteresis_deg = 1.35f;
    float target_lock_break_fov_deg = 10.5f;
    int aim_key_vk = VK_XBUTTON1;
    int aim_toggle_vk = VK_HOME; // edge toggle: memory lock on/off (also works with hold key)

    float spray_smooth = 0.42f;
    float spray_max_step_degrees = 2.4f;
    float spray_chest_drop_z = 28.0f;
    float rcs_scale_tap = 1.2f;
    float rcs_scale_spray = 2.2f;
    int spray_shots_threshold = 2;
};

void tick(const memory::Process& process, const sdk::EntitySystem& entities,
          const std::vector<sdk::EspTarget>& targets, bool aim_active, int screen_w, int screen_h,
          const SoftAimConfig& config = {});

} // namespace aim
