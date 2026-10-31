#pragma once
#include <windows.h>

class GdiOverlay;

namespace screen_label {

bool sync_overlay_to_cs2(GdiOverlay& overlay, DWORD game_process_id);

} // namespace screen_label
