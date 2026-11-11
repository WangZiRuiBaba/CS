#include "sdk/entity.hpp"

#include "offsets/offsets.hpp"
#include "sdk/bones.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <unordered_set>
#include <vector>

namespace sdk {

namespace {

constexpr int kMaxPlayerSlots = 64;
constexpr int kFallbackEntityScan = 512;
constexpr int kCasualMaxEnemies = 20;
constexpr float kMaxDistanceSq = 4000.0f * 4000.0f;
constexpr float kMinBoxHeight = 2.0f;
// Reject degenerate boxes: a pawn touching the camera (or a bogus bone read)
// projects to an absurd height and would be drawn as a full-screen line.
constexpr float kMaxBoxHeightFactor = 1.5f;
constexpr float kDefaultHeadOffset = 72.0f;
constexpr std::uint32_t kInvalidHandle = 0xFFFFFFFFu;

bool is_playable_team(int team) { return team == 2 || team == 3; }

} // namespace

class EntitySystem::EntityListCache {
public:
    explicit EntityListCache(const memory::Process& proc, std::uintptr_t client)
        : proc_(proc), client_(client) {}

    std::uintptr_t get(int index) {
        if (index < 0 || index >= 0x8000)
            return 0;

        if (!system_) {
            system_ = proc_.read<std::uintptr_t>(client_ + offsets::dwEntityList).value_or(0);
            if (!system_)
                return 0;
        }

        const int chunk_index = (index & 0x7FFF) >> 9;
        if (chunk_index >= static_cast<int>(chunks_.size()))
            return 0;

        if (!chunks_[chunk_index]) {
            chunks_[chunk_index] =
                proc_
                    .read<std::uintptr_t>(system_ + 0x10 +
                                          static_cast<std::uintptr_t>(chunk_index) * 8)
                    .value_or(0);
        }

        if (!chunks_[chunk_index])
            return 0;

        const int slot = index & 0x1FF;
        if (chunk_index == 0 && slot < kChunk0Slots) {
            if (!chunk0_ready_) {
                chunk0_.resize(static_cast<std::size_t>(kChunk0Slots) * offsets::kEntityStride);
                chunk0_ready_ = proc_.read_raw(chunks_[0], chunk0_.data(), chunk0_.size());
            }
            if (chunk0_ready_) {
                std::uintptr_t value = 0;
                std::memcpy(&value,
                            chunk0_.data() + static_cast<std::size_t>(slot) * offsets::kEntityStride,
                            sizeof(value));
                return value;
            }
        }

        return proc_
            .read<std::uintptr_t>(chunks_[chunk_index] +
                                  static_cast<std::uintptr_t>(slot) * offsets::kEntityStride)
            .value_or(0);
    }

private:
    static constexpr int kChunk0Slots = 512;

