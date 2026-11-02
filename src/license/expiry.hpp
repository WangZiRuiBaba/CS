#pragma once

#include <string>

namespace license {

// First run starts the countdown (retail builds only). Returns false if expired.
bool ensure_license_valid();

[[nodiscard]] std::wstring current_machine_code();

} // namespace license
