#include "config/runtime_config.hpp"

#include <ShlObj.h>

#include <cwchar>
#include <filesystem>

namespace config {

namespace {

constexpr wchar_t kSection[] = L"aim";

float read_float(const wchar_t* ini, const wchar_t* key, float default_value) {
    wchar_t buf[64]{};
    GetPrivateProfileStringW(kSection, key, L"", buf, static_cast<DWORD>(std::size(buf)), ini);
    if (!buf[0])
        return default_value;
    return static_cast<float>(wcstod(buf, nullptr));
}

int read_int(const wchar_t* ini, const wchar_t* key, int default_value) {
    wchar_t buf[64]{};
    GetPrivateProfileStringW(kSection, key, L"", buf, static_cast<DWORD>(std::size(buf)), ini);
    if (!buf[0])
        return default_value;
    return static_cast<int>(wcstol(buf, nullptr, 10));
}

void write_float(const wchar_t* ini, const wchar_t* key, float value) {
    wchar_t buf[64]{};
    swprintf_s(buf, L"%.4f", value);
    WritePrivateProfileStringW(kSection, key, buf, ini);
}

void write_int(const wchar_t* ini, const wchar_t* key, int value) {
    wchar_t buf[64]{};
    swprintf_s(buf, L"%d", value);
    WritePrivateProfileStringW(kSection, key, buf, ini);
}

bool get_file_write_time(const wchar_t* path, FILETIME& out) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &data))
        return false;
    out = data.ftLastWriteTime;
    return true;
}

bool filetime_equal(const FILETIME& a, const FILETIME& b) {
    return a.dwLowDateTime == b.dwLowDateTime && a.dwHighDateTime == b.dwHighDateTime;
}

} // namespace

const wchar_t* runtime_config_path() {
    static std::wstring path;
    if (path.empty()) {
        wchar_t* local_app = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local_app)) &&
            local_app) {
            path = local_app;
            CoTaskMemFree(local_app);
        } else {
            path = L".";
        }
        path += L"\\CS2_ESP\\runtime.ini";
    }
    return path.c_str();
}

void write_default_runtime_config_if_missing() {
    const wchar_t* path = runtime_config_path();
    if (GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES)
        return;

    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);

    aim::SoftAimConfig def{};
    write_float(path, L"fov_degrees", def.fov_degrees);
    write_float(path, L"smooth", def.smooth);
    write_float(path, L"max_step_degrees", def.max_step_degrees);
    write_float(path, L"head_z_offset", def.head_z_offset);
    write_float(path, L"screen_fov_pixels", def.screen_fov_pixels);
    write_float(path, L"target_switch_hysteresis_deg", def.target_switch_hysteresis_deg);
    write_float(path, L"target_lock_break_fov_deg", def.target_lock_break_fov_deg);
    write_float(path, L"spray_smooth", def.spray_smooth);
    write_float(path, L"spray_max_step_degrees", def.spray_max_step_degrees);
    write_float(path, L"spray_chest_drop_z", def.spray_chest_drop_z);
    write_float(path, L"rcs_scale_tap", def.rcs_scale_tap);
    write_float(path, L"rcs_scale_spray", def.rcs_scale_spray);
    write_int(path, L"spray_shots_threshold", def.spray_shots_threshold);
}

bool try_reload_runtime_config(aim::SoftAimConfig& cfg, FILETIME& last_write_time) {
    const wchar_t* path = runtime_config_path();
    FILETIME write_time{};
    if (!get_file_write_time(path, write_time))
        return false;

    if (filetime_equal(write_time, last_write_time))
        return false;

    aim::SoftAimConfig next = cfg;
    next.fov_degrees = read_float(path, L"fov_degrees", next.fov_degrees);
    next.smooth = read_float(path, L"smooth", next.smooth);
    next.max_step_degrees = read_float(path, L"max_step_degrees", next.max_step_degrees);
    next.head_z_offset = read_float(path, L"head_z_offset", next.head_z_offset);
    next.screen_fov_pixels = read_float(path, L"screen_fov_pixels", next.screen_fov_pixels);
    next.target_switch_hysteresis_deg =
        read_float(path, L"target_switch_hysteresis_deg", next.target_switch_hysteresis_deg);
    next.target_lock_break_fov_deg =
        read_float(path, L"target_lock_break_fov_deg", next.target_lock_break_fov_deg);
    next.spray_smooth = read_float(path, L"spray_smooth", next.spray_smooth);
    next.spray_max_step_degrees =
        read_float(path, L"spray_max_step_degrees", next.spray_max_step_degrees);
    next.spray_chest_drop_z = read_float(path, L"spray_chest_drop_z", next.spray_chest_drop_z);
    next.rcs_scale_tap = read_float(path, L"rcs_scale_tap", next.rcs_scale_tap);
    next.rcs_scale_spray = read_float(path, L"rcs_scale_spray", next.rcs_scale_spray);
    next.spray_shots_threshold =
        read_int(path, L"spray_shots_threshold", next.spray_shots_threshold);

    cfg = next;
    last_write_time = write_time;
    return true;
}

} // namespace config
