#pragma once

#include <Windows.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace memory {

class Process {
public:
    Process() = default;
    ~Process();

    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;

    bool attach(const std::wstring& process_name, bool request_write_access = false);
    bool ensure_write_access();
    bool downgrade_to_read_only();
    bool refresh_handle();
    void detach();

    [[nodiscard]] bool is_attached() const { return handle_ != nullptr; }
    [[nodiscard]] bool has_write_access() const { return write_access_; }
    [[nodiscard]] DWORD process_id() const { return pid_; }
    [[nodiscard]] std::uintptr_t client_base() const { return client_base_; }
    [[nodiscard]] bool last_io_failed() const { return last_io_failed_; }
    void clear_io_failed() { last_io_failed_ = false; }

    template <typename T>
    std::optional<T> read(std::uintptr_t address) const {
        T value{};
        if (!read_raw(address, &value, sizeof(T)))
            return std::nullopt;
        return value;
    }

    bool read_raw(std::uintptr_t address, void* buffer, std::size_t size) const;

    template <typename T>
    bool write(std::uintptr_t address, const T& value) const {
        return write_raw(address, &value, sizeof(T));
    }

    bool write_raw(std::uintptr_t address, const void* buffer, std::size_t size) const;

    template <typename T, typename... Offsets>
    std::optional<T> read_chain(std::uintptr_t base, Offsets... offsets) const {
        std::uintptr_t addr = base;
        const int offs[] = {static_cast<int>(offsets)...};
        for (std::size_t i = 0; i < sizeof...(offsets) - 1; ++i) {
            const auto next = read<std::uintptr_t>(addr + offs[i]);
            if (!next || *next == 0)
                return std::nullopt;
            addr = *next;
        }
        return read<T>(addr + offs[sizeof...(offsets) - 1]);
    }

private:
    DWORD access_flags(bool write) const;
    bool open_handle(bool write);
    std::uintptr_t get_module_base(const std::wstring& module_name) const;
    void note_io_result(bool ok) const;

    HANDLE handle_{nullptr};
    DWORD pid_{0};
    std::uintptr_t client_base_{0};
    bool write_access_{false};
    mutable bool last_io_failed_{false};
};

} // namespace memory
