#pragma once

#include <array>
#include <cstddef>

namespace sdk::bones {

inline constexpr std::ptrdiff_t kBoneArrayInModelState = 0x80;
inline constexpr std::size_t kStride = 32;
inline constexpr int kMaxBoneIndex = 27;

inline constexpr float kMaxBoneRadiusFromOrigin = 150.0f;
// Adjacent bones only. A line long enough to reach the gun or the opposite foot is dropped.
inline constexpr float kMaxLinkLength = 52.0f;

enum Id : int {
    Pelvis = 0,
    Spine0 = 1,
    Spine1 = 2,
    Spine2 = 3,
    Spine3 = 4,
    Neck = 5,
    Head = 6,
    LeftClavicle = 7,
    LeftUpperArm = 8,
    LeftForearm = 9,
    LeftHand = 10,
    RightClavicle = 12,
    RightUpperArm = 13,
    RightForearm = 14,
    RightHand = 15,
    LeftHip = 22,
    LeftKnee = 23,
    LeftAnkle = 24,
    RightHip = 25,
    RightKnee = 26,
    RightAnkle = 27,
};

struct Link {
    int from;
    int to;
    float width;
};

// Parent-child bones so each segment lies on the mesh. Head is a circle.
// Right hand is included only when it stays next to the forearm; a gun bone is longer and skipped.
inline constexpr std::array<Link, 20> kLinks = {{
    {Pelvis, Spine0, 1.0f},
    {Spine0, Spine1, 1.0f},
    {Spine1, Spine2, 1.0f},
    {Spine2, Spine3, 1.0f},
    {Spine3, Neck, 1.0f},
    {Neck, Head, 1.0f},
    {Neck, LeftClavicle, 1.0f},
    {LeftClavicle, LeftUpperArm, 1.0f},
    {LeftUpperArm, LeftForearm, 1.0f},
    {LeftForearm, LeftHand, 1.0f},
    {Neck, RightClavicle, 1.0f},
    {RightClavicle, RightUpperArm, 1.0f},
    {RightUpperArm, RightForearm, 1.0f},
    {RightForearm, RightHand, 1.0f},
    {Pelvis, LeftHip, 1.0f},
    {LeftHip, LeftKnee, 1.0f},
    {LeftKnee, LeftAnkle, 1.0f},
    {Pelvis, RightHip, 1.0f},
    {RightHip, RightKnee, 1.0f},
    {RightKnee, RightAnkle, 1.0f},
}};

static_assert(kLinks.size() == 20);

} // namespace sdk::bones
