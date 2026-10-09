#pragma once

#include "PluginInterface.h"
#include "WorkdayCalendar.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct CodexOptions
{
    int poll_seconds{60};
    int theme{}; // Host, light, dark
    int bar_style{}; // Segmented, continuous
    int text_style{}; // Compact, detailed
    bool show_session{true};
    bool show_weekly{true};
    bool show_cards{true};
    bool show_time_bar{true};
    int rest_schedule{}; // 0: Sat/Sun off, 1: Sun off, 2/3: alternating ISO weeks
    int morning_start{9 * 60 + 30};
    int morning_end{12 * 60};
    int afternoon_start{13 * 60 + 30};
    int afternoon_end{18 * 60 + 30};
};

struct CodexSnapshot
{
    bool has_session{};
    bool has_weekly{};
    double session_used{};
    double weekly_used{};
    long long session_reset{};
    long long weekly_reset{};
    int reset_credits{-1};
    std::vector<long long> reset_credit_expiries;
    std::vector<std::pair<std::wstring, std::wstring>> account_rows;
    std::wstring status{ L"等待 Codex 数据" };
    std::wstring account_details;
    unsigned long long input_tokens{};
    unsigned long long cached_input_tokens{};
    unsigned long long output_tokens{};
    unsigned long long reasoning_tokens{};
    unsigned long long total_tokens{};
    size_t session_count{};
    size_t unreadable_sessions{};
    CodexCalendar::Calendar calendar;
};

class CCodexUsageItem : public IPluginItem
{
public:
    const wchar_t* GetItemName() const override;
    const wchar_t* GetItemId() const override;
    const wchar_t* GetItemLableText() const override;
    const wchar_t* GetItemValueText() const override;
    const wchar_t* GetItemValueSampleText() const override;
    bool IsCustomDraw() const override { return true; }
    int GetItemWidth() const override { return 279; }
    int GetItemWidthEx(void* hDC) const override;
    void DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode) override;
    int OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag) override;
    int IsDoubleLineExclusive() const override { return 1; }
};

class CCodexUsagePlugin : public ITMPlugin
{
public:
    static CCodexUsagePlugin& Instance();

    IPluginItem* GetItem(int index) override;
    void DataRequired() override;
    const wchar_t* GetInfo(PluginInfoIndex index) override;
    void OnInitialize(ITrafficMonitor* app) override;
    void OnShutdown();
    const wchar_t* GetTooltipInfo() override;
    int GetCommandCount() override { return 2; }
    const wchar_t* GetCommandName(int command_index) override;
    void OnPluginCommand(int command_index, void* hWnd, void* para) override;

    OptionReturn ShowOptionsDialog(void* hParent) override;
    CodexOptions Options() const;
    bool SaveOptions(const CodexOptions& options);
    CodexSnapshot Snapshot() const;
    void RequestRefresh();
    bool IsChinese() const { return m_is_chinese; }

private:
    CCodexUsagePlugin() = default;
    void PollLoop();
    void RefreshSnapshot();

    mutable std::mutex m_options_mutex;
    CodexOptions m_options;
    std::wstring m_config_path;
    CCodexUsageItem m_item;
    ITrafficMonitor* m_app{};
    bool m_is_chinese{ true };
    mutable std::mutex m_snapshot_mutex;
    CodexSnapshot m_snapshot;
    std::mutex m_worker_mutex;
    std::condition_variable m_worker_cv;
    std::atomic<bool> m_stop{ false };
    std::atomic<bool> m_refresh_requested{ false };
    std::thread m_worker;
};

extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance();
