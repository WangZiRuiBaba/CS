#pragma once

#ifndef CS2_ESP_ESP_ONLY
#include "aim/soft_aim.hpp"
#endif
#include "memory/process.hpp"

#include <Windows.h>
#include "render/overlay.hpp"
#include "sdk/entity.hpp"
#include "sdk/math.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace esp {

// 烟闪写内存：实现保留在 sdk/entity.cpp，当前关闭快捷键与运行时调用。
inline constexpr bool kUtilitySuppressFeatureEnabled = false;

class EspLoop {
public:
    explicit EspLoop(memory::Process& process);

    void tick(render::Overlay& overlay, bool game_active);

    void toggle();
    [[nodiscard]] bool enabled() const { return enabled_; }

    void toggle_skeleton();
    [[nodiscard]] bool skeleton_enabled() const { return skeleton_enabled_; }

#ifndef CS2_ESP_ESP_ONLY
    void toggle_aim();
    [[nodiscard]] bool aim_toggled() const { return aim_toggled_; }
    [[nodiscard]] int aim_toggle_vk() const { return aim_config_.aim_toggle_vk; }
#endif

    void toggle_utility_suppress();
    [[nodiscard]] bool utility_suppressed() const { return utility_suppressed_; }

    void toggle_color_picker(render::Overlay& overlay);
    void set_color_picker_visible(render::Overlay& overlay, bool visible);
    void toggle_color_picker_tab();
    [[nodiscard]] bool color_picker_visible() const { return color_picker_visible_; }
    [[nodiscard]] bool toast_active() const;

    void show_hint(std::wstring message, std::chrono::seconds duration = std::chrono::seconds(8));

    void poll_runtime_config();

    [[nodiscard]] bool needs_write_access(bool aim_held) const;
    [[nodiscard]] bool needs_write_access_from_input() const;
    [[nodiscard]] bool wants_scheduled_handle_refresh() const;

private:
    void show_mode_toast(std::wstring message);
    void clear_display_cache();
    void poll_color_picker_input(render::Overlay& overlay);
    void sync_custom_from_colors();
    void apply_custom_to_colors();
    [[nodiscard]] render::OverlayHud build_hud() const;

    memory::Process& process_;
    sdk::EntitySystem entities_;
#ifndef CS2_ESP_ESP_ONLY
    aim::SoftAimConfig aim_config_{};
#endif
    bool enabled_{true};
    bool skeleton_enabled_{false};
#ifndef CS2_ESP_ESP_ONLY
    bool aim_toggled_{false};
#endif
    bool utility_suppressed_{false};
    bool color_picker_visible_{false};
    render::CustomColorState custom_color_{};
    bool color_drag_sv_{false};
    bool color_drag_hue_{false};
    COLORREF box_color_{RGB(255, 55, 55)};
    COLORREF skeleton_color_{RGB(0, 210, 255)};
    bool logged_matrix_{false};
    std::wstring toast_message_;
    std::chrono::steady_clock::time_point toast_until_{};

    std::vector<sdk::EspTarget> target_cache_;
    std::chrono::steady_clock::time_point target_cache_time_{};
    std::chrono::steady_clock::time_point last_full_collect_{};
    std::chrono::steady_clock::time_point last_nonempty_targets_{};
    std::optional<sdk::ViewMatrix> last_view_matrix_{};
    std::uintptr_t last_local_pawn_{};
    int empty_collect_streak_{0};

    FILETIME runtime_config_write_time_{};
};

} // namespace esp