    const memory::Process& proc_;
    std::uintptr_t client_{};
    std::uintptr_t system_{};
    std::array<std::uintptr_t, 32> chunks_{};
    std::vector<unsigned char> chunk0_{};
    bool chunk0_ready_{false};
};

EntitySystem::EntitySystem(const memory::Process& process) : proc_(process) {}

std::optional<ViewMatrix> EntitySystem::view_matrix() const {
    const std::uintptr_t client = proc_.client_base();
    if (!client)
        return std::nullopt;

    ViewMatrix vm{};
    if (!proc_.read_raw(client + offsets::dwViewMatrix, vm.data(), sizeof(vm)))
        return std::nullopt;
    // Plain view blocks (wrong dwViewMatrix) have no perspective row — reject them.
    const float perspective = std::fabs(vm[12]) + std::fabs(vm[13]) + std::fabs(vm[14]);
    if (!std::isfinite(perspective) || perspective < 1e-5f)
        return std::nullopt;
    return vm;
}

std::uintptr_t EntitySystem::entity_system() const {
    const std::uintptr_t client = proc_.client_base();
    if (!client)
        return 0;
    return proc_.read<std::uintptr_t>(client + offsets::dwEntityList).value_or(0);
}

std::uintptr_t EntitySystem::entity_from_index(int index) const {
    const std::uintptr_t client = proc_.client_base();
    if (!client)
        return 0;
    EntityListCache cache(proc_, client);
    return cache.get(index);
}

std::uintptr_t EntitySystem::pawn_from_handle(std::uint32_t handle) const {
    if (!handle || handle == kInvalidHandle)
        return 0;
    return entity_from_index(static_cast<int>(handle & 0x7FFF));
}

std::uintptr_t EntitySystem::local_pawn() const {
    const std::uintptr_t client = proc_.client_base();
    if (!client)
        return 0;

    if (const auto direct = proc_.read<std::uintptr_t>(client + offsets::dwLocalPlayerPawn)) {
        if (*direct > 0x10000) {
            if (const auto health = proc_.read<int>(*direct + offsets::schemas::m_iHealth)) {
                if (*health > 0 && *health <= 200)
                    return *direct;
            }
            // Brief health/read glitches should not drop ESP for whole frames.
            if (pawn_origin(*direct))
                return *direct;
        }
    }

    EntityListCache cache(proc_, client);
    if (const auto controller =
            proc_.read<std::uintptr_t>(client + offsets::dwLocalPlayerController)) {
        if (*controller) {
            if (const auto handle =
                    proc_.read<std::uint32_t>(*controller + offsets::schemas::m_hPlayerPawn)) {
                if (*handle && *handle != kInvalidHandle) {
                    if (const auto pawn = cache.get(static_cast<int>(*handle & 0x7FFF)))
                        return pawn;
                }
            }
        }
    }

    return 0;
}

std::optional<Vec3> EntitySystem::local_eye_position() const {
    const auto pawn = local_pawn();
    if (!pawn)
        return std::nullopt;

    const auto feet = pawn_origin(pawn);
    if (!feet)
        return std::nullopt;

    if (const auto view_off = pawn_view_offset(pawn))
        return *feet + *view_off;

    Vec3 eye = *feet;
    eye.z += kDefaultHeadOffset;
    return eye;
}

std::optional<std::uint16_t> EntitySystem::active_weapon_def_index() const {
    const std::uintptr_t client = proc_.client_base();
    const auto pawn = local_pawn();
    if (!client || !pawn)
        return std::nullopt;

    const auto weapon_services =
        proc_.read<std::uintptr_t>(pawn + offsets::schemas::m_pWeaponServices);
    if (!weapon_services)
        return std::nullopt;

    const auto weapon_handle =
        proc_.read<std::uint32_t>(*weapon_services + offsets::schemas::m_hActiveWeapon);
    if (!weapon_handle || *weapon_handle == kInvalidHandle)
        return std::nullopt;

    EntityListCache list(proc_, client);
    const auto weapon = list.get(static_cast<int>(*weapon_handle & 0x7FFF));
    if (!weapon)
        return std::nullopt;

    const auto item_view = weapon + offsets::schemas::m_AttributeManager +
                           offsets::schemas::m_Item;
    return proc_.read<std::uint16_t>(item_view + offsets::schemas::m_iItemDefinitionIndex);
}

std::optional<int> EntitySystem::local_team() const {
    const std::uintptr_t client = proc_.client_base();
    if (!client)
        return std::nullopt;

    if (const auto controller =
            proc_.read<std::uintptr_t>(client + offsets::dwLocalPlayerController)) {
        if (*controller) {
            if (const auto team = proc_.read<int>(*controller + offsets::schemas::m_iTeamNum)) {
                if (is_playable_team(*team))
                    return team;
            }
        }
    }

    const auto pawn = local_pawn();
    if (!pawn)
        return std::nullopt;
    return proc_.read<int>(pawn + offsets::schemas::m_iTeamNum);
}

bool EntitySystem::is_valid_pawn(std::uintptr_t pawn) const {
    if (!pawn)
        return false;
    const auto health = proc_.read<int>(pawn + offsets::schemas::m_iHealth);
    if (!health || *health <= 0 || *health > 200)
        return false;
    return pawn_origin(pawn).has_value();
}

std::optional<Vec3> EntitySystem::pawn_origin(std::uintptr_t pawn) const {
    const auto scene_node =
        proc_.read<std::uintptr_t>(pawn + offsets::schemas::m_pGameSceneNode);
    if (!scene_node)
        return std::nullopt;
    return proc_.read<Vec3>(*scene_node + offsets::schemas::m_vecAbsOrigin);
}

std::optional<Vec3> EntitySystem::pawn_view_offset(std::uintptr_t pawn) const {
    return proc_.read<Vec3>(pawn + offsets::schemas::m_vecViewOffset);
}

namespace {

bool bone_near_origin(const Vec3& pos, const Vec3& origin, float radius) {
    const float dx = pos.x - origin.x;
    const float dy = pos.y - origin.y;
    const float dz = pos.z - origin.z;
    return dx * dx + dy * dy + dz * dz <= radius * radius;
}

bool valid_bone_vec(const Vec3& pos, const Vec3& origin) {
    if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z))
        return false;
    if (pos.x == 0.0f && pos.y == 0.0f && pos.z == 0.0f)
        return false;
    return bone_near_origin(pos, origin, bones::kMaxBoneRadiusFromOrigin);
}

