#pragma once

#include "aim/soft_aim.hpp"

#include <Windows.h>

namespace config {

[[nodiscard]] const wchar_t* runtime_config_path();

// Merges runtime.ini into cfg when the file changed. Returns true if cfg was updated.
bool try_reload_runtime_config(aim::SoftAimConfig& cfg, FILETIME& last_write_time);

void write_default_runtime_config_if_missing();

} // namespace config
