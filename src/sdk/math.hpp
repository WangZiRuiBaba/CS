#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace sdk {

struct Vec3 {
    float x{};
    float y{};
    float z{};

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
};

struct Vec2 {
    float x{};
    float y{};
};

struct QAngle {
    float pitch{};
    float yaw{};
    float roll{};
};

constexpr float kDegToRad = 3.14159265f / 180.0f;
constexpr float kRadToDeg = 180.0f / 3.14159265f;

inline float normalize_yaw(float yaw) {
    while (yaw > 180.0f)
        yaw -= 360.0f;
    while (yaw < -180.0f)
        yaw += 360.0f;
    return yaw;
}

inline float normalize_pitch(float pitch) {
    if (pitch > 89.0f)
        return 89.0f;
    if (pitch < -89.0f)
        return -89.0f;
    return pitch;
}

inline QAngle calc_angle(const Vec3& from, const Vec3& to) {
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float dz = to.z - from.z;
    const float hyp = std::sqrt(dx * dx + dy * dy);

    QAngle ang{};
    ang.pitch = -std::atan2(dz, hyp) * kRadToDeg;
    ang.yaw = std::atan2(dy, dx) * kRadToDeg;
    ang.roll = 0.0f;
    return ang;
}

inline float angular_distance(const QAngle& a, const QAngle& b) {
    const float dp = a.pitch - b.pitch;
    const float dy = normalize_yaw(a.yaw - b.yaw);
    return std::sqrt(dp * dp + dy * dy);
}

using ViewMatrix = std::array<float, 16>;

inline bool ndc_on_screen(float ndc_x, float ndc_y) {
    return std::isfinite(ndc_x) && std::isfinite(ndc_y) && std::fabs(ndc_x) <= 2.5f &&
           std::fabs(ndc_y) <= 2.5f;
}

inline bool world_to_screen_row(const Vec3& world, Vec2& screen, const ViewMatrix& vm, int width,
                                int height) {
    const float w = vm[12] * world.x + vm[13] * world.y + vm[14] * world.z + vm[15];
    if (w < 0.01f || w > 50000.f)
        return false;

    const float x = vm[0] * world.x + vm[1] * world.y + vm[2] * world.z + vm[3];
    const float y = vm[4] * world.x + vm[5] * world.y + vm[6] * world.z + vm[7];
    const float inv_w = 1.0f / w;

    const float ndc_x = x * inv_w;
    const float ndc_y = y * inv_w;
    if (!ndc_on_screen(ndc_x, ndc_y))
        return false;

    screen.x = (width * 0.5f) + (ndc_x * width * 0.5f);
    screen.y = (height * 0.5f) - (ndc_y * height * 0.5f);
    return std::isfinite(screen.x) && std::isfinite(screen.y);
}

inline bool world_to_screen_col(const Vec3& world, Vec2& screen, const ViewMatrix& vm, int width,
                                int height) {
    const float w = vm[3] * world.x + vm[7] * world.y + vm[11] * world.z + vm[15];
    if (w < 0.01f || w > 50000.f)
        return false;

    const float x = vm[0] * world.x + vm[4] * world.y + vm[8] * world.z + vm[12];
    const float y = vm[1] * world.x + vm[5] * world.y + vm[9] * world.z + vm[13];
    const float inv_w = 1.0f / w;

    const float ndc_x = x * inv_w;
    const float ndc_y = y * inv_w;
    if (!ndc_on_screen(ndc_x, ndc_y))
        return false;

    screen.x = (width * 0.5f) + (ndc_x * width * 0.5f);
    screen.y = (height * 0.5f) - (ndc_y * height * 0.5f);
    return std::isfinite(screen.x) && std::isfinite(screen.y);
}

inline bool world_to_screen(const Vec3& world, Vec2& screen, const ViewMatrix& vm, int width,
                            int height) {
    if (world_to_screen_row(world, screen, vm, width, height))
        return true;
    return world_to_screen_col(world, screen, vm, width, height);
}

} // namespace sdk