std::optional<Vec3> decode_bone_position(const memory::Process& proc, std::uintptr_t addr,
                                         const Vec3& origin) {
    alignas(4) float raw[8]{};
    if (!proc.read_raw(addr, raw, sizeof(raw)))
        return std::nullopt;

    const Vec3 at_start{raw[0], raw[1], raw[2]};
    const Vec3 after_quat{raw[4], raw[5], raw[6]};
    const bool start_ok = valid_bone_vec(at_start, origin);
    const bool quat_ok = valid_bone_vec(after_quat, origin);
    if (start_ok && quat_ok) {
        const auto dist_sq = [](const Vec3& p, const Vec3& o) {
            const float dx = p.x - o.x;
            const float dy = p.y - o.y;
            const float dz = p.z - o.z;
            return dx * dx + dy * dy + dz * dz;
        };
        return dist_sq(at_start, origin) <= dist_sq(after_quat, origin) ? at_start : after_quat;
    }
    if (start_ok)
        return at_start;
    if (quat_ok)
        return after_quat;
    return std::nullopt;
}

} // namespace

std::optional<Vec3> EntitySystem::pawn_bone_position(std::uintptr_t pawn, int bone_index,
                                                     const Vec3& origin) const {
    if (bone_index < 0 || bone_index > bones::kMaxBoneIndex)
        return std::nullopt;

    const auto scene =
        proc_.read<std::uintptr_t>(pawn + offsets::schemas::m_pGameSceneNode);
    if (!scene || *scene == 0)
        return std::nullopt;

    const auto bone_array = proc_.read<std::uintptr_t>(
        *scene + offsets::schemas::m_modelState + bones::kBoneArrayInModelState);
    if (!bone_array || *bone_array == 0)
        return std::nullopt;

    const auto addr =
        *bone_array + static_cast<std::uintptr_t>(bone_index) * bones::kStride;
    return decode_bone_position(proc_, addr, origin);
}

bool EntitySystem::pawn_alive(std::uintptr_t pawn) const {
    const auto health = proc_.read<int>(pawn + offsets::schemas::m_iHealth);
    if (!health || *health <= 0)
        return false;
    if (const auto life = proc_.read<std::uint8_t>(pawn + offsets::schemas::m_lifeState))
        return *life == 0;
    return true;
}

