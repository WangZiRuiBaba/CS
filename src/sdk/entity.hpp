#pragma once

#include "memory/process.hpp"
#include "sdk/math.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sdk {

struct SkeletonLine {
    float x1{};
    float y1{};
    float x2{};
    float y2{};
    float width = 1.0f;
};

struct EspTarget {
    float x{};
    float y{};
    float w{};
    float h{};
    int health{};
    std::string name;
    Vec3 aim_head{};
    Vec3 world_feet{};
    Vec3 world_head{};
    bool has_c4 = false;
    std::uintptr_t pawn_id{}; // stable id for aim target lock
    std::vector<SkeletonLine> skeleton;
    std::optional<Vec2> skeleton_head;
};

// Recompute the screen box from the last world feet/head and a new view matrix.
inline void reproject_box(EspTarget& t, const ViewMatrix& vm, int screen_w, int screen_h) {
    Vec2 screen_feet{};
    Vec2 screen_head{};
    if (!world_to_screen(t.world_feet, screen_feet, vm, screen_w, screen_h) ||
        !world_to_screen(t.world_head, screen_head, vm, screen_w, screen_h)) {
        t.w = 0.f;
        t.h = 0.f;
        return;
    }
    const float height = screen_feet.y - screen_head.y;
    if (height < 2.f) {
        t.w = 0.f;
        t.h = 0.f;
        return;
    }
    const float width = height * 0.48f;
    t.x = screen_head.x - width * 0.5f;
    t.y = screen_head.y;
    t.w = width;
    t.h = height;
}

struct EspDebugStats {
    std::uintptr_t local_pawn{};
    int local_team{};
    int controllers_seen{};
    int pawns_resolved{};
    int enemies_after_team{};
    int drawn{};
};

class EntitySystem {
public:
    explicit EntitySystem(const memory::Process& process);

    [[nodiscard]] std::optional<ViewMatrix> view_matrix() const;
    [[nodiscard]] std::uintptr_t local_pawn() const;
    [[nodiscard]] std::optional<Vec3> local_eye_position() const;
    [[nodiscard]] std::optional<int> local_team() const;
    [[nodiscard]] std::optional<std::uint16_t> active_weapon_def_index() const;

    [[nodiscard]] std::vector<EspTarget> collect_enemies(const ViewMatrix& vm, int screen_w,
                                                         int screen_h, bool include_skeleton,
                                                         EspDebugStats* stats = nullptr) const;

private:
    [[nodiscard]] std::uintptr_t entity_system() const;
    [[nodiscard]] std::uintptr_t entity_from_index(int index) const;
    [[nodiscard]] std::uintptr_t pawn_from_handle(std::uint32_t handle) const;
    [[nodiscard]] std::optional<Vec3> pawn_origin(std::uintptr_t pawn) const;
    [[nodiscard]] std::optional<Vec3> pawn_view_offset(std::uintptr_t pawn) const;
    [[nodiscard]] bool pawn_alive(std::uintptr_t pawn) const;
    [[nodiscard]] bool is_valid_pawn(std::uintptr_t pawn) const;
    [[nodiscard]] std::optional<Vec3> pawn_bone_position(std::uintptr_t pawn, int bone_index,
                                                         const Vec3& origin) const;
    [[nodiscard]] bool try_add_enemy_pawn(std::uintptr_t pawn, std::uintptr_t local, int my_team,
                                          const Vec3& local_origin, const ViewMatrix& vm,
                                          int screen_w, int screen_h, bool include_skeleton,
                                          std::vector<EspTarget>& out,
                                          EspDebugStats* stats) const;

    class EntityListCache;

    const memory::Process& proc_;
};

} // namespace sdk
