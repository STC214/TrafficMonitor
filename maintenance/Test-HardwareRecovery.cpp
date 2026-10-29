#include "../TrafficMonitor/HardwareRecovery.h"
#include "../TrafficMonitor/NetworkTable.h"
#include "../TrafficMonitor/TaskbarPosition.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Check(bool value, const char *name)
{
    std::printf("%s: %s\n", value ? "PASS" : "FAIL", name);
    if (!value)
        std::exit(1);
}

int wmain(int argc, wchar_t **argv)
{
    if (argc > 1)
    {
        std::wstring arguments;
        for (int i = 1; i < argc; ++i)
        {
            if (i > 1)
                arguments += L" ";
            arguments += argv[i];
        }
        if (!HardwareRecovery::PrepareStartup(arguments.c_str()))
            return 2;
        if (HardwareRecovery::Current().attempts != 1)
            return 4; // Only signal success after all child assertions pass.
        wchar_t path[MAX_PATH]{};
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::wstring marker = std::wstring(path) + L".child-ok";
        HANDLE file = CreateFileW(marker.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return 3;
        const char text[] = "parent exited; startup wait completed; recovery history inherited";
        DWORD written{};
        WriteFile(file, text, sizeof(text) - 1, &written, nullptr);
        CloseHandle(file);
        return 0;
    }
    using namespace HardwareRecovery;
    Check(TaskbarPosition::LeftEdge(false, true, 160) == 0, "left edge is zero when widget avoidance is disabled");
    Check(TaskbarPosition::LeftEdge(false, true, 320) == 0, "left edge remains zero at 200 percent DPI");
    Check(TaskbarPosition::LeftEdge(true, false, 160) == 0, "hidden widgets do not reserve left space");
    Check(TaskbarPosition::LeftEdge(true, true, 240) == 240,
          "explicit widget avoidance preserves scaled reserved width");
    Check(TaskbarPosition::LeftEdge(true, true, 0) == 0, "zero widget width also permits an exact left edge");
    Check(TaskbarPosition::LeftEdge(true, true, -160) == 0, "negative widget width does not move off-screen");
    Check(TaskbarPosition::ClientX(3800, 1920) == 1880, "right monitor notification position uses client coordinates");
    Check(TaskbarPosition::ClientX(-40, -1920) == 1880, "left monitor notification position uses client coordinates");
    Check(TaskbarPosition::SnapLeft(600, 200) == 398, "snap placement preserves start-button spacing");
    Check(TaskbarPosition::SnapLeft(100, 200) == 0, "snap placement does not clip past the left edge");
    Check(TaskbarPosition::Vertical(48, 0, 24, 0) == 12, "missing start button falls back to vertical centering");
    Check(TaskbarPosition::Vertical(72, 48, 24, 0) == 36, "touch-taskbar vertical correction is preserved");
    Check(TaskbarPosition::Vertical(48, 48, 80, 0) == 0, "oversized window does not acquire a negative automatic top");
    Check(TaskbarPosition::Vertical(72, 48, 64, 0) == 8,
          "touch correction does not push a tall window below the taskbar");
    Check(TaskbarPosition::Vertical(48, 48, 24, -10) == 2, "explicit vertical offset is preserved");
    bool vertical_bounds = true;
    for (int height : {24, 40, 48, 72, 96, 144})
        for (int start : {0, 24, 48, 72, 200})
            for (int window : {1, 24, 48, 80, 160})
            {
                const int top = TaskbarPosition::Vertical(height, start, window, 0);
                const int maximum = height > window ? height - window : 0;
                vertical_bounds = vertical_bounds && top >= 0 && top <= maximum;
            }
    Check(vertical_bounds, "150 vertical layout combinations stay within automatic bounds");
    bool monitor_coordinates = true;
    for (int origin : {-7680, -3840, -1920, 0, 1920, 3840, 7680})
        for (int position : {0, 48, 960, 1800, 1880, 1920})
            monitor_coordinates =
                monitor_coordinates && TaskbarPosition::ClientX(origin + position, origin) == position;
    Check(monitor_coordinates, "42 mixed-monitor coordinate combinations preserve local positions");
    TaskbarPosition::PlacementCache placement_cache;
    const TaskbarPosition::Placement edge{0, 12, 200, 24};
    Check(placement_cache.NeedsMove(edge, false), "initial placement is applied");
    placement_cache.Commit(edge, true);
    Check(!placement_cache.NeedsMove(edge, false), "unchanged placement avoids redundant window moves");
    Check(placement_cache.NeedsMove({160, 12, 200, 24}, false),
          "widget change invalidates placement without a forced refresh");
    Check(placement_cache.NeedsMove({0, 12, 240, 24}, false), "width change invalidates placement");
    Check(placement_cache.NeedsMove({0, 24, 200, 24}, false), "vertical taskbar change invalidates placement");
    Check(placement_cache.NeedsMove(edge, true), "forced placement is honored even for identical coordinates");
    placement_cache.Commit(edge, false);
    Check(placement_cache.NeedsMove(edge, false), "failed forced move is retried next tick");
    placement_cache.Commit(edge, true);
    placement_cache.Reset();
    Check(placement_cache.NeedsMove(edge, false), "recreated taskbar resets cached placement");
    MIB_IFTABLE *table{};
    DWORD capacity{};
    bool resized{};
    unsigned calls{};
    auto growing = [&](MIB_IFTABLE *value, DWORD *bytes, BOOL) -> DWORD {
        ++calls;
        if (calls < 3)
        {
            *bytes = sizeof(MIB_IFTABLE) * calls;
            return ERROR_INSUFFICIENT_BUFFER;
        }
        value->dwNumEntries = 1;
        return NO_ERROR;
    };
    Check(ReadNetworkTable(table, capacity, resized, growing) == NO_ERROR && calls == 3 && resized &&
              table->dwNumEntries == 1,
          "network buffer grows before successful sample is read");
    const DWORD allocated = capacity;
    auto failing = [](MIB_IFTABLE *, DWORD *bytes, BOOL) -> DWORD {
        *bytes = 1;
        return ERROR_GEN_FAILURE;
    };
    Check(ReadNetworkTable(table, capacity, resized, failing) == ERROR_GEN_FAILURE && capacity == allocated,
          "failed query preserves allocated capacity and returns failure");
    auto unstable = [](MIB_IFTABLE *, DWORD *bytes, BOOL) -> DWORD {
        *bytes = sizeof(MIB_IFTABLE) * 2;
        return ERROR_INSUFFICIENT_BUFFER;
    };
    Check(ReadNetworkTable(table, capacity, resized, unstable) == ERROR_INSUFFICIENT_BUFFER,
          "permanently changing device table has bounded retries");
    auto corrupt = [](MIB_IFTABLE *value, DWORD *, BOOL) -> DWORD {
        value->dwNumEntries = MAXDWORD;
        return NO_ERROR;
    };
    Check(ReadNetworkTable(table, capacity, resized, corrupt) == ERROR_INVALID_DATA,
          "successful query with invalid row count is rejected");
    auto empty = [](MIB_IFTABLE *value, DWORD *, BOOL) -> DWORD {
        value->dwNumEntries = 0;
        return NO_ERROR;
    };
    Check(ReadNetworkTable(table, capacity, resized, empty) == NO_ERROR && table->dwNumEntries == 0,
          "empty device table is valid without any row access");
    MIB_IFROW row{};
    row.dwDescrLen = MAXDWORD;
    std::memset(row.bDescr, 'x', sizeof(row.bDescr));
    Check(NetworkRowDescription(row).size() == sizeof(row.bDescr), "unterminated row description is bounded");
    std::free(table);
    FailureGate gate;
    Check(!gate.Observe(false), "first failure does not restart");
    Check(!gate.Observe(false), "second failure does not restart");
    Check(!gate.Observe(true) && gate.Failures() == 0, "success resets consecutive failures");
    gate.Observe(false);
    gate.Observe(false);
    Check(gate.Observe(false), "third consecutive failure requests restart");
    Check(!gate.Observe(false), "persistent failure does not flood restart requests");
    Check(BudgetAvailable(2, 100, 1000), "third restart allowed");
    Check(!BudgetAvailable(3, 100, 1000), "fourth restart blocked");
    Check(BudgetAvailable(3, 100, 600100), "retry budget resets after ten minutes");
    RetrySchedule retry;
    Check(!retry.Due(0), "idle recovery does not restart");
    retry.Request();
    Check(retry.Due(0), "failure request is scheduled");
    retry.Defer(100, 600000);
    Check(!retry.Due(600099) && retry.Due(600100), "rate-limited recovery resumes after its deadline");
    retry.Defer(600100, 30000);
    Check(!retry.Due(630099) && retry.Due(630100), "failed launch is retried without flooding");
    retry.Complete();
    Check(!retry.Due(999999), "successful launch clears pending recovery");
    Check(!PrepareStartup(L"--hardware-recovery invalid"), "malformed restart argument rejected");
    Check(PrepareStartup(nullptr), "null normal-start arguments accepted");
    Check(!PrepareStartup(L"--hardware-recovery 1 1 1 trailing"), "trailing recovery arguments rejected");
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    Current().launching = true;
    Check(LaunchReplacement(path) == LaunchResult::Failed && GetLastError() == ERROR_BUSY,
          "concurrent launch requests are serialized");
    Current().launching = false;
    Current().attempts = MaxRestarts;
    Current().window = GetTickCount64();
    Check(LaunchReplacement(path) == LaunchResult::RateLimited, "exhausted launch budget creates no child");
    Current().attempts = 0;
    Current().window = 0;
    Check(LaunchReplacement(std::wstring(path) + L".missing") == LaunchResult::Failed && Current().attempts == 0,
          "failed process creation preserves restart budget");
    DWORD child{};
    Check(LaunchReplacement(path, &child) == LaunchResult::Launched, "replacement process launched");
    std::printf("ChildPID=%lu\n", child);
    return 0; // Child must wait until this process exits.
}