bool EntitySystem::try_add_enemy_pawn(std::uintptr_t pawn, std::uintptr_t local, int my_team,
                                      const Vec3& local_origin, const ViewMatrix& vm, int screen_w,
                                      int screen_h, bool include_skeleton,
                                      std::vector<EspTarget>& out, EspDebugStats* stats) const {
    if (!pawn || pawn == local)
        return false;

    const auto health = proc_.read<int>(pawn + offsets::schemas::m_iHealth);
    if (!health || *health <= 0 || *health > 200)
        return false;

    if (const auto life = proc_.read<std::uint8_t>(pawn + offsets::schemas::m_lifeState)) {
        if (*life != 0)
            return false;
    }

    const auto team = proc_.read<int>(pawn + offsets::schemas::m_iTeamNum);
    if (!team || !is_playable_team(*team) || *team == my_team)
        return false;

    if (stats)
        ++stats->enemies_after_team;

    const auto feet = pawn_origin(pawn);
    if (!feet)
        return false;

    const float dx = feet->x - local_origin.x;
    const float dy = feet->y - local_origin.y;
    const float dz = feet->z - local_origin.z;
    if (dx * dx + dy * dy + dz * dz > kMaxDistanceSq)
        return false;

    Vec3 head = *feet;
    if (const auto head_bone = pawn_bone_position(pawn, bones::Head, *feet))
        head = *head_bone;
    else if (const auto view_off = pawn_view_offset(pawn))
        head = *feet + *view_off;
    else
        head.z += kDefaultHeadOffset;

    Vec2 screen_feet{};
    Vec2 screen_head{};
    if (!world_to_screen(*feet, screen_feet, vm, screen_w, screen_h))
        return false;
    if (!world_to_screen(head, screen_head, vm, screen_w, screen_h))
        return false;

    const float height = std::fabs(screen_feet.y - screen_head.y);
    if (height < kMinBoxHeight || height > static_cast<float>(screen_h) * kMaxBoxHeightFactor)
        return false;

    const float width = height * 0.48f;
    const float top = (std::min)(screen_head.y, screen_feet.y);
    const float center_x = (screen_head.x + screen_feet.x) * 0.5f;

    EspTarget t{};
    t.x = center_x - width * 0.5f;
    t.y = top;
    t.w = width;
    t.h = height;
    t.health = *health;
    t.aim_head = head;
    t.world_feet = *feet;
    t.world_head = head;
    t.pawn_id = pawn;

    constexpr std::uint16_t kC4Def = 49;
    if (const auto services = proc_.read<std::uintptr_t>(pawn + offsets::schemas::m_pWeaponServices)) {
        if (*services && proc_.client_base()) {
            EntityListCache weapons(proc_, proc_.client_base());
            auto is_c4 = [&](std::uint32_t handle) {
                if (!handle || handle == kInvalidHandle)
                    return false;
                const auto weapon = weapons.get(static_cast<int>(handle & 0x7FFF));
                if (!weapon)
                    return false;
                const auto item = weapon + offsets::schemas::m_AttributeManager + offsets::schemas::m_Item;
                const auto def =
                    proc_.read<std::uint16_t>(item + offsets::schemas::m_iItemDefinitionIndex);
                return def && *def == kC4Def;
            };
            const auto count = proc_.read<int>(*services + offsets::schemas::m_hMyWeapons).value_or(0);
            const auto data =
                proc_.read<std::uintptr_t>(*services + offsets::schemas::m_hMyWeapons + 8).value_or(0);
            if (count > 0 && count <= 16 && data) {
                for (int i = 0; i < count; ++i) {
                    const auto handle =
                        proc_.read<std::uint32_t>(data + static_cast<std::uintptr_t>(i) * sizeof(std::uint32_t))
                            .value_or(0);
                    if (is_c4(handle)) {
                        t.has_c4 = true;
                        break;
                    }
                }
            }
            if (!t.has_c4) {
                const auto active =
                    proc_.read<std::uint32_t>(*services + offsets::schemas::m_hActiveWeapon).value_or(0);
                t.has_c4 = is_c4(active);
            }
        }
    }

    if (include_skeleton) {
        std::array<std::optional<Vec3>, bones::kMaxBoneIndex + 1> bone_cache{};
        std::array<bool, bones::kMaxBoneIndex + 1> bone_fetched{};
        const auto bone_at = [&](int index) -> std::optional<Vec3> {
            if (index < 0 || index > bones::kMaxBoneIndex)
                return std::nullopt;
            const auto slot = static_cast<std::size_t>(index);
            if (!bone_fetched[slot]) {
                bone_cache[slot] = pawn_bone_position(pawn, index, *feet);
                bone_fetched[slot] = true;
            }
            return bone_cache[slot];
        };

        t.skeleton.reserve(bones::kLinks.size());
        for (const auto& link : bones::kLinks) {
            const auto a = bone_at(link.from);
            const auto b = bone_at(link.to);
            if (!a || !b)
                continue;

            const float lx = a->x - b->x;
            const float ly = a->y - b->y;
            const float lz = a->z - b->z;
            const float max_len = bones::kMaxLinkLength;
            if (lx * lx + ly * ly + lz * lz > max_len * max_len)
                continue;

            // Both ends at foot height and apart: that is a line tying the feet together.
            const float horiz = std::sqrt(lx * lx + ly * ly);
            if (std::fabs(a->z - feet->z) < 14.f && std::fabs(b->z - feet->z) < 14.f && horiz > 8.f)
                continue;

            Vec2 screen_a{};
            Vec2 screen_b{};
            if (!world_to_screen(*a, screen_a, vm, screen_w, screen_h))
                continue;
            if (!world_to_screen(*b, screen_b, vm, screen_w, screen_h))
                continue;

            t.skeleton.push_back(SkeletonLine{screen_a.x, screen_a.y, screen_b.x, screen_b.y,
                                              link.width});
        }

        if (const auto head_bone = bone_at(bones::Head)) {
            Vec2 head_screen{};
            if (world_to_screen(*head_bone, head_screen, vm, screen_w, screen_h))
                t.skeleton_head = head_screen;
        }
    }

    out.push_back(std::move(t));
    if (stats)
        ++stats->drawn;
    return true;
}

