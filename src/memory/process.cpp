#include "memory/process.hpp"

#include <TlHelp32.h>

namespace memory {

namespace {

using NtReadVirtualMemoryFn = LONG(NTAPI*)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);
using NtWriteVirtualMemoryFn = LONG(NTAPI*)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);

NtReadVirtualMemoryFn nt_read_virtual_memory() {
    static NtReadVirtualMemoryFn fn = [] {
        const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (!ntdll)
            return static_cast<NtReadVirtualMemoryFn>(nullptr);
        return reinterpret_cast<NtReadVirtualMemoryFn>(
            GetProcAddress(ntdll, "NtReadVirtualMemory"));
    }();
    return fn;
}

NtWriteVirtualMemoryFn nt_write_virtual_memory() {
    static NtWriteVirtualMemoryFn fn = [] {
        const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (!ntdll)
            return static_cast<NtWriteVirtualMemoryFn>(nullptr);
        return reinterpret_cast<NtWriteVirtualMemoryFn>(
            GetProcAddress(ntdll, "NtWriteVirtualMemory"));
    }();
    return fn;
}

bool is_invalid_handle_error() {
    const DWORD err = GetLastError();
    return err == ERROR_INVALID_HANDLE || err == ERROR_ACCESS_DENIED || err == ERROR_PARTIAL_COPY;
}

} // namespace

Process::~Process() { detach(); }

DWORD Process::access_flags(const bool write) const {
    DWORD flags = PROCESS_VM_READ | PROCESS_QUERY_LIMITED_INFORMATION;
    if (write)
        flags |= PROCESS_VM_WRITE;
    return flags;
}

bool Process::open_handle(const bool write) {
    if (pid_ == 0)
        return false;

    if (handle_) {
        CloseHandle(handle_);
        handle_ = nullptr;
    }

    handle_ = OpenProcess(access_flags(write), FALSE, pid_);
    write_access_ = write && handle_ != nullptr;
    return handle_ != nullptr;
}

bool Process::attach(const std::wstring& process_name, const bool request_write_access) {
    detach();

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return false;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    if (!Process32FirstW(snap, &entry)) {
        CloseHandle(snap);
        return false;
    }

    do {
        if (_wcsicmp(entry.szExeFile, process_name.c_str()) != 0)
            continue;

        pid_ = entry.th32ProcessID;
        break;
    } while (Process32NextW(snap, &entry));

    CloseHandle(snap);

    if (pid_ == 0)
        return false;

    if (!open_handle(request_write_access))
        return false;

    client_base_ = get_module_base(L"client.dll");
    if (!client_base_) {
        detach();
        return false;
    }

    last_io_failed_ = false;
    return true;
}

bool Process::ensure_write_access() {
    if (!pid_)
        return false;
    if (write_access_ && handle_)
        return true;
    if (!open_handle(true))
        return false;
    if (!client_base_)
        client_base_ = get_module_base(L"client.dll");
    return client_base_ != 0;
}

bool Process::downgrade_to_read_only() {
    if (!pid_)
        return false;
    if (!write_access_)
        return handle_ != nullptr;
    if (!open_handle(false))
        return false;
    if (!client_base_)
        client_base_ = get_module_base(L"client.dll");
    return client_base_ != 0;
}

bool Process::refresh_handle() {
    if (pid_ == 0)
        return false;
    const bool want_write = write_access_;
    if (!open_handle(want_write))
        return false;
    if (!client_base_)
        client_base_ = get_module_base(L"client.dll");
    return client_base_ != 0;
}

void Process::detach() {
    if (handle_) {
        CloseHandle(handle_);
        handle_ = nullptr;
    }
    pid_ = 0;
    client_base_ = 0;
    write_access_ = false;
    last_io_failed_ = false;
}

void Process::note_io_result(const bool ok) const {
    if (ok) {
        last_io_failed_ = false;
        return;
    }
    if (is_invalid_handle_error())
        last_io_failed_ = true;
}

bool Process::read_raw(std::uintptr_t address, void* buffer, std::size_t size) const {
    if (!handle_ || !address || !buffer || size == 0) {
        note_io_result(false);
        return false;
    }

    SIZE_T bytes_read = 0;

    if (const auto nt_read = nt_read_virtual_memory()) {
        const LONG status =
            nt_read(handle_, reinterpret_cast<PVOID>(address), buffer, size, &bytes_read);
        if (status >= 0 && bytes_read == size) {
            note_io_result(true);
            return true;
        }
        SetLastError(static_cast<DWORD>(status));
    }

    const BOOL ok = ReadProcessMemory(handle_, reinterpret_cast<LPCVOID>(address), buffer, size,
                                      &bytes_read) != 0 &&
                    bytes_read == size;
    note_io_result(ok);
    return ok;
}

bool Process::write_raw(std::uintptr_t address, const void* buffer, std::size_t size) const {
    if (!handle_ || !write_access_ || !address || !buffer || size == 0) {
        note_io_result(false);
        return false;
    }

    SIZE_T bytes_written = 0;

    if (const auto nt_write = nt_write_virtual_memory()) {
        const LONG status =
            nt_write(handle_, reinterpret_cast<PVOID>(address),
                     const_cast<PVOID>(static_cast<const void*>(buffer)), size, &bytes_written);
        if (status >= 0 && bytes_written == size) {
            note_io_result(true);
            return true;
        }
        SetLastError(static_cast<DWORD>(status));
    }

    const BOOL ok = WriteProcessMemory(handle_, reinterpret_cast<LPVOID>(address), buffer, size,
                                       &bytes_written) != 0 &&
                    bytes_written == size;
    note_io_result(ok);
    return ok;
}

std::uintptr_t Process::get_module_base(const std::wstring& module_name) const {
    if (!handle_)
        return 0;

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid_);
    if (snap == INVALID_HANDLE_VALUE)
        return 0;

    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    std::uintptr_t base = 0;
    if (Module32FirstW(snap, &entry)) {
        do {
            if (_wcsicmp(entry.szModule, module_name.c_str()) == 0) {
                base = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
                break;
            }
        } while (Module32NextW(snap, &entry));
    }

    CloseHandle(snap);
    return base;
}

} // namespace memory
