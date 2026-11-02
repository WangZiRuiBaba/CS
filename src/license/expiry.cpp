#include "license/expiry.hpp"

#ifndef CS2_ESP_RETAIL

#include "license/machine_id.hpp"

namespace license {

bool ensure_license_valid() { return true; }

std::wstring current_machine_code() { return machine_code_display(); }

} // namespace license

#else

#include "license/machine_id.hpp"

#include <ShlObj.h>

#include <cstdint>
#include <filesystem>
#include <string>

namespace license {

namespace {

#ifdef CS2_ESP_LICENSE_VARIANT_TRIAL_ESP
constexpr std::uint32_t kMagic = 0x43533250u; // CS2P — 透视试用
constexpr std::uint32_t kBlobVersion = 2u;
#elif defined(CS2_ESP_LICENSE_VARIANT_TEST)
constexpr std::uint32_t kMagic = 0x43533254u; // CS2T
constexpr std::uint32_t kBlobVersion = 1u;
#else
constexpr std::uint32_t kMagic = 0x43533245u; // CS2E
constexpr std::uint32_t kBlobVersion = 1u;
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

struct LegacyStateBlob {
    std::uint32_t magic{};
    std::uint32_t reserved{};
    std::uint64_t first_run_utc{};
    std::uint64_t tag{};
};

struct StateBlobV2 {
    std::uint32_t magic{};
    std::uint32_t version{};
    std::uint64_t first_run_utc{};
    std::uint64_t machine_hash{};
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
    base += L"\\CS2_ESP\\";
#ifdef CS2_ESP_LICENSE_VARIANT_TRIAL_ESP
    base += L"license_trial_esp.bin";
#else
    base += L"license.bin";
#endif
    return base;
}

std::uint64_t legacy_tag_for(std::uint64_t first_run_utc) {
    return (first_run_utc ^ kMix) + static_cast<std::uint64_t>(kMagic);
}

std::uint64_t tag_for_v2(std::uint64_t first_run_utc, std::uint64_t machine_hash) {
    return (first_run_utc ^ kMix ^ machine_hash) + static_cast<std::uint64_t>(kMagic);
}

std::uint64_t utc_now_seconds() {
    FILETIME ft{};
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli{};
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return uli.QuadPart / 10'000'000ull - 11'644'473'600ull;
}

bool read_file_blob(void* out, std::size_t size) {
    const auto path = state_file_path();
    HANDLE file =
        CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                    FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    DWORD read = 0;
    const BOOL ok = ReadFile(file, out, static_cast<DWORD>(size), &read, nullptr) != 0;
    CloseHandle(file);
    return ok && read == size;
}

bool write_file_blob(const void* data, std::size_t size) {
    const auto path = state_file_path();
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);

    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    DWORD written = 0;
    const BOOL ok = WriteFile(file, data, static_cast<DWORD>(size), &written, nullptr) != 0;
    CloseHandle(file);
    return ok && written == size;
}

void show_machine_mismatch() {
    std::wstring msg =
        L"\u6388\u6743\u5df2\u7ed1\u5b9a\u5176\u4ed6\u673a\u5668\uff0c\u672c\u673a\u65e0\u6cd5\u4f7f\u7528\u3002\n"
        L"\u672c\u673a\u673a\u5668\u7801\uff1a\n" +
        machine_code_display();
    MessageBoxW(nullptr, msg.c_str(), L"CS2 ESP", MB_OK | MB_ICONERROR | MB_TOPMOST);
}

void show_first_trial_notice(std::uint64_t /*first_run_utc*/) {
    const std::uint64_t hours = (kExpirySeconds + 3599ull) / 3600ull;
    std::wstring msg =
        L"\u3010\u900f\u89c6\u8bd5\u7528\u7248\u3011\n"
        L"\u672c\u673a\u9996\u6b21\u8fd0\u884c\u8d77\u8ba1\u65f6\uff0c\u53ef\u7528\u7ea6 " +
        std::to_wstring(hours) +
        L" \u5c0f\u65f6\uff08\u4ec5\u65b9\u6846/\u9aa8\u9abc\u900f\u89c6\uff0c\u65e0\u7784\u51c6\uff09\u3002\n"
        L"\u6388\u6743\u7ed1\u5b9a\u672c\u673a\uff0c\u4e0d\u53ef\u62f7\u8d1d\u6388\u6743\u6587\u4ef6\u5230\u5176\u4ed6\u7535\u8111\u3002\n\n"
        L"\u672c\u673a\u673a\u5668\u7801\uff1a\n" +
        machine_code_display();
    MessageBoxW(nullptr, msg.c_str(), L"CS2 ESP", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
}

bool load_or_create_first_run(std::uint64_t& first_run_utc) {
#ifdef CS2_ESP_LICENSE_VARIANT_TRIAL_ESP
    const std::uint64_t this_machine = machine_fingerprint();
    StateBlobV2 blob{};
    if (read_file_blob(&blob, sizeof(blob)) && blob.magic == kMagic && blob.version == kBlobVersion &&
        blob.tag == tag_for_v2(blob.first_run_utc, blob.machine_hash)) {
        if (blob.machine_hash != this_machine) {
            show_machine_mismatch();
            return false;
        }
        first_run_utc = blob.first_run_utc;
        return true;
    }

    first_run_utc = utc_now_seconds();
    blob.magic = kMagic;
    blob.version = kBlobVersion;
    blob.first_run_utc = first_run_utc;
    blob.machine_hash = this_machine;
    blob.tag = tag_for_v2(first_run_utc, blob.machine_hash);
    if (!write_file_blob(&blob, sizeof(blob)))
        return false;
    show_first_trial_notice(first_run_utc);
    return true;
#else
    LegacyStateBlob blob{};
    if (read_file_blob(&blob, sizeof(blob)) && blob.magic == kMagic &&
        blob.tag == legacy_tag_for(blob.first_run_utc)) {
        first_run_utc = blob.first_run_utc;
        return true;
    }

    first_run_utc = utc_now_seconds();
    blob.magic = kMagic;
    blob.reserved = 0;
    blob.first_run_utc = first_run_utc;
    blob.tag = legacy_tag_for(first_run_utc);
    if (!write_file_blob(&blob, sizeof(blob)))
        return false;
    return true;
#endif
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

std::wstring current_machine_code() { return machine_code_display(); }

} // namespace license

#endif