void EntitySystem::suppress_flash_and_smoke() const {
    if (const auto pawn = local_pawn()) {
        const auto alpha = proc_.read<float>(pawn + offsets::schemas::m_flFlashOverlayAlpha).value_or(0.f);
        const auto duration = proc_.read<float>(pawn + offsets::schemas::m_flFlashDuration).value_or(0.f);
        if (alpha > 0.01f || duration > 0.01f) {
            const float zero = 0.f;
            proc_.write(pawn + offsets::schemas::m_flFlashOverlayAlpha, zero);
            proc_.write(pawn + offsets::schemas::m_flFlashMaxAlpha, zero);
            proc_.write(pawn + offsets::schemas::m_flFlashDuration, zero);
            proc_.write(pawn + offsets::schemas::m_flFlashBangTime, zero);
            proc_.write(pawn + offsets::schemas::m_flFlashScreenshotAlpha, zero);
        }
    }

    static ULONGLONG last_scan = 0;
    static std::vector<std::uintptr_t> smokes;
    const ULONGLONG now = GetTickCount64();
    const auto client = proc_.client_base();
    if (!client)
        return;

    // Projectiles live above the player slots. One pass a second, only chunk 0,
    // so this does not hitch the frame the way a 1024-entity scan did.
    if (now - last_scan >= 1000) {
        last_scan = now;
        smokes.erase(std::remove_if(smokes.begin(), smokes.end(),
                                    [&](std::uintptr_t ent) { return !pawn_origin(ent); }),
                     smokes.end());

        EntityListCache list(proc_, client);
        for (int i = 64; i < 512; ++i) {
            const auto ent = list.get(i);
            if (!ent)
                continue;
            if (std::find(smokes.begin(), smokes.end(), ent) != smokes.end())
                continue;
            const auto did = proc_.read<std::uint8_t>(ent + offsets::schemas::m_bDidSmokeEffect);
            if (!did || *did == 0)
                continue;
            const auto tick = proc_.read<int>(ent + offsets::schemas::m_nSmokeEffectTickBegin);
            if (!tick || *tick <= 0)
                continue;
            const auto det = proc_.read<Vec3>(ent + offsets::schemas::m_vSmokeDetonationPos);
            const auto origin = pawn_origin(ent);
            if (!det || !origin || !std::isfinite(det->x) || std::fabs(det->x) > 20000.f)
                continue;
            const float dx = det->x - origin->x;
            const float dy = det->y - origin->y;
            const float dz = det->z - origin->z;
            if (dx * dx + dy * dy + dz * dz > 400.f * 400.f)
                continue;
            smokes.push_back(ent);
        }
    }

    const std::uint8_t zero_b = 0;
    const int zero_i = 0;
    for (const std::uintptr_t ent : smokes) {
        proc_.write(ent + offsets::schemas::m_bDidSmokeEffect, zero_b);
        proc_.write(ent + offsets::schemas::m_bSmokeVolumeDataReceived, zero_b);
        proc_.write(ent + offsets::schemas::m_nVoxelFrameDataSize, zero_i);
        proc_.write(ent + offsets::schemas::m_nSmokeEffectTickBegin, zero_i);
    }
}

