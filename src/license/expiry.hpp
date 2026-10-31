#pragma once

namespace license {

// First run starts the countdown (retail builds only). Returns false if expired.
bool ensure_license_valid();

} // namespace license
