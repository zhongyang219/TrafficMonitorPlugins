// Run from an x64 VS developer prompt, passing the built DLL path.
#include <Windows.h>
#include "../../../include/PluginInterface.h"
#include "../OptionsResource.h"
#include "../CodexUsagePlugin.h"
#include <cassert>
#include <filesystem>
#include <string>
#include <thread>
#include <iostream>

struct Host : ITrafficMonitor
{
    std::wstring directory;
    int GetAPIVersion() override { return 1; }
    const wchar_t* GetVersion() override { return L"test"; }
    double GetMonitorValue(MonitorItem) override { return 0; }
    const wchar_t* GetMonitorValueString(MonitorItem, int) override { return L""; }
    void ShowNotifyMessage(const wchar_t*) override {}
    unsigned short GetLanguageId() const override { return 0x409; }
    const wchar_t* GetPluginConfigDir() const override { return directory.c_str(); }
    int GetDPI(DPIType) const override { return 96; }
    unsigned int GetThemeColor() const override { return 0; }
};

int wmain(int argc, wchar_t** argv)
{
    assert(argc == 2);
    const auto folder = std::filesystem::temp_directory_path() / (L"CodexOptionsTest-" + std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(folder);
    Host host;
    host.directory = folder.wstring();
    SetEnvironmentVariableW(L"CODEX_HOME", host.directory.c_str());
    HMODULE dll = LoadLibraryW(argv[1]);
    assert(dll);
    auto factory = reinterpret_cast<ITMPlugin*(*)()>(GetProcAddress(dll, "TMPluginGetInstance"));
    assert(factory);
    auto plugin = factory();
    plugin->OnInitialize(&host);
    static_cast<CCodexUsagePlugin*>(plugin)->OnShutdown(); // Test UI/config without a polling worker.
    const auto config = (folder / L"CodexUsage.ini").wstring();
    const DWORD ui_thread = GetCurrentThreadId();
    auto dialog = [&](int scenario) {
        std::thread driver([&] {
            HWND window{};
            for (int attempt = 0; attempt < 500 && !window; ++attempt)
            {
                EnumThreadWindows(ui_thread, [](HWND hwnd, LPARAM param) -> BOOL {
                    if (GetDlgItem(hwnd, IDC_POLL)) { *reinterpret_cast<HWND*>(param) = hwnd; return FALSE; }
                    return TRUE;
                }, reinterpret_cast<LPARAM>(&window));
                if (!window) Sleep(10);
            }
            assert(window);
            // Synchronous UI messages wait until WM_INITDIALOG has completed.
            assert(SendDlgItemMessageW(window, IDC_POLL, CB_GETCOUNT, 0, 0) == 4);
            if (scenario == 3)
            {
                assert(SendDlgItemMessageW(window, IDC_POLL, CB_GETCURSEL, 0, 0) == 2);
                assert(SendDlgItemMessageW(window, IDC_THEME, CB_GETCURSEL, 0, 0) == 2);
                assert(SendDlgItemMessageW(window, IDC_WEEKLY, BM_GETCHECK, 0, 0) == BST_UNCHECKED);
            }
            if (scenario <= 1)
            {
                SendDlgItemMessageW(window, IDC_POLL, CB_SETCURSEL, 2, 0);
                SendDlgItemMessageW(window, IDC_THEME, CB_SETCURSEL, 2, 0);
                SendDlgItemMessageW(window, IDC_BAR, CB_SETCURSEL, 1, 0);
                SendDlgItemMessageW(window, IDC_TEXT, CB_SETCURSEL, 1, 0);
                SendDlgItemMessageW(window, IDC_WEEKLY, BM_SETCHECK, BST_UNCHECKED, 0);
                SendDlgItemMessageW(window, IDC_CARDS, BM_SETCHECK, BST_UNCHECKED, 0);
            }
            SendMessageW(window, WM_COMMAND, scenario == 0 || scenario == 3 ? IDCANCEL : IDOK, 0);
        });
        const auto result = plugin->ShowOptionsDialog(nullptr);
        driver.join();
        return result;
    };
    assert(dialog(0) == ITMPlugin::OR_OPTION_UNCHANGED);
    assert(!std::filesystem::exists(config));
    assert(dialog(1) == ITMPlugin::OR_OPTION_CHANGED);
    assert(GetPrivateProfileIntW(L"Options", L"PollSeconds", 0, config.c_str()) == 900);
    assert(GetPrivateProfileIntW(L"Options", L"ShowWeekly", 1, config.c_str()) == 0);
    assert(dialog(2) == ITMPlugin::OR_OPTION_UNCHANGED);
    plugin->OnInitialize(&host);
    static_cast<CCodexUsagePlugin*>(plugin)->OnShutdown();
    assert(dialog(3) == ITMPlugin::OR_OPTION_UNCHANGED);
    HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP bitmap = CreateBitmap(1000, 80, 1, 32, nullptr);
    auto old = SelectObject(dc, bitmap);
    assert(plugin->GetItem(0)->GetItemWidthEx(dc) > 0);
    plugin->GetItem(0)->DrawItem(dc, 0, 0, 1000, 80, false);
    SelectObject(dc, old);
    DeleteObject(bitmap);
    DeleteDC(dc);
    static_cast<CCodexUsagePlugin*>(plugin)->OnShutdown();
    FreeLibrary(dll);
    std::filesystem::remove_all(folder);
    std::cout << "Options dialog: cancel, apply, unchanged, reload and drawing passed\n";
}