std::vector<EspTarget> EntitySystem::collect_enemies(const ViewMatrix& vm, int screen_w,
                                                     int screen_h, bool include_skeleton,
                                                     EspDebugStats* stats) const {
    std::vector<EspTarget> out;
    if (screen_w <= 0 || screen_h <= 0)
        return out;

    const std::uintptr_t client = proc_.client_base();
    if (!client)
        return out;

    const auto local = local_pawn();
    const auto my_team = local_team();
    if (stats) {
        stats->local_pawn = local;
        stats->local_team = my_team.value_or(0);
        stats->controllers_seen = 0;
        stats->pawns_resolved = 0;
        stats->enemies_after_team = 0;
        stats->drawn = 0;
    }

    if (!my_team || !local)
        return out;

    const auto local_origin = pawn_origin(local);
    if (!local_origin)
        return out;

    out.reserve(kCasualMaxEnemies);
    EntityListCache list(proc_, client);
    std::unordered_set<std::uintptr_t> seen_pawns;
    seen_pawns.reserve(kCasualMaxEnemies);

    auto consider_pawn = [&](std::uintptr_t pawn) {
        if (!pawn || pawn == local || seen_pawns.count(pawn))
            return;
        if (try_add_enemy_pawn(pawn, local, *my_team, *local_origin, vm, screen_w, screen_h,
                               include_skeleton, out, stats))
            seen_pawns.insert(pawn);
    };

    // Primary path (original): player controller slots 1..64 -> m_hPlayerPawn.
    int pawns_from_controllers = 0;
    for (int i = 1; i <= kMaxPlayerSlots; ++i) {
        const auto controller = list.get(i);
        if (!controller)
            continue;

        if (stats)
            ++stats->controllers_seen;

        const auto pawn_handle =
            proc_.read<std::uint32_t>(controller + offsets::schemas::m_hPlayerPawn);
        if (!pawn_handle || *pawn_handle == kInvalidHandle)
            continue;

        const auto pawn = list.get(static_cast<int>(*pawn_handle & 0x7FFF));
        if (!pawn)
            continue;

        ++pawns_from_controllers;
        if (stats)
            ++stats->pawns_resolved;

        consider_pawn(pawn);
    }

    // Fallback only when controllers did not resolve anyone. Same list, extra reads.
    if (pawns_from_controllers == 0) {
        for (int i = 1; i <= kFallbackEntityScan; ++i) {
            const auto ent = list.get(i);
            if (!ent || ent == local)
                continue;
            consider_pawn(ent);
        }
    }

    return out;
}

} // namespace sdk
