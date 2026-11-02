#include "license/machine_id.hpp"

#include <Windows.h>

#include <array>
#include <cstdio>
#include <string>

namespace license {

namespace {

constexpr std::uint64_t kFnvOffset = 0xcbf29ce484222325ull;
constexpr std::uint64_t kFnvPrime = 0x100000001b3ull;

void mix_bytes(std::uint64_t& hash, const void* data, std::size_t len) {
    const auto* p = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < len; ++i) {
        hash ^= static_cast<std::uint64_t>(p[i]);
        hash *= kFnvPrime;
    }
}

void mix_u32(std::uint64_t& hash, std::uint32_t v) {
    mix_bytes(hash, &v, sizeof(v));
}

bool read_machine_guid(wchar_t* out, DWORD out_chars) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0,
                      KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS)
        return false;

    DWORD type = 0;
    DWORD size = out_chars * sizeof(wchar_t);
    const LSTATUS st =
        RegQueryValueExW(key, L"MachineGuid", nullptr, &type,
                         reinterpret_cast<LPBYTE>(out), &size);
    RegCloseKey(key);
    return st == ERROR_SUCCESS && type == REG_SZ && out[0] != L'\0';
}

} // namespace

std::uint64_t machine_fingerprint() {
    std::uint64_t hash = kFnvOffset;

    std::array<wchar_t, 128> guid{};
    if (read_machine_guid(guid.data(), static_cast<DWORD>(guid.size())))
        mix_bytes(hash, guid.data(), wcslen(guid.data()) * sizeof(wchar_t));

    wchar_t vol_root[] = L"C:\\";
    DWORD serial = 0;
    if (GetVolumeInformationW(vol_root, nullptr, 0, &serial, nullptr, nullptr, nullptr, 0))
        mix_u32(hash, serial);

    wchar_t computer[MAX_COMPUTERNAME_LENGTH + 1]{};
    DWORD comp_len = MAX_COMPUTERNAME_LENGTH + 1;
    if (GetComputerNameW(computer, &comp_len))
        mix_bytes(hash, computer, comp_len * sizeof(wchar_t));

    return hash;
}

std::wstring machine_code_display() {
    const std::uint64_t fp = machine_fingerprint();
    wchar_t buf[32]{};
    swprintf_s(buf, L"%016llX", static_cast<unsigned long long>(fp));
    return buf;
}

} // namespace license
