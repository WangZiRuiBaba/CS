#include "license/expiry.hpp"

#ifndef CS2_ESP_RETAIL

namespace license {

bool ensure_license_valid() { return true; }

} // namespace license

#else

#include <ShlObj.h>

#include <cstdint>
#include <filesystem>
#include <string>

namespace license {

namespace {

#ifdef CS2_ESP_LICENSE_VARIANT_TEST
constexpr std::uint32_t kMagic = 0x43533254u; // CS2T
#else
constexpr std::uint32_t kMagic = 0x43533245u; // CS2E
#endif
constexpr std::uint64_t kMix = 0x9E3779B97F4A7C15ull;
#if defined(CS2_ESP_EXPIRY_SECONDS) && CS2_ESP_EXPIRY_SECONDS > 0
constexpr std::uint64_t kExpirySeconds = static_cast<std::uint64_t>(CS2_ESP_EXPIRY_SECONDS);
#else
#ifndef CS2_ESP_EXPIRY_DAYS
#define CS2_ESP_EXPIRY_DAYS 3
#endif
constexpr std::uint64_t kExpirySeconds =
    static_cast<std::uint64_t>(CS2_ESP_EXPIRY_DAYS) * 86400ull;
#endif

struct StateBlob {
    std::uint32_t magic{};
    std::uint32_t reserved{};
    std::uint64_t first_run_utc{};
    std::uint64_t tag{};
};

std::wstring state_file_path() {
    wchar_t* local_app = nullptr;
    std::wstring base;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local_app)) &&
        local_app) {
        base = local_app;
        CoTaskMemFree(local_app);
    } else {
        base = L".";
    }
    base += L"\\CS2_ESP\\license.bin";
    return base;
}

std::uint64_t tag_for(std::uint64_t first_run_utc) {
    return (first_run_utc ^ kMix) + static_cast<std::uint64_t>(kMagic);
}

std::uint64_t utc_now_seconds() {
    FILETIME ft{};
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli{};
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return uli.QuadPart / 10'000'000ull - 11'644'473'600ull;
}

bool read_state(StateBlob& out) {
    const auto path = state_file_path();
    HANDLE file =
        CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                    FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    DWORD read = 0;
    const BOOL ok =
        ReadFile(file, &out, static_cast<DWORD>(sizeof(out)), &read, nullptr) != 0;
    CloseHandle(file);
    return ok && read == sizeof(out);
}

bool write_state(const StateBlob& blob) {
    const auto path = state_file_path();
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);

    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    DWORD written = 0;
    const BOOL ok =
        WriteFile(file, &blob, static_cast<DWORD>(sizeof(blob)), &written, nullptr) != 0;
    CloseHandle(file);
    return ok && written == sizeof(blob);
}

bool load_or_create_first_run(std::uint64_t& first_run_utc) {
    StateBlob blob{};
    if (read_state(blob) && blob.magic == kMagic && blob.tag == tag_for(blob.first_run_utc)) {
        first_run_utc = blob.first_run_utc;
        return true;
    }

    first_run_utc = utc_now_seconds();
    blob.magic = kMagic;
    blob.reserved = 0;
    blob.first_run_utc = first_run_utc;
    blob.tag = tag_for(first_run_utc);
    if (!write_state(blob))
        return false;
    return true;
}

void show_expired_message() {
    MessageBoxW(nullptr,
                L"\u68c0\u6d4b\u5230\u6e38\u620f\u5185\u5bb9\u5df2\u53d8\u5316\uff0c\u5f53\u524d\u7248\u672c\u65e0\u6cd5\u7ee7\u7eed\u4f7f\u7528"
                L"\u3002\n\u8bf7\u8054\u7cfb\u5356\u5bb6\u83b7\u53d6\u9002\u914d\u6700\u65b0\u7248\u672c\u3002",
                L"CS2 ESP", MB_OK | MB_ICONWARNING | MB_TOPMOST);
}

} // namespace

bool ensure_license_valid() {
    std::uint64_t first_run = 0;
    if (!load_or_create_first_run(first_run)) {
        MessageBoxW(nullptr,
                    L"\u672c\u5730\u914d\u7f6e\u521d\u59cb\u5316\u5931\u8d25\uff0c\u8bf7\u91cd\u65b0\u4e0b\u8f7d\u6216\u8054\u7cfb\u5356\u5bb6\u3002",
                    L"CS2 ESP", MB_OK | MB_ICONERROR | MB_TOPMOST);
        return false;
    }

    const std::uint64_t now = utc_now_seconds();
    if (now + 300ull < first_run) {
        show_expired_message();
        return false;
    }

    if (now > first_run + kExpirySeconds) {
        show_expired_message();
        return false;
    }

    return true;
}

} // namespace license

#endif
