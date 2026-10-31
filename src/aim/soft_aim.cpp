#include "aim/soft_aim.hpp"

#include "offsets/offsets.hpp"

#include <Windows.h>

#include <cmath>
#include <optional>

namespace aim {

namespace {

std::optional<sdk::QAngle> read_eye_angles(const memory::Process& process,
                                           std::uintptr_t local_pawn, bool prefer_v_angle) {
    if (local_pawn && prefer_v_angle) {
        sdk::QAngle ang{};
        if (process.read_raw(local_pawn + sdk::offsets::schemas::v_angle, &ang, sizeof(ang))) {
            if (std::isfinite(ang.pitch) && std::isfinite(ang.yaw))
                return ang;
        }
    }

    if (local_pawn) {
        sdk::QAngle ang{};
        if (process.read_raw(local_pawn + sdk::offsets::schemas::m_angEyeAngles, &ang,
                             sizeof(ang))) {
            if (std::isfinite(ang.pitch) && std::isfinite(ang.yaw) &&
                std::fabs(ang.pitch) <= 89.5f)
                return ang;
        }
    }

    const std::uintptr_t client = process.client_base();
    if (!client)
        return std::nullopt;

    sdk::QAngle ang{};
    if (!process.read_raw(client + sdk::offsets::dwViewAngles, &ang, sizeof(ang)))
        return std::nullopt;
    return ang;
}

sdk::QAngle read_recoil_punch(const memory::Process& process, std::uintptr_t local_pawn) {
    sdk::QAngle punch{};

    if (!local_pawn)
        return punch;

    if (const auto cam =
            process.read<std::uintptr_t>(local_pawn + sdk::offsets::schemas::m_pCameraServices)) {
        if (*cam) {
            sdk::QAngle view_punch{};
            if (process.read_raw(*cam + sdk::offsets::schemas::m_vecCsViewPunchAngle, &view_punch,
                                 sizeof(view_punch))) {
                punch.pitch += view_punch.pitch;
                punch.yaw += view_punch.yaw;
            }
        }
    }

    if (const auto svc = process.read<std::uintptr_t>(
            local_pawn + sdk::offsets::schemas::m_pAimPunchServices)) {
        if (*svc) {
            sdk::QAngle aim_punch{};
            if (process.read_raw(*svc + sdk::offsets::schemas::m_unpredictableBaseAngle, &aim_punch,
                                 sizeof(aim_punch))) {
                punch.pitch += aim_punch.pitch;
                punch.yaw += aim_punch.yaw;
            }
        }
    }

    return punch;
}

bool write_view_angles(const memory::Process& process, std::uintptr_t local_pawn,
                       const sdk::QAngle& ang) {
    const std::uintptr_t client = process.client_base();
    if (!client)
        return false;

    sdk::QAngle out = ang;
    out.pitch = sdk::normalize_pitch(out.pitch);
    out.yaw = sdk::normalize_yaw(out.yaw);
    out.roll = 0.0f;

    bool ok = process.write(client + sdk::offsets::dwViewAngles, out);
    if (local_pawn) {
        ok = process.write(local_pawn + sdk::offsets::schemas::m_angEyeAngles, out) || ok;
        ok = process.write(local_pawn + sdk::offsets::schemas::v_angle, out) || ok;
    }
    return ok;
}

float clamp_step(float delta, float max_step) {
    if (delta > max_step)
        return max_step;
    if (delta < -max_step)
        return -max_step;
    return delta;
}

struct AimLockState {
    std::uintptr_t locked_pawn = 0;
};

AimLockState g_lock{};

float screen_dist_to_crosshair(const sdk::EspTarget& t, int screen_w, int screen_h) {
    const float cx = screen_w * 0.5f;
    const float cy = screen_h * 0.5f;
    const float tx = t.x + t.w * 0.5f;
    const float ty = t.y + t.h * 0.15f;
    const float dx = tx - cx;
    const float dy = ty - cy;
    return std::sqrt(dx * dx + dy * dy);
}

float angular_dist_to_head(const sdk::Vec3& eye, const sdk::QAngle& view,
                           const sdk::EspTarget& t) {
    return sdk::angular_distance(view, sdk::calc_angle(eye, t.aim_head));
}

const sdk::EspTarget* find_by_pawn(const std::vector<sdk::EspTarget>& targets,
                                   std::uintptr_t pawn) {
    if (!pawn)
        return nullptr;
    for (const auto& t : targets) {
        if (t.pawn_id == pawn)
            return &t;
    }
    return nullptr;
}

const sdk::EspTarget* pick_target(const std::vector<sdk::EspTarget>& targets, const sdk::Vec3& eye,
                                  const sdk::QAngle& view, int screen_w, int screen_h,
                                  float max_pixel_dist, float switch_hysteresis_deg,
                                  float lock_break_fov_deg, AimLockState& lock) {
    if (targets.empty() || screen_w <= 0 || screen_h <= 0)
        return nullptr;

    const sdk::EspTarget* best = nullptr;
    float best_ang = 1.0e9f;

    for (const auto& t : targets) {
        if (screen_dist_to_crosshair(t, screen_w, screen_h) >= max_pixel_dist)
            continue;
        const float ang = angular_dist_to_head(eye, view, t);
        if (ang < best_ang) {
            best_ang = ang;
            best = &t;
        }
    }

    if (!best) {
        lock.locked_pawn = 0;
        return nullptr;
    }

    const sdk::EspTarget* locked = find_by_pawn(targets, lock.locked_pawn);
    if (locked) {
        const float locked_ang = angular_dist_to_head(eye, view, *locked);
        if (locked_ang <= lock_break_fov_deg) {
            if (best == locked || best_ang + switch_hysteresis_deg >= locked_ang)
                return locked;
        }
    }

    lock.locked_pawn = best->pawn_id;
    return best;
}

bool is_spraying(const memory::Process& process, std::uintptr_t local_pawn) {
    const bool firing = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    if (!local_pawn)
        return firing;

    const auto shots =
        process.read<int>(local_pawn + sdk::offsets::schemas::m_iShotsFired).value_or(0);
    return firing && shots >= 1;
}

} // namespace

void tick(const memory::Process& process, const sdk::EntitySystem& entities,
          const std::vector<sdk::EspTarget>& targets, const bool aim_active, const int screen_w,
          const int screen_h, const SoftAimConfig& config) {
    if (!aim_active) {
        g_lock.locked_pawn = 0;
        return;
    }
    if (targets.empty())
        return;

    const auto eye = entities.local_eye_position();
    const auto local_pawn = entities.local_pawn();
    if (!eye || !local_pawn)
        return;

    const auto shots =
        process.read<int>(local_pawn + sdk::offsets::schemas::m_iShotsFired).value_or(0);
    const bool spray_mode =
        is_spraying(process, local_pawn) && shots >= config.spray_shots_threshold;

    const auto current = read_eye_angles(process, local_pawn, spray_mode);
    if (!current)
        return;

    const sdk::EspTarget* picked = pick_target(
        targets, *eye, *current, screen_w, screen_h, config.screen_fov_pixels,
        config.target_switch_hysteresis_deg, config.target_lock_break_fov_deg, g_lock);
    if (!picked)
        return;

    sdk::Vec3 aim_point = picked->aim_head;
    aim_point.z += config.head_z_offset;
    if (spray_mode)
        aim_point.z -= config.spray_chest_drop_z;

    sdk::QAngle want = sdk::calc_angle(*eye, aim_point);

    const sdk::QAngle punch = read_recoil_punch(process, local_pawn);

    float smooth = spray_mode ? config.spray_smooth : config.smooth;
    float max_step = spray_mode ? config.spray_max_step_degrees : config.max_step_degrees;
    float rcs = spray_mode ? config.rcs_scale_spray : config.rcs_scale_tap;
    const float fov = spray_mode ? config.fov_degrees * 1.35f : config.fov_degrees;

    want.pitch -= punch.pitch * rcs;
    want.yaw -= punch.yaw * rcs;
    const float dist = sdk::angular_distance(*current, want);
    if (dist > fov)
        return;

    float delta_pitch = (want.pitch - current->pitch) * smooth;
    float delta_yaw = sdk::normalize_yaw(want.yaw - current->yaw) * smooth;

    delta_pitch = clamp_step(delta_pitch, max_step);
    delta_yaw = clamp_step(delta_yaw, max_step);

    if (std::fabs(delta_pitch) < 0.008f && std::fabs(delta_yaw) < 0.008f)
        return;

    sdk::QAngle next{};
    next.pitch = sdk::normalize_pitch(current->pitch + delta_pitch);
    next.yaw = sdk::normalize_yaw(current->yaw + delta_yaw);
    next.roll = 0.0f;

    write_view_angles(process, local_pawn, next);
}

} // namespace aim
