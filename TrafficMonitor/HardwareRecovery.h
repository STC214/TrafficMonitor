#pragma once

#include <atomic>
#include <cstdint>

namespace HardwareRecovery
{
// Counts consecutive failures, not isolated missing/unsupported sensors.
class FailureGate
{
  public:
    bool Observe(bool succeeded)
    {
        if (succeeded)
            m_failures = 0;
        else if (m_failures < 3)
            return ++m_failures == 3;
        return false;
    }
    unsigned Failures() const
    {
        return m_failures;
    }

  private:
    unsigned m_failures{};
};

constexpr std::uint64_t RetryWindowMs = 10 * 60 * 1000;
constexpr unsigned MaxRestarts = 3;

class RetrySchedule
{
  public:
    void Request()
    {
        m_pending = true;
    }
    bool Due(std::uint64_t now) const
    {
        return m_pending && now >= m_next;
    }
    void Defer(std::uint64_t now, std::uint64_t delay)
    {
        m_pending = true;
        m_next = now + delay;
    }
    void Complete()
    {
        m_pending = false;
        m_next = 0;
    }

  private:
    bool m_pending{};
    std::uint64_t m_next{};
};

inline bool BudgetAvailable(unsigned attempts, std::uint64_t window, std::uint64_t now)
{
    return now < window || now - window >= RetryWindowMs || attempts < MaxRestarts;
}
} // namespace HardwareRecovery

#ifdef _WIN32
#include <cwchar>
#include <string>
#include <vector>
#include <windows.h>

namespace HardwareRecovery
{
struct State
{
    FailureGate gate; // Used exclusively by the acquisition thread.
    FailureGate networkGate;
    std::atomic<bool> requested{false};
    RetrySchedule retry; // UI thread only; retains work across timeout/rate-limit/launch failure.
    std::atomic<unsigned> attempts{0};
    std::atomic<ULONGLONG> window{0};
    std::atomic<bool> launching{false}; // Crash recovery may run outside the UI thread.
};

inline State &Current()
{
    static State state;
    return state;
}

// Run before the single-instance mutex: the replacement must wait for its
// parent to exit and release the mutex before opening any hardware handles.
inline bool PrepareStartup(const wchar_t *arguments)
{
    constexpr wchar_t prefix[] = L"--hardware-recovery";
    if (arguments == nullptr)
        return true;
    if (wcsncmp(arguments, prefix, _countof(prefix) - 1) != 0)
        return true;
    DWORD parent{};
    unsigned attempts{};
    unsigned long long window{};
    if (swscanf_s(arguments, L"--hardware-recovery %lu %u %llu", &parent, &attempts, &window) != 3 ||
        parent == GetCurrentProcessId() || parent == 0 || attempts == 0 || attempts > MaxRestarts)
        return false;
    const std::wstring canonical = std::wstring(prefix) + L" " + std::to_wstring(parent) + L" " +
                                   std::to_wstring(attempts) + L" " + std::to_wstring(window);
    if (canonical != arguments || window > GetTickCount64())
        return false;
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, parent);
    if (process != nullptr)
    {
        DWORD result = WaitForSingleObject(process, 30000);
        CloseHandle(process);
        if (result != WAIT_OBJECT_0)
            return false;
    }
    else if (GetLastError() != ERROR_INVALID_PARAMETER)
        return false;
    Sleep(5000); // Allow driver replacement/device enumeration to settle.
    Current().attempts = attempts;
    Current().window = window;
    return true;
}

enum class LaunchResult
{
    Launched,
    RateLimited,
    Failed
};

inline LaunchResult LaunchReplacement(const std::wstring &executable, DWORD *child_id = nullptr)
{
    auto &state = Current();
    if (state.launching.exchange(true))
    {
        SetLastError(ERROR_BUSY);
        return LaunchResult::Failed;
    }
    struct LaunchGuard
    {
        std::atomic<bool> &flag;
        ~LaunchGuard()
        {
            flag = false;
        }
    } guard{state.launching};
    const ULONGLONG now = GetTickCount64();
    if (!BudgetAvailable(state.attempts, state.window, now))
        return LaunchResult::RateLimited;
    if (state.window == 0 || now < state.window || now - state.window >= RetryWindowMs)
    {
        state.window = now;
        state.attempts = 0;
    }
    std::wstring command = L"\"" + executable + L"\" --hardware-recovery " + std::to_wstring(GetCurrentProcessId()) +
                           L" " + std::to_wstring(state.attempts + 1) + L" " + std::to_wstring(state.window);
    std::vector<wchar_t> buffer(command.begin(), command.end());
    buffer.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    // Inherit the current token (including elevation), not shell/runas.
    if (!CreateProcessW(executable.c_str(), buffer.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
                        &startup, &process))
        return LaunchResult::Failed;
    CloseHandle(process.hThread);
    if (child_id != nullptr)
        *child_id = process.dwProcessId;
    CloseHandle(process.hProcess);
    ++state.attempts;
    return LaunchResult::Launched;
}
} // namespace HardwareRecovery
#endif
