#include "esp/esp.hpp"

#ifndef CS2_ESP_ESP_ONLY
#include "aim/soft_aim.hpp"
#endif
#include "config/runtime_config.hpp"
#include "render/color_palette.hpp"
#include "render/color_picker_panel.hpp"
#include "stealth/stealth.hpp"

#ifdef CS2_ESP_CONSOLE
#include <iostream>
#endif

namespace esp {

namespace {

constexpr auto kToastDuration = std::chrono::seconds(3);
constexpr auto kEmptyTargetGrace = std::chrono::milliseconds(900);
constexpr int kEmptyCollectStreakToClear = 8;
constexpr int kCollectRetryCount = 1;

bool left_click_edge() {
    static bool prev = false;
    const bool down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    const bool edge = down && !prev;
    prev = down;
    return edge;
}

} // namespace

EspLoop::EspLoop(memory::Process& process) : process_(process), entities_(process) {
    config::write_default_runtime_config_if_missing();
#ifndef CS2_ESP_ESP_ONLY
    config::try_reload_runtime_config(aim_config_, runtime_config_write_time_);
#endif
}

void EspLoop::poll_runtime_config() {
#ifndef CS2_ESP_ESP_ONLY
    config::try_reload_runtime_config(aim_config_, runtime_config_write_time_);
#endif
}

void EspLoop::clear_display_cache() {
    target_cache_.clear();
    target_cache_time_ = {};
    last_full_collect_ = {};
    last_nonempty_targets_ = {};
    empty_collect_streak_ = 0;
}

void EspLoop::show_mode_toast(std::wstring message) {
    toast_message_ = std::move(message);
    toast_until_ = std::chrono::steady_clock::now() + kToastDuration;
}

void EspLoop::show_hint(std::wstring message, const std::chrono::seconds duration) {
    toast_message_ = std::move(message);
    toast_until_ = std::chrono::steady_clock::now() + duration;
}

void EspLoop::toggle() {
    enabled_ = !enabled_;
    clear_display_cache();
    show_mode_toast(enabled_ ? L"\u65b9\u6846: \u5f00" : L"\u65b9\u6846: \u5173");
#ifdef CS2_ESP_CONSOLE
    std::cout << "[ESP] " << (enabled_ ? "ON" : "OFF") << '\n';
#endif
}

void EspLoop::toggle_skeleton() {
    skeleton_enabled_ = !skeleton_enabled_;
    clear_display_cache();
    show_mode_toast(skeleton_enabled_ ? L"\u9aa8\u9abc: \u5f00" : L"\u9aa8\u9abc: \u5173");
#ifdef CS2_ESP_CONSOLE
    std::cout << "[Skeleton] " << (skeleton_enabled_ ? "ON" : "OFF") << '\n';
#endif
}

void EspLoop::toggle_utility_suppress() {
    if constexpr (!kUtilitySuppressFeatureEnabled)
        return;
    utility_suppressed_ = !utility_suppressed_;
    show_mode_toast(utility_suppressed_ ? L"\u70df\u96fe/\u95ea\u5149: \u5ffd\u89c6"
                                        : L"\u70df\u96fe/\u95ea\u5149: \u6b63\u5e38");
}

bool EspLoop::needs_write_access(const bool aim_held) const {
    if constexpr (kUtilitySuppressFeatureEnabled) {
        if (utility_suppressed_)
            return true;
    }
#ifndef CS2_ESP_ESP_ONLY
    if (aim_toggled_ || aim_held)
        return true;
#else
    (void)aim_held;
#endif
    return false;
}

bool EspLoop::needs_write_access_from_input() const {
#ifndef CS2_ESP_ESP_ONLY
    const bool aim_held = (GetAsyncKeyState(aim_config_.aim_key_vk) & 0x8000) != 0;
    return needs_write_access(aim_held);
#else
    return needs_write_access(false);
#endif
}

bool EspLoop::wants_scheduled_handle_refresh() const {
    if (enabled_ || skeleton_enabled_ || color_picker_visible_ || toast_active())
        return true;
    if constexpr (kUtilitySuppressFeatureEnabled) {
        if (utility_suppressed_)
            return true;
    }
#ifndef CS2_ESP_ESP_ONLY
    if (aim_toggled_)
        return true;
#endif
    return false;
}

#ifndef CS2_ESP_ESP_ONLY
void EspLoop::toggle_aim() {
    aim_toggled_ = !aim_toggled_;
    show_mode_toast(aim_toggled_ ? L"\u8f85\u52a9\u7784\u51c6: \u5f00" : L"\u8f85\u52a9\u7784\u51c6: \u5173");
#ifdef CS2_ESP_CONSOLE
    std::cout << "[Aim] Memory lock " << (aim_toggled_ ? "ON" : "OFF") << '\n';
#endif
}
#endif

render::OverlayHud EspLoop::build_hud() const {
    render::OverlayHud hud{};
    hud.esp_enabled = enabled_;
    hud.skeleton_enabled = skeleton_enabled_;
#ifndef CS2_ESP_ESP_ONLY
    hud.aim_toggled = aim_toggled_;
#endif
    hud.color_picker_visible = color_picker_visible_;
    hud.color_picker_tab = custom_color_.tab;
    hud.color_custom = custom_color_;
    hud.box_color = box_color_;
    hud.skeleton_color = skeleton_color_;
    hud.toast = toast_message_;
    hud.toast_until = toast_until_;
#ifdef CS2_ESP_ESP_ONLY
    hud.help_caption =
        L"Insert \u65b9\u6846 \u00b7 PageUp \u9aa8\u9abc \u00b7 F7 \u989c\u8272 \u00b7 End \u9000\u51fa";
#else
    hud.help_caption =
        L"Insert \u65b9\u6846 \u00b7 PageUp \u9aa8\u9abc \u00b7 F7 \u989c\u8272 \u00b7 Home \u7784\u51c6 \u00b7 End \u9000\u51fa";
#endif
    if constexpr (kUtilitySuppressFeatureEnabled) {
        hud.help_caption += L" \u00b7 PageDown \u70df\u95ea";
    }
    return hud;
}

void EspLoop::toggle_color_picker(render::Overlay& overlay) {
    set_color_picker_visible(overlay, !color_picker_visible_);
}

void EspLoop::toggle_color_picker_tab() {
    custom_color_.tab = custom_color_.tab == render::ColorPickerTab::Presets
                            ? render::ColorPickerTab::Custom
                            : render::ColorPickerTab::Presets;
    if (custom_color_.tab == render::ColorPickerTab::Custom)
        sync_custom_from_colors();
}

void EspLoop::set_color_picker_visible(render::Overlay& overlay, const bool visible) {
    color_picker_visible_ = visible;
    if (color_picker_visible_)
        sync_custom_from_colors();
    else {
        color_drag_sv_ = false;
        color_drag_hue_ = false;
    }
    overlay.set_click_through(!color_picker_visible_);
    show_mode_toast(color_picker_visible_ ? L"\u989c\u8272\u9762\u677f: \u5f00" : L"\u989c\u8272\u9762\u677f: \u5173");
}

void EspLoop::sync_custom_from_colors() {
    custom_color_.sync_hsv_from_color(render::ColorPickRow::Box, box_color_);
    custom_color_.sync_hsv_from_color(render::ColorPickRow::Skeleton, skeleton_color_);
}

void EspLoop::apply_custom_to_colors() {
    box_color_ = custom_color_.color_for(render::ColorPickRow::Box);
    skeleton_color_ = custom_color_.color_for(render::ColorPickRow::Skeleton);
}

void EspLoop::poll_color_picker_input(render::Overlay& overlay) {
    if (!color_picker_visible_ || !overlay.is_valid())
        return;

    const render::OverlayHud hud = build_hud();
    const render::ColorPickerPanelLayout layout =
        render::ColorPickerPanel::layout(overlay.width(), overlay.height(), custom_color_.tab);

    const bool lbtn = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

    POINT pt{};
    GetCursorPos(&pt);
    ScreenToClient(overlay.hwnd(), &pt);

    if (lbtn) {
        if (color_drag_sv_) {
            render::ColorPickerPanel::update_from_sv(layout, pt.x, pt.y, custom_color_);
            apply_custom_to_colors();
            return;
        }
        if (color_drag_hue_) {
            render::ColorPickerPanel::update_from_hue(layout, pt.x, custom_color_);
            apply_custom_to_colors();
            return;
        }
    } else {
        color_drag_sv_ = false;
        color_drag_hue_ = false;
    }

    if (!left_click_edge())
        return;

    const render::ColorPickerHitResult hit = overlay.hit_color_picker(hud, pt.x, pt.y);
    switch (hit.hit) {
    case render::ColorPickerHit::TabPresets:
        custom_color_.tab = render::ColorPickerTab::Presets;
        break;
    case render::ColorPickerHit::TabCustom:
        custom_color_.tab = render::ColorPickerTab::Custom;
        sync_custom_from_colors();
        break;
    case render::ColorPickerHit::Swatch:
        if (hit.swatch_row == render::ColorPickRow::Box)
            box_color_ = hit.swatch_color;
        else
            skeleton_color_ = hit.swatch_color;
        custom_color_.apply_color(hit.swatch_row, hit.swatch_color);
        break;
    case render::ColorPickerHit::TargetBox:
        custom_color_.active_target = render::ColorPickRow::Box;
        break;
    case render::ColorPickerHit::TargetSkeleton:
        custom_color_.active_target = render::ColorPickRow::Skeleton;
        break;
    case render::ColorPickerHit::SvPlane:
        color_drag_sv_ = true;
        render::ColorPickerPanel::update_from_sv(layout, pt.x, pt.y, custom_color_);
        apply_custom_to_colors();
        break;
    case render::ColorPickerHit::HueBar:
        color_drag_hue_ = true;
        render::ColorPickerPanel::update_from_hue(layout, pt.x, custom_color_);
        apply_custom_to_colors();
        break;
    default:
        break;
    }
}

bool EspLoop::toast_active() const {
    if (toast_message_.empty())
        return false;
    return std::chrono::steady_clock::now() < toast_until_;
}

void EspLoop::tick(render::Overlay& overlay, bool game_active) {
    if (!process_.is_attached())
        return;

    if (!toast_message_.empty() && std::chrono::steady_clock::now() >= toast_until_)
        toast_message_.clear();

    const auto hud = build_hud();

    overlay.set_visible(true);

    if (!game_active) {
        overlay.set_visible(false);
        if (overlay.sync_to_game_window(process_.process_id()))
            overlay.render({}, hud);
        return;
    }

    if constexpr (kUtilitySuppressFeatureEnabled) {
        if (utility_suppressed_) {
            process_.ensure_write_access();
            entities_.suppress_flash_and_smoke();
        }
    }
    /*
     * 烟闪屏蔽（Page Down）已关闭：实现见 EntitySystem::suppress_flash_and_smoke()。
     * 将 esp.hpp 中 kUtilitySuppressFeatureEnabled 改为 true 并恢复 main.cpp 快捷键即可启用。
     */

    const std::uintptr_t local_pawn = entities_.local_pawn();
    if (local_pawn)
        last_local_pawn_ = local_pawn;

    if (!overlay.sync_to_game_window(process_.process_id())) {
        if (toast_active())
            overlay.render({}, hud);
        return;
    }

#ifdef CS2_ESP_ESP_ONLY
    const bool aim_held = false;
    const bool aim_active = false;
#else
    const bool aim_held = (GetAsyncKeyState(aim_config_.aim_key_vk) & 0x8000) != 0;
    const bool aim_active = aim_toggled_ || aim_held;
#endif

    poll_color_picker_input(overlay);

    if (!enabled_ && !skeleton_enabled_ && !aim_active && !color_picker_visible_) {
        if (toast_active())
            overlay.render({}, hud);
        return;
    }

    auto vm = entities_.view_matrix();
    if (vm)
        last_view_matrix_ = *vm;
    else if (last_view_matrix_)
        vm = last_view_matrix_;
    else {
        logged_matrix_ = false;
        if (enabled_ || skeleton_enabled_ || aim_active || color_picker_visible_ || toast_active())
            overlay.render({}, hud);
        return;
    }

#ifdef CS2_ESP_CONSOLE
    if (!logged_matrix_) {
        const bool non_zero = vm->at(0) != 0.0f || vm->at(5) != 0.0f;
        std::cout << "[ESP] ViewMatrix " << (non_zero ? "OK" : "zero - check offsets") << '\n';
        logged_matrix_ = true;
    }
#endif

    const auto now = std::chrono::steady_clock::now();
    const bool esp_on = enabled_;
    const bool skeleton_on = skeleton_enabled_;
    // Camera moves every frame; pawn positions do not. Reproject cached feet/head
    // each tick, and re-read entities about 30 times a second. Aim still reads every tick.
    const std::chrono::milliseconds collect_interval =
        target_cache_.empty() ? std::chrono::milliseconds(100) : std::chrono::milliseconds(32);
    const bool entity_refresh_due =
        last_full_collect_.time_since_epoch().count() == 0 ||
        now - last_full_collect_ >= collect_interval;
    if (esp_on || skeleton_on || aim_active) {
        if (aim_active || entity_refresh_due || target_cache_.empty()) {
            std::vector<sdk::EspTarget> fresh;
            for (int attempt = 0; attempt < kCollectRetryCount; ++attempt) {
                fresh = entities_.collect_enemies(*vm, overlay.width(), overlay.height(),
                                                  skeleton_on, nullptr);
                if (!fresh.empty())
                    break;
            }
            last_full_collect_ = now;
            target_cache_time_ = now;
            if (!fresh.empty()) {
                target_cache_ = std::move(fresh);
                last_nonempty_targets_ = now;
                empty_collect_streak_ = 0;
            } else if (!target_cache_.empty()) {
                ++empty_collect_streak_;
                const bool grace_expired =
                    last_nonempty_targets_.time_since_epoch().count() != 0 &&
                    now - last_nonempty_targets_ > kEmptyTargetGrace;
                if (empty_collect_streak_ >= kEmptyCollectStreakToClear && grace_expired)
                    clear_display_cache();
            }
        } else {
            for (sdk::EspTarget& target : target_cache_)
                sdk::reproject_box(target, *vm, overlay.width(), overlay.height());
        }
    }

    if (enabled_ || skeleton_on || color_picker_visible_ || toast_active())
        overlay.render(target_cache_, hud);

#ifndef CS2_ESP_ESP_ONLY
    if (aim_active) {
        process_.ensure_write_access();
        aim::tick(process_, entities_, target_cache_, aim_active, overlay.width(),
                  overlay.height(), aim_config_);
    }
#endif
}

} // namespace esp
