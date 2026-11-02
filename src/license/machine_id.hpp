#pragma once

#include <cstdint>
#include <string>

namespace license {

[[nodiscard]] std::uint64_t machine_fingerprint();

/** 大写 16 位十六进制，便于用户复制机器码 */
[[nodiscard]] std::wstring machine_code_display();

} // namespace license
