#include <Windows.h>
#include <Winhttp.h>

#include "CodexUsagePlugin.h"
#include "JsonValue.h"
#include "TimeBarMarkers.h"
#include "OptionsResource.h"
#include "PluginTooltipGuard.h"
#include "PluginAppButton.h"
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

#include <algorithm>
#include <ctime>
#include <cmath>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iomanip>
#include <map>
#include <sstream>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace
{
    constexpr wchar_t kUsageHost[] = L"chatgpt.com";
    constexpr wchar_t kUsagePath[] = L"/backend-api/wham/usage";
    constexpr wchar_t kCreditsPath[] = L"/backend-api/wham/rate-limit-reset-credits";
    constexpr DWORD kHttpTimeoutMs = 5000;
    constexpr wchar_t kDetailPopupClass[] = L"TrafficMonitorCodexUsageDetails";

    HWND g_detail_popup{};
    PluginTooltipGuard g_tooltip_guard;
    HINSTANCE g_popup_instance{};
    bool g_popup_tracking_mouse{};
    std::vector<std::wstring> g_popup_lines;
    int g_popup_scroll{};
    int g_popup_dpi{ 96 };
    POINT g_popup_anchor{};
    ULONGLONG g_popup_opened{};

    std::wstring ReadUtf8File(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) return {};
        std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (bytes.empty() || bytes.size() > 32 * 1024 * 1024) return {};
        const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(),
            static_cast<int>(bytes.size()), nullptr, 0);
        if (count <= 0) return {};
        std::wstring result(static_cast<size_t>(count), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(),
            static_cast<int>(bytes.size()), result.data(), count);
        return result;
    }

    std::wstring CodexHome()
    {
        wchar_t value[32768]{};
        DWORD size = GetEnvironmentVariableW(L"CODEX_HOME", value, static_cast<DWORD>(std::size(value)));
        if (size > 0 && size < std::size(value)) return value;
        size = GetEnvironmentVariableW(L"USERPROFILE", value, static_cast<DWORD>(std::size(value)));
        if (size > 0 && size < std::size(value)) return std::wstring(value) + L"\\.codex";
        return {};
    }

    struct Credentials
    {
        std::wstring access_token;
        std::wstring account_id;
    };

    bool ReadCredentials(Credentials& credentials)
    {
        const std::wstring home = CodexHome();
        if (home.empty()) return false;
        JsonValue root;
        if (!ParseJson(ReadUtf8File(std::filesystem::path(home) / L"auth.json"), root)) return false;
        const JsonValue* tokens = root.Get(L"tokens");
        if (tokens == nullptr) return false;
        const JsonValue* access = tokens->Get(L"access_token");
        const JsonValue* account = tokens->Get(L"account_id");
        credentials.access_token = access ? access->String() : L"";
        credentials.account_id = account ? account->String() : L"";
        return !credentials.access_token.empty();
    }

    struct Handle
    {
        HINTERNET value{};
        explicit Handle(HINTERNET handle = nullptr) : value(handle) {}
        ~Handle() { if (value) WinHttpCloseHandle(value); }
        Handle(const Handle&) = delete;
        Handle& operator=(const Handle&) = delete;
        operator HINTERNET() const { return value; }
    };

    bool HttpGet(const wchar_t* path, const Credentials& credentials, std::wstring& body)
    {
        if (credentials.access_token.find_first_of(L"\r\n") != std::wstring::npos ||
            credentials.account_id.find_first_of(L"\r\n") != std::wstring::npos)
            return false;
        Handle session(WinHttpOpen(L"codex-cli", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
        if (!session.value) return false;
        WinHttpSetTimeouts(session, kHttpTimeoutMs, kHttpTimeoutMs, kHttpTimeoutMs, kHttpTimeoutMs);
        Handle connection(WinHttpConnect(session, kUsageHost, INTERNET_DEFAULT_HTTPS_PORT, 0));
        if (!connection.value) return false;
        Handle request(WinHttpOpenRequest(connection, L"GET", path, nullptr,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
        if (!request.value) return false;

        std::wstring headers = L"Authorization: Bearer " + credentials.access_token +
            L"\r\nUser-Agent: codex-cli\r\nAccept: application/json\r\n";
        if (!credentials.account_id.empty())
            headers += L"ChatGPT-Account-Id: " + credentials.account_id + L"\r\n";
        const BOOL sent = WinHttpSendRequest(request, headers.c_str(), static_cast<DWORD>(headers.size()),
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
        if (!headers.empty()) SecureZeroMemory(headers.data(), headers.size() * sizeof(wchar_t));
        if (!sent || !WinHttpReceiveResponse(request, nullptr)) return false;

        DWORD status = 0, status_size = sizeof(status);
        if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX) || status < 200 || status >= 300)
            return false;

        std::string bytes;
        for (;;)
        {
            DWORD available = 0;
            if (!WinHttpQueryDataAvailable(request, &available)) return false;
            if (available == 0) break;
            if (bytes.size() + available > 4 * 1024 * 1024) return false;
            const size_t old_size = bytes.size();
            bytes.resize(old_size + available);
            DWORD read = 0;
            if (!WinHttpReadData(request, bytes.data() + old_size, available, &read)) return false;
            bytes.resize(old_size + read);
        }
        if (bytes.empty()) return false;
        const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(),
            static_cast<int>(bytes.size()), nullptr, 0);
        if (count <= 0) return false;
        body.resize(static_cast<size_t>(count));
        return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(),
            static_cast<int>(bytes.size()), body.data(), count) == count;
    }

    const JsonValue* At(const JsonValue* value, std::initializer_list<const wchar_t*> path)
    {
        for (const wchar_t* key : path)
        {
            value = value ? value->Get(key) : nullptr;
            if (value == nullptr) break;
        }
        return value;
    }

    long long UnixSeconds(const JsonValue* value)
    {
        if (value == nullptr || value->type != JsonValue::Type::Number) return 0;
        return static_cast<long long>(value->number);
    }

    long long CreditExpiry(const JsonValue* value)
    {
        if (!value) return 0;
        if (value->type == JsonValue::Type::Number) return UnixSeconds(value);
        const std::wstring text = value->String();
        int year{}, month{}, day{}, hour{}, minute{}, second{}, consumed{};
        if (swscanf_s(text.c_str(), L"%d-%d-%dT%d:%d:%d%n", &year, &month, &day,
            &hour, &minute, &second, &consumed) != 6) return 0;
        std::tm utc{};
        utc.tm_year = year - 1900; utc.tm_mon = month - 1; utc.tm_mday = day;
        utc.tm_hour = hour; utc.tm_min = minute; utc.tm_sec = second;
        long long timestamp = _mkgmtime64(&utc);
        size_t zone = static_cast<size_t>(consumed);
        if (zone < text.size() && text[zone] == L'.')
            while (++zone < text.size() && iswdigit(text[zone])) {}
        if (zone < text.size() && (text[zone] == L'+' || text[zone] == L'-'))
        {
            int offset_hour{}, offset_minute{};
            if (swscanf_s(text.c_str() + zone + 1, L"%d:%d", &offset_hour, &offset_minute) != 2) return 0;
            timestamp -= (text[zone] == L'+' ? 1 : -1) * (offset_hour * 3600 + offset_minute * 60);
        }
        return timestamp > 0 ? timestamp : 0;
    }

    void SetWindow(CodexSnapshot& snapshot, const JsonValue* window, bool weekly)
    {
        if (window == nullptr || window->type != JsonValue::Type::Object) return;
        const JsonValue* used = window->Get(L"used_percent");
        if (used == nullptr || used->type != JsonValue::Type::Number) return;
        const long long reset = UnixSeconds(window->Get(L"reset_at"));
        if (weekly)
        {
            snapshot.has_weekly = true;
            snapshot.weekly_used = std::clamp(used->number, 0.0, 100.0);
            snapshot.weekly_reset = reset;
        }
        else
        {
            snapshot.has_session = true;
            snapshot.session_used = std::clamp(used->number, 0.0, 100.0);
            snapshot.session_reset = reset;
        }
    }

    std::wstring FieldLabel(const std::wstring& key, bool chinese)
    {
        static const std::map<std::wstring, std::pair<const wchar_t*, const wchar_t*>> labels{
            { L"plan_type", { L"套餐", L"Plan" } },
            { L"rate_limit", { L"额度限制", L"Rate limits" } },
            { L"primary_window", { L"5 小时窗口", L"Primary window" } },
            { L"secondary_window", { L"7 天窗口", L"Secondary window" } },
            { L"used_percent", { L"已用百分比", L"Used percent" } },
            { L"reset_at", { L"重置时间戳", L"Reset timestamp" } },
            { L"limit_window_seconds", { L"窗口长度秒数", L"Window seconds" } },
            { L"rate_limit_reset_credits", { L"重置额度", L"Reset credits" } },
            { L"available_count", { L"可用数量", L"Available count" } },
            { L"credits", { L"额度明细", L"Credit details" } },
            { L"status", { L"状态", L"Status" } },
            { L"expires_at", { L"过期时间", L"Expires at" } },
            { L"model_usage", { L"模型用量", L"Model usage" } },
            { L"additional_rate_limits", { L"额外额度窗口", L"Additional limits" } },
            { L"code_review_rate_limit", { L"代码审查额度", L"Code review limits" } },
            { L"chatpass", { L"Chatpass", L"Chatpass" } },
            { L"spend_control", { L"消费控制", L"Spend control" } },
            { L"enabled", { L"已启用", L"Enabled" } },
            { L"allowed", { L"可用", L"Allowed" } },
            { L"limit", { L"限制", L"Limit" } },
            { L"used", { L"已用", L"Used" } },
            { L"name", { L"名称", L"Name" } },
            { L"model", { L"模型", L"Model" } },
            { L"remaining", { L"剩余", L"Remaining" } },
        };
        const auto found = labels.find(key);
        if (found != labels.end()) return chinese ? found->second.first : found->second.second;
        std::wstring label = key;
        std::replace(label.begin(), label.end(), L'_', L' ');
        if (!chinese && !label.empty()) label[0] = static_cast<wchar_t>(towupper(label[0]));
        return label;
    }

    std::wstring FormatBeijingTime(long long unix_seconds)
    {
        if (unix_seconds <= 0) return std::to_wstring(unix_seconds);
        const std::time_t beijing_time = static_cast<std::time_t>(unix_seconds + 8 * 60 * 60);
        std::tm value{};
        if (gmtime_s(&value, &beijing_time) != 0) return std::to_wstring(unix_seconds);
        std::wostringstream text;
        text << (value.tm_year + 1900) << L"-" << std::setw(2) << std::setfill(L'0') << (value.tm_mon + 1)
            << L"-" << std::setw(2) << std::setfill(L'0') << value.tm_mday << L" "
            << std::setw(2) << std::setfill(L'0') << value.tm_hour << L":"
            << std::setw(2) << std::setfill(L'0') << value.tm_min << L" (UTC+8)";
        return text.str();
    }

    bool IsSensitiveField(const std::wstring& key)
    {
        std::wstring lower = key;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t ch) {
            return static_cast<wchar_t>(std::towlower(ch));
        });
        return lower.find(L"token") != std::wstring::npos || lower.find(L"email") != std::wstring::npos ||
            lower == L"id" || lower == L"account_id" || lower == L"user_id" ||
            (lower.size() > 3 && lower.substr(lower.size() - 3) == L"_id");
    }

    void AppendJsonFields(const JsonValue& value, const std::wstring& path, bool chinese,
        std::wostringstream& out, size_t& row_count, unsigned depth)
    {
        if (row_count >= 80 || depth > 6 || out.tellp() > 7000) return;
        if (value.type == JsonValue::Type::Object)
        {
            for (const auto& [key, child] : value.object)
            {
                if (IsSensitiveField(key)) continue;
                const std::wstring label = FieldLabel(key, chinese);
                AppendJsonFields(child, path.empty() ? label : path + L" / " + label,
                    chinese, out, row_count, depth + 1);
                if (row_count >= 80) break;
            }
        }
        else if (value.type == JsonValue::Type::Array)
        {
            const size_t count = (std::min)(value.array.size(), static_cast<size_t>(16));
            for (size_t i = 0; i < count && row_count < 80; ++i)
                AppendJsonFields(value.array[i], path + L" #" + std::to_wstring(i + 1),
                    chinese, out, row_count, depth + 1);
        }
        else if (value.type == JsonValue::Type::String && !value.string.empty())
        {
            out << path << L": " << value.string.substr(0, 256) << L"\r\n";
            ++row_count;
        }
        else if (value.type == JsonValue::Type::Number)
        {
            const bool reset_time = path.find(L"Reset timestamp") != std::wstring::npos || path.find(L"重置时间戳") != std::wstring::npos;
            if (reset_time && value.number > 1000000000.0)
                out << path << L": " << FormatBeijingTime(static_cast<long long>(value.number));
            else
            {
                out << path << L": " << value.number;
                if ((path.size() >= 6 && path.substr(path.size() - 6) == L"percent") ||
                    (path.size() >= 3 && path.substr(path.size() - 3) == L"百分比")) out << L"%";
            }
            out << L"\r\n";
            ++row_count;
        }
        else if (value.type == JsonValue::Type::Boolean)
        {
            out << path << L": " << (chinese ? (value.boolean ? L"是" : L"否") : (value.boolean ? L"yes" : L"no")) << L"\r\n";
            ++row_count;
        }
    }

    void ParseUsage(const JsonValue& root, CodexSnapshot& snapshot, bool chinese)
    {
        const JsonValue* rate_limit = root.Get(L"rate_limit");
        const JsonValue* primary = rate_limit ? rate_limit->Get(L"primary_window") : nullptr;
        const JsonValue* secondary = rate_limit ? rate_limit->Get(L"secondary_window") : nullptr;
        bool has_short = false, has_long = false;
        for (const JsonValue* window : { primary, secondary })
        {
            const JsonValue* duration = window ? window->Get(L"limit_window_seconds") : nullptr;
            if (duration && duration->type == JsonValue::Type::Number)
            {
                if (duration->number <= 86400 && !has_short) { SetWindow(snapshot, window, false); has_short = snapshot.has_session; }
                else if (duration->number > 86400 && !has_long) { SetWindow(snapshot, window, true); has_long = snapshot.has_weekly; }
            }
        }
        // Older API responses omit window duration and preserve primary/secondary ordering.
        if (!snapshot.has_session) SetWindow(snapshot, primary, false);
        if (!snapshot.has_weekly) SetWindow(snapshot, secondary, true);

        const JsonValue* credits = At(&root, { L"rate_limit_reset_credits", L"available_count" });
        if (credits && credits->type == JsonValue::Type::Number)
            snapshot.reset_credits = static_cast<int>(credits->number);

        std::wostringstream details;
        details << (chinese ? L"Codex 账号信息\r\n" : L"Codex account details\r\n");
        size_t rows = 0;
        AppendJsonFields(root, L"", chinese, details, rows, 0);
        snapshot.account_details = details.str();
        const auto add = [&](const wchar_t* label, const JsonValue* value) {
            if (!value) return;
            std::wstring text;
            if (value->type == JsonValue::Type::String) text = value->string;
            else if (value->type == JsonValue::Type::Boolean) text = value->boolean ? (chinese ? L"是" : L"Yes") : (chinese ? L"否" : L"No");
            else if (value->type == JsonValue::Type::Number) { std::wostringstream out; out << value->number; text = out.str(); }
            else return;
            snapshot.account_rows.emplace_back(label, text);
        };
        add(chinese ? L"套餐" : L"Plan", root.Get(L"plan_type"));
        add(chinese ? L"允许使用" : L"Usage allowed", At(&root, { L"rate_limit", L"allowed" }));
        add(chinese ? L"额度耗尽" : L"Quota exhausted", At(&root, { L"rate_limit", L"limit_reached" }));
        snapshot.account_rows.emplace_back(chinese ? L"额外额度" : L"Extra credits", L"");
        add(chinese ? L"余额" : L"Balance", At(&root, { L"credit_details", L"balance" }));
        add(chinese ? L"有可用额度" : L"Has credits", At(&root, { L"credit_details", L"has_credits" }));
        add(chinese ? L"无限额度" : L"Unlimited", At(&root, { L"credit_details", L"unlimited" }));
        add(chinese ? L"已达超额限制" : L"Overage limit reached", At(&root, { L"credit_details", L"overage_limit_reached" }));
        const JsonValue* models = root.Get(L"model_usage");
        if (models && models->type == JsonValue::Type::Object)
            for (const auto& model : models->object)
            {
                snapshot.account_rows.emplace_back(model.first, L"");
                add(chinese ? L"可用" : L"Available", model.second.Get(L"available"));
                add(chinese ? L"额度可启用" : L"Credits can enable", model.second.Get(L"credits_would_enable"));
            }
    }

    unsigned long long JsonUnsigned(const JsonValue* value)
    {
        if (value == nullptr || value->type != JsonValue::Type::Number || value->number < 0) return 0;
        return static_cast<unsigned long long>(value->number);
    }

    bool AddTokenUsage(const JsonValue& root, CodexSnapshot& current)
    {
        const JsonValue* type = root.Get(L"type");
        const JsonValue* payload = root.Get(L"payload");
        if (type == nullptr || type->String() != L"event_msg" || payload == nullptr) return false;
        const JsonValue* payload_type = payload->Get(L"type");
        if (payload_type == nullptr || payload_type->String() != L"token_count") return false;
        const JsonValue* info = payload->Get(L"info");
        const JsonValue* usage = info ? info->Get(L"total_token_usage") : nullptr;
        if (usage == nullptr || usage->type != JsonValue::Type::Object) return false;
        current.input_tokens = JsonUnsigned(usage->Get(L"input_tokens"));
        current.cached_input_tokens = JsonUnsigned(usage->Get(L"cached_input_tokens"));
        current.output_tokens = JsonUnsigned(usage->Get(L"output_tokens"));
        current.reasoning_tokens = JsonUnsigned(usage->Get(L"reasoning_output_tokens"));
        current.total_tokens = JsonUnsigned(usage->Get(L"total_tokens"));
        return true;
    }

    unsigned long long TokenDelta(unsigned long long current, unsigned long long previous)
    {
        // A reset starts a new cumulative sequence within the same log.
        return current >= previous ? current - previous : current;
    }

    void ScanSessionFile(const std::filesystem::path& path, std::map<std::wstring, CodexSnapshot>& sessions,
        size_t& unreadable, long long day_start, long long day_end)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) { ++unreadable; return; }
        std::string line;
        CodexSnapshot previous, today;
        bool found_usage = false;
        while (std::getline(file, line))
        {
            if (line.find("\"token_count\"") == std::string::npos) continue;
            const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, line.data(),
                static_cast<int>(line.size()), nullptr, 0);
            if (count <= 0) continue;
            std::wstring wide(static_cast<size_t>(count), L'\0');
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, line.data(),
                static_cast<int>(line.size()), wide.data(), count);
            JsonValue value;
            CodexSnapshot current;
            if (!ParseJson(wide, value) || !AddTokenUsage(value, current)) continue;
            const long long timestamp = CreditExpiry(value.Get(L"timestamp"));
            if (timestamp >= day_start && timestamp < day_end)
            {
                today.input_tokens += TokenDelta(current.input_tokens, previous.input_tokens);
                today.cached_input_tokens += TokenDelta(current.cached_input_tokens, previous.cached_input_tokens);
                today.output_tokens += TokenDelta(current.output_tokens, previous.output_tokens);
                today.reasoning_tokens += TokenDelta(current.reasoning_tokens, previous.reasoning_tokens);
                today.total_tokens += TokenDelta(current.total_tokens, previous.total_tokens);
                found_usage = true;
            }
            // Keep yesterday's cumulative value as the baseline for the first event today.
            previous = current;
        }
        if (file.bad()) ++unreadable;
        if (found_usage)
        {
            const std::wstring key = path.filename().wstring();
            const auto found = sessions.find(key);
            if (found == sessions.end() || today.total_tokens > found->second.total_tokens)
                sessions[key] = today;
        }
    }

    void ScanSessions(CodexSnapshot& snapshot)
    {
        const std::wstring home = CodexHome();
        if (home.empty()) return;
        SYSTEMTIME local{};
        GetLocalTime(&local);
        std::tm midnight{};
        midnight.tm_year = local.wYear - 1900;
        midnight.tm_mon = local.wMonth - 1;
        midnight.tm_mday = local.wDay;
        midnight.tm_isdst = -1;
        const long long day_start = _mktime64(&midnight);
        ++midnight.tm_mday;
        midnight.tm_isdst = -1;
        const long long day_end = _mktime64(&midnight);
        const std::filesystem::path root(home);
        std::map<std::wstring, CodexSnapshot> sessions;
        for (const auto& folder : { root / L"sessions", root / L"archived_sessions" })
        {
            std::error_code error;
            for (std::filesystem::recursive_directory_iterator it(folder,
                std::filesystem::directory_options::skip_permission_denied, error), end;
                !error && it != end; it.increment(error))
            {
                if (!it->is_regular_file(error) || it->path().extension() != L".jsonl") continue;
                // Older chats may still be active today. Skip only logs not updated since midnight.
                WIN32_FILE_ATTRIBUTE_DATA attributes{};
                if (!GetFileAttributesExW(it->path().c_str(), GetFileExInfoStandard, &attributes))
                {
                    ++snapshot.unreadable_sessions;
                    continue;
                }
                ULARGE_INTEGER written{};
                written.LowPart = attributes.ftLastWriteTime.dwLowDateTime;
                written.HighPart = attributes.ftLastWriteTime.dwHighDateTime;
                const long long modified = static_cast<long long>(written.QuadPart / 10000000ULL) - 11644473600LL;
                if (modified < day_start) continue;
                ScanSessionFile(it->path(), sessions, snapshot.unreadable_sessions, day_start, day_end);
            }
        }
        for (const auto& entry : sessions)
        {
            snapshot.input_tokens += entry.second.input_tokens;
            snapshot.cached_input_tokens += entry.second.cached_input_tokens;
            snapshot.output_tokens += entry.second.output_tokens;
            snapshot.reasoning_tokens += entry.second.reasoning_tokens;
            snapshot.total_tokens += entry.second.total_tokens;
        }
        snapshot.session_count = sessions.size();
    }

    std::wstring FormatCount(unsigned long long value)
    {
        std::wstring digits = std::to_wstring(value);
        for (int i = static_cast<int>(digits.size()) - 3; i > 0; i -= 3) digits.insert(static_cast<size_t>(i), L",");
        return digits;
    }

    std::wstring FormatTotalCount(unsigned long long value, bool chinese)
    {
        std::wstring scaled;
        if (value > 0 && value < 10000)
            scaled = L"<0.0001";
        else
        {
            std::wostringstream out;
            out << std::fixed << std::setprecision(4) << (value / 100000000.0);
            scaled = out.str();
            while (!scaled.empty() && scaled.back() == L'0') scaled.pop_back();
            if (!scaled.empty() && scaled.back() == L'.') scaled.pop_back();
        }
        return FormatCount(value) + (chinese ? L"（" : L" (") + scaled
            + (chinese ? L" 亿）" : L" x 100M)");
    }

    std::wstring Countdown(long long reset, bool weekly)
    {
        if (reset <= 0) return L"--";
        const long long remaining = (std::max)(0LL, reset - static_cast<long long>(time(nullptr)));
        const long long hours = remaining / 3600;
        const long long minutes = (remaining % 3600) / 60;
        if (remaining < 3600)
            return std::to_wstring(remaining / 60) + L"m" + std::to_wstring(remaining % 60) + L"s";
        if (weekly && remaining < 86400)
            return std::to_wstring(hours) + L"h" + std::to_wstring(minutes) + L"m";
        if (hours >= 24)
        {
            std::wostringstream out; out << (hours / 24) << L"d " << (hours % 24) << L"h"; return out.str();
        }
        std::wostringstream out; out << hours << L"h " << minutes << L"m"; return out.str();
    }

    std::wstring ResetBadge(long long expiry)
    {
        if (expiry <= 0) return L"?";
        const long long seconds = (std::max)(0LL, expiry - static_cast<long long>(time(nullptr)));
        if (seconds < 3600) return std::to_wstring(seconds / 60) + L"m";
        if (seconds < 86400) return std::to_wstring(seconds / 3600) + L"h";
        return std::to_wstring(seconds / 86400);
    }

    std::wstring DetailedCountdown(long long expiry)
    {
        if (expiry <= 0) return L"--";
        const long long seconds = (std::max)(0LL, expiry - static_cast<long long>(time(nullptr)));
        return std::to_wstring(seconds / 86400) + L"d " + std::to_wstring(seconds / 3600 % 24)
            + L"h " + std::to_wstring(seconds / 60 % 60) + L"m " + std::to_wstring(seconds % 60) + L"s";
    }

    std::wstring QuotaSummary(const CodexSnapshot& snapshot, bool weekly)
    {
        const bool available = weekly ? snapshot.has_weekly : snapshot.has_session;
        if (!available) return L"--";
        const double remaining = std::clamp(100.0 - (weekly ? snapshot.weekly_used : snapshot.session_used), 0.0, 100.0);
        if (CCodexUsagePlugin::Instance().Options().text_style == 1)
            return std::wstring(CCodexUsagePlugin::Instance().IsChinese() ? L"剩余 " : L"Remaining ")
                + std::to_wstring(static_cast<int>(std::lround(remaining))) + L"% · "
                + (CCodexUsagePlugin::Instance().IsChinese() ? L"重置 " : L"Resets ")
                + Countdown(weekly ? snapshot.weekly_reset : snapshot.session_reset, weekly);
        return std::wstring(CCodexUsagePlugin::Instance().IsChinese() ? L"" : L"")
            + std::to_wstring(static_cast<int>(std::lround(remaining))) + L"% · "
            + Countdown(weekly ? snapshot.weekly_reset : snapshot.session_reset, weekly);
    }

    void DrawText(HDC dc, const RECT& rect, const std::wstring& text, COLORREF color, UINT align = DT_LEFT)
    {
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, color);
        RECT copy = rect;
        ::DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &copy, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | align);
    }

    void RefreshPopupLines(bool reset_scroll = true)
    {
        g_popup_lines.clear();
        const auto snapshot = CCodexUsagePlugin::Instance().Snapshot();
        const bool cn = CCodexUsagePlugin::Instance().IsChinese();
        const auto row = [&](const wchar_t* title, const std::wstring& value) { g_popup_lines.push_back(std::wstring(title) + L"\t" + value); };
        g_popup_lines.push_back(cn ? L"今日本地会话 · Token" : L"Today's local sessions · Tokens");
        row(cn ? L"输入" : L"Input", FormatCount(snapshot.input_tokens));
        row(cn ? L"缓存输入（包含于输入）" : L"Cached input (included)", FormatCount(snapshot.cached_input_tokens));
        row(cn ? L"输出" : L"Output", FormatCount(snapshot.output_tokens));
        row(cn ? L"推理输出（包含于输出）" : L"Reasoning output (included)", FormatCount(snapshot.reasoning_tokens));
        row(cn ? L"合计" : L"Total", FormatTotalCount(snapshot.total_tokens, cn));
        row(cn ? L"会话 / 不可读日志" : L"Sessions / unreadable", std::to_wstring(snapshot.session_count) + L" / " + std::to_wstring(snapshot.unreadable_sessions));
        g_popup_lines.push_back(cn ? L"账号" : L"Account");
        row(cn ? L"连接状态" : L"Connection", snapshot.status);
        for (const auto& account : snapshot.account_rows)
            g_popup_lines.push_back(account.first + (account.second.empty() ? L"" : L"\t" + account.second));
        g_popup_lines.push_back(cn ? L"额度窗口 · 北京时间" : L"Quota windows · Beijing time");
        for (bool weekly : { false, true })
        {
            const long long reset = weekly ? snapshot.weekly_reset : snapshot.session_reset;
            row(weekly ? (cn ? L"7 天重置" : L"7-day reset") : (cn ? L"5 小时重置" : L"5-hour reset"), reset > 0 ? FormatBeijingTime(reset) : L"--");
            row(cn ? L"倒计时" : L"Countdown", DetailedCountdown(reset));
        }
        if (reset_scroll) g_popup_scroll = 0;
        else
        {
            RECT client{}; GetClientRect(g_detail_popup, &client);
            const int line_height = MulDiv(22, g_popup_dpi, 96);
            const int page = (std::max)(1, (static_cast<int>(client.bottom) - MulDiv(50, g_popup_dpi, 96)) / line_height);
            const int rows = (std::max)(static_cast<int>(g_popup_lines.size()), 2 + static_cast<int>(snapshot.reset_credit_expiries.size()) * 5);
            g_popup_scroll = std::clamp(g_popup_scroll, 0, (std::max)(0, rows - page));
        }
    }

    void HidePopupIfCursorOutside()
    {
        if (!g_detail_popup || !IsWindowVisible(g_detail_popup)) return;
        POINT cursor{};
        RECT popup_rect{};
        if (!GetCursorPos(&cursor) || !GetWindowRect(g_detail_popup, &popup_rect)) return;
        const bool outside = !PtInRect(&popup_rect, cursor);
        const bool on_anchor = std::abs(cursor.x - g_popup_anchor.x) <= 12 && std::abs(cursor.y - g_popup_anchor.y) <= 12;
        const bool clicked = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) || (GetAsyncKeyState(VK_RBUTTON) & 0x8000);
        if (GetTickCount64() - g_popup_opened > 600 && outside && (!on_anchor || clicked))
            ShowWindow(g_detail_popup, SW_HIDE);
    }

    void DestroyDetailPopup()
    {
        if (g_detail_popup && IsWindow(g_detail_popup))
            DestroyWindow(g_detail_popup);
        g_detail_popup = nullptr;
        if (g_popup_instance)
            UnregisterClassW(kDetailPopupClass, g_popup_instance);
        g_popup_instance = nullptr;
    }

    LRESULT CALLBACK DetailPopupProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_SHOWWINDOW:
            if (wParam) g_tooltip_guard.Acquire(GetWindow(hwnd, GW_OWNER));
            else g_tooltip_guard.Release();
            break;
        case WM_ERASEBKGND:
            return 1;
        case WM_MOUSEMOVE:
            if (!g_popup_tracking_mouse)
            {
                TRACKMOUSEEVENT tracking{ sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0 };
                g_popup_tracking_mouse = TrackMouseEvent(&tracking) != FALSE;
            }
            break;
        case WM_MOUSELEAVE:
            g_popup_tracking_mouse = false;
            HidePopupIfCursorOutside();
            break;
        case WM_MOUSEWHEEL:
        {
            const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            g_popup_scroll = (std::max)(0, g_popup_scroll + (delta < 0 ? 3 : -3));
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_TIMER:
            if (wParam == 2)
            {
                if (IsWindowVisible(hwnd) && (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) ShowWindow(hwnd, SW_HIDE);
                HidePopupIfCursorOutside(); return 0;
            }
            if (!IsWindowVisible(hwnd)) return 0;
            RefreshPopupLines(false);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_PAINT:
        {
            RefreshPopupLines(false);
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(hwnd, &paint);
            RECT rect{};
            GetClientRect(hwnd, &rect);
            HBRUSH background = CreateSolidBrush(RGB(255, 251, 225));
            FillRect(dc, &rect, background);
            DeleteObject(background);

            const bool chinese = CCodexUsagePlugin::Instance().IsChinese();
            const int line_height = MulDiv(22, g_popup_dpi, 96);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(46, 48, 52));
            HFONT title_font = CreateFontW(MulDiv(18, g_popup_dpi, 96), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            HGDIOBJ old_font = SelectObject(dc, title_font);
            PluginAppButton::Draw(hwnd, dc, g_popup_dpi, true);
            RECT title{ MulDiv(14, g_popup_dpi, 96), MulDiv(10, g_popup_dpi, 96), rect.right - MulDiv(14, g_popup_dpi, 96), MulDiv(38, g_popup_dpi, 96) };
            const wchar_t* title_text = chinese ? L"Codex 用量与 Token" : L"Codex usage and tokens";
            ::DrawTextW(dc, title_text, -1, &title, DT_SINGLELINE | DT_VCENTER);
            RECT close{ rect.right - MulDiv(32, g_popup_dpi, 96), 0, rect.right, MulDiv(32, g_popup_dpi, 96) };
            ::DrawTextW(dc, L"\u00d7", 1, &close, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(dc, old_font);
            DeleteObject(title_font);
            HFONT body_font = CreateFontW(-MulDiv(13, g_popup_dpi, 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            old_font = SelectObject(dc, body_font);

            const int content_top = MulDiv(42, g_popup_dpi, 96);
            RECT clip{ MulDiv(10, g_popup_dpi, 96), content_top, rect.right - MulDiv(10, g_popup_dpi, 96), rect.bottom - MulDiv(8, g_popup_dpi, 96) };
            IntersectClipRect(dc, clip.left, clip.top, clip.right, clip.bottom);
            const int pad = MulDiv(16, g_popup_dpi, 96);
            const int column_right = rect.right * 57 / 100;
            const int value_x = pad + (column_right - pad * 2) * 52 / 100;
            for (size_t i = static_cast<size_t>(g_popup_scroll); i < g_popup_lines.size(); ++i)
            {
                const int top = content_top + static_cast<int>(i - g_popup_scroll) * line_height;
                if (top >= clip.bottom) break;
                const auto& text = g_popup_lines[i];
                const size_t split = text.find(L'\t');
                RECT label{ pad, top, split == std::wstring::npos ? column_right - pad : value_x - MulDiv(8, g_popup_dpi, 96), top + line_height };
                if (split == std::wstring::npos)
                {
                    HBRUSH heading = CreateSolidBrush(RGB(241, 234, 201));
                    FillRect(dc, &label, heading); DeleteObject(heading);
                }
                DrawText(dc, label, text.substr(0, split), RGB(60, 64, 70));
                if (split != std::wstring::npos)
                {
                    RECT value{ value_x, top, column_right - pad, top + line_height };
                    DrawText(dc, value, text.substr(split + 1), RGB(32, 35, 40));
                }
            }
            const auto snapshot = CCodexUsagePlugin::Instance().Snapshot();
            const int right_left = column_right + MulDiv(8, g_popup_dpi, 96);
            const int right_edge = rect.right - pad;
            const int right_top = content_top - g_popup_scroll * line_height;
            RECT card_heading{ right_left, right_top, right_edge, right_top + line_height };
            DrawText(dc, card_heading, chinese ? L"重置卡 · 北京时间（UTC+8）" : L"Reset cards · Beijing (UTC+8)", RGB(60, 64, 70));
            RECT count_rect{ right_left, right_top + line_height, right_edge, right_top + 2 * line_height };
            DrawText(dc, count_rect, (chinese ? L"可用数量：" : L"Available: ") + (snapshot.reset_credits >= 0 ? std::to_wstring(snapshot.reset_credits) : L"--"), RGB(60, 64, 70));
            for (size_t i = 0; i < snapshot.reset_credit_expiries.size(); ++i)
            {
                const int card_top = right_top + (2 + static_cast<int>(i) * 5) * line_height;
                RECT card{ right_left, card_top, right_edge, card_top + 4 * line_height };
                HBRUSH background_card = CreateSolidBrush(RGB(255, 254, 243));
                FillRect(dc, &card, background_card); DeleteObject(background_card);
                HBRUSH outline = CreateSolidBrush(RGB(214, 206, 175));
                FrameRect(dc, &card, outline); DeleteObject(outline);
                const long long expiry = snapshot.reset_credit_expiries[i];
                const std::wstring labels[] = {
                    (chinese ? L"重置卡 " : L"Reset card ") + std::to_wstring(i + 1),
                    chinese ? L"状态：可用" : L"Status: available",
                    (chinese ? L"到期：" : L"Expires: ") + (expiry > 0 ? FormatBeijingTime(expiry) : L"--"),
                    (chinese ? L"倒计时：" : L"Countdown: ") + DetailedCountdown(expiry)
                };
                for (int j = 0; j < 4; ++j)
                {
                    RECT line{ card.left + MulDiv(8, g_popup_dpi, 96), card.top + j * line_height,
                        card.right - MulDiv(8, g_popup_dpi, 96), card.top + (j + 1) * line_height };
                    DrawText(dc, line, labels[j], RGB(60, 64, 70));
                }
            }
            SelectClipRgn(dc, nullptr);
            SelectObject(dc, old_font); DeleteObject(body_font);

            SCROLLINFO scroll{ sizeof(SCROLLINFO), SIF_RANGE | SIF_PAGE | SIF_POS };
            scroll.nMin = 0;
            scroll.nMax = (std::max)(static_cast<int>(g_popup_lines.size()), 2 + static_cast<int>(snapshot.reset_credit_expiries.size()) * 5) - 1;
            scroll.nPage = static_cast<UINT>((rect.bottom - content_top - 8) / line_height);
            scroll.nPos = g_popup_scroll;
            SetScrollInfo(hwnd, SB_VERT, &scroll, TRUE);
            EndPaint(hwnd, &paint);
            return 0;
        }
        case WM_VSCROLL:
        {
            switch (LOWORD(wParam))
            {
            case SB_LINEUP: g_popup_scroll = (std::max)(0, g_popup_scroll - 1); break;
            case SB_LINEDOWN: ++g_popup_scroll; break;
            case SB_PAGEUP: g_popup_scroll = (std::max)(0, g_popup_scroll - 8); break;
            case SB_PAGEDOWN: g_popup_scroll += 8; break;
            case SB_THUMBPOSITION:
            case SB_THUMBTRACK:
            {
                SCROLLINFO info{ sizeof(SCROLLINFO), SIF_TRACKPOS };
                GetScrollInfo(hwnd, SB_VERT, &info);
                g_popup_scroll = info.nTrackPos;
                break;
            }
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONUP:
            if (PluginAppButton::Click(hwnd, g_popup_dpi, lParam, true)) return 0;
        {
            RECT rect{}; GetClientRect(hwnd, &rect);
            const int side = MulDiv(32, g_popup_dpi, 96);
            if (static_cast<short>(LOWORD(lParam)) >= rect.right - side && static_cast<short>(HIWORD(lParam)) < side)
                ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
        case WM_CLOSE:
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        case WM_DESTROY:
            g_tooltip_guard.Release();
            KillTimer(hwnd, 1);
            KillTimer(hwnd, 2);
            if (g_detail_popup == hwnd) g_detail_popup = nullptr;
            g_popup_tracking_mouse = false;
            return 0;
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    void ShowDetailPopup(HWND owner)
    {
        if (owner == nullptr || !IsWindow(owner)) return;
        if (g_detail_popup && IsWindowVisible(g_detail_popup))
        {
            ShowWindow(g_detail_popup, SW_HIDE);
            return;
        }
        GetCursorPos(&g_popup_anchor);
        g_popup_opened = GetTickCount64();
        RefreshPopupLines();
        if (!g_detail_popup)
        {
            if (!g_popup_instance)
                GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCWSTR>(reinterpret_cast<ULONG_PTR>(&DetailPopupProc)), &g_popup_instance);
            if (!g_popup_instance) g_popup_instance = GetModuleHandleW(nullptr);
            WNDCLASSEXW wc{ sizeof(WNDCLASSEXW) };
            wc.lpfnWndProc = DetailPopupProc;
            wc.hInstance = g_popup_instance;
            wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            wc.hbrBackground = nullptr;
            wc.lpszClassName = kDetailPopupClass;
            if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;

            RECT owner_rect{};
            GetWindowRect(owner, &owner_rect);
            HMONITOR monitor = MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST);
            MONITORINFO monitor_info{ sizeof(MONITORINFO) };
            GetMonitorInfoW(monitor, &monitor_info);
            g_popup_dpi = static_cast<int>(GetDpiForWindow(owner));
            if (g_popup_dpi <= 0) g_popup_dpi = 96;
            const int width = (std::min)(MulDiv(720, g_popup_dpi, 96), static_cast<int>(monitor_info.rcWork.right - monitor_info.rcWork.left));
            const int height = (std::min)(MulDiv(560, g_popup_dpi, 96), static_cast<int>(monitor_info.rcWork.bottom - monitor_info.rcWork.top));
            int left = owner_rect.left;
            int top = owner_rect.top - height - 4;
            if (top < monitor_info.rcWork.top) top = owner_rect.bottom + 4;
            const int work_left = static_cast<int>(monitor_info.rcWork.left);
            const int work_top = static_cast<int>(monitor_info.rcWork.top);
            const int work_right = static_cast<int>(monitor_info.rcWork.right);
            const int work_bottom = static_cast<int>(monitor_info.rcWork.bottom);
            left = (std::clamp)(left, work_left, (std::max)(work_left, work_right - width));
            top = (std::clamp)(top, work_top, (std::max)(work_top, work_bottom - height));

            g_detail_popup = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                kDetailPopupClass, L"Codex usage", WS_POPUP | WS_BORDER | WS_VSCROLL,
                left, top, width, height, owner, nullptr, wc.hInstance, nullptr);
            if (!g_detail_popup) return;
            SetTimer(g_detail_popup, 1, 1000, nullptr);
            SetTimer(g_detail_popup, 2, 100, nullptr);
        }
        SetWindowPos(g_detail_popup, HWND_TOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        ShowWindow(g_detail_popup, SW_SHOWNOACTIVATE);
        InvalidateRect(g_detail_popup, nullptr, FALSE);
    }

    void DrawRow(HDC dc, const CodexSnapshot& snapshot, bool weekly, int x, int y, int w, int h, bool dark)
    {
        const auto options = CCodexUsagePlugin::Instance().Options();
        if (options.theme != 0) dark = options.theme == 2;
        const bool available = weekly ? snapshot.has_weekly : snapshot.has_session;
        const double used = weekly ? snapshot.weekly_used : snapshot.session_used;
        const long long reset = weekly ? snapshot.weekly_reset : snapshot.session_reset;
        TEXTMETRICW metrics{}; GetTextMetricsW(dc, &metrics);
        const auto sc = [&](int n) { return MulDiv(n, (std::max)(16L, metrics.tmHeight), 16); };
        const int left = x + sc(2);
        const int row_height = h;
        const COLORREF foreground = dark ? RGB(235, 238, 242) : RGB(45, 49, 54);
        const double remaining = available ? std::clamp(100.0 - used, 0.0, 100.0) : 0.0;
        // Match codex-usage-monitor: inclusive 20% red and 50% amber limits.
        const COLORREF filled = remaining <= 20.0
            ? (dark ? RGB(242, 139, 130) : RGB(197, 34, 31))
            : remaining <= 50.0
                ? (dark ? RGB(253, 214, 99) : RGB(227, 116, 0))
                : (dark ? RGB(129, 201, 149) : RGB(24, 128, 56));
        const COLORREF empty = dark ? RGB(82, 91, 101) : RGB(194, 202, 211);
        SIZE short_label{}, long_label{};
        GetTextExtentPoint32W(dc, L"5h", 2, &short_label);
        GetTextExtentPoint32W(dc, L"7d", 2, &long_label);
        const int label_width = (std::max)(short_label.cx, long_label.cx);
        RECT label{ left, y, left + label_width, y + row_height };
        DrawText(dc, label, weekly ? L"7d" : L"5h", foreground, DT_RIGHT);

        const int bar_x = label.right + sc(8);
        const int bar_width = sc(89); // Ten narrow 8px cells with nine 1px gaps.
        const int gap = options.bar_style == 0 ? sc(1) : 0;
        const int segments = options.bar_style == 0 ? 10 : 1;
        const int segment_width = (bar_width - (segments - 1) * gap) / segments;
        const double filled_segments = remaining * segments / 100.0;
        const int time_gap = 1;
        const int time_height = (std::max)(1, sc(2));
        const int row_gap = (std::max)(3, sc(3));
        const int bar_height = (std::max)(2, (std::min)(sc(13), h - time_gap - time_height - row_gap));
        const int vertical_offset = (std::max)(2, sc(1)) + 1;
        const int top = y + vertical_offset + (std::max)(0, (row_height - bar_height - time_gap - time_height - row_gap) / 2);
        for (int i = 0; i < segments; ++i)
        {
            RECT bar{ bar_x + i * (segment_width + gap), top,
                bar_x + i * (segment_width + gap) + segment_width, top + bar_height };
            HBRUSH brush = CreateSolidBrush(empty);
            FillRect(dc, &bar, brush);
            DeleteObject(brush);
            // Each cell represents 10%. As quota is used, its colored fill recedes from top to bottom.
            const double covered = available ? std::clamp(filled_segments - i, 0.0, 1.0) : 0.0;
            const int fill_height = static_cast<int>(std::lround(bar_height * covered));
            if (fill_height > 0)
            {
                RECT fill = options.bar_style == 0
                    ? RECT{ bar.left, bar.bottom - fill_height, bar.right, bar.bottom }
                    : RECT{ bar.left, bar.top, bar.left + static_cast<int>(std::lround(segment_width * covered)), bar.bottom };
                brush = CreateSolidBrush(filled);
                FillRect(dc, &fill, brush);
                DeleteObject(brush);
            }
        }
        const int drawn_width = segments * (segment_width + gap) - gap;
        if (options.show_time_bar && available && reset > 0)
        {
            const double duration = weekly ? 7.0 * 86400 : 5.0 * 3600;
            const long long now = static_cast<long long>(time(nullptr));
            const double ratio = std::clamp((reset - now) / duration, 0.0, 1.0);
            RECT time_bar{ bar_x, top + bar_height + time_gap, bar_x + static_cast<int>(std::lround(drawn_width * ratio)), top + bar_height + time_gap + time_height };
            HBRUSH work_brush = CreateSolidBrush(dark ? RGB(138, 144, 153) : RGB(107, 114, 128));
            HBRUSH rest_brush = CreateSolidBrush(dark ? RGB(110, 118, 130) : RGB(151, 160, 174));
            const CodexTimeBar::Schedule schedule{ options.rest_schedule, options.morning_start, options.morning_end,
                options.afternoon_start, options.afternoon_end };
            const auto markers = CodexTimeBar::MarkerPixels(now, reset, weekly, drawn_width, snapshot.calendar, 0, schedule);
            // Leave gaps unpainted so the host's existing background remains visible.
            for (int pixel = 0; pixel < time_bar.right - time_bar.left; ++pixel)
            {
                if (std::binary_search(markers.begin(), markers.end(), pixel)) continue;
                // Match the reverse time direction of the shrinking bar; sample each pixel's center.
                const long long timestamp = (std::max)(now, reset - static_cast<long long>((pixel + 0.5) * duration / drawn_width));
                RECT section{ bar_x + pixel, time_bar.top, bar_x + pixel + 1, time_bar.bottom };
                FillRect(dc, &section, CodexTimeBar::IsWorkingTime(timestamp, weekly, snapshot.calendar, schedule) ? work_brush : rest_brush);
            }
            DeleteObject(work_brush);
            DeleteObject(rest_brush);
        }
        const int value_x = bar_x + drawn_width + sc(8);
        const std::wstring summary = QuotaSummary(snapshot, weekly);
        SIZE size{}; GetTextExtentPoint32W(dc, summary.c_str(), static_cast<int>(summary.size()), &size);
        RECT summary_rect{ value_x, y, value_x + size.cx + sc(1), y + row_height };
        DrawText(dc, summary_rect, summary, foreground);
        const std::wstring other_summary = options.show_session && options.show_weekly ? QuotaSummary(snapshot, !weekly) : summary;
        SIZE other_size{}; GetTextExtentPoint32W(dc, other_summary.c_str(), static_cast<int>(other_summary.size()), &other_size);
        const int badge_x = value_x + (std::max)(size.cx, other_size.cx) + sc(6);
        const int count = options.show_cards ? (std::min)(8, (std::max)(0, snapshot.reset_credits)) : 0;
        const bool double_row = options.show_session && options.show_weekly;
        for (int i = double_row && weekly ? 1 : 0; i < count; i += double_row ? 2 : 1)
        {
            const int card_size = (std::max)(10, (std::min)(sc(14), row_height - sc(3)));
            const int bx = badge_x + (double_row ? i / 2 : i) * (card_size + sc(2));
            if (bx + card_size > x + w) break;
            RECT badge{ bx, y + (row_height - card_size) / 2, bx + card_size, y + (row_height - card_size) / 2 + card_size };
            const long long expiry = static_cast<size_t>(i) < snapshot.reset_credit_expiries.size() ? snapshot.reset_credit_expiries[i] : 0;
            const long long seconds = expiry - static_cast<long long>(time(nullptr));
            const COLORREF color = expiry > 0 && seconds < 3600 ? RGB(198, 40, 40) : expiry > 0 && seconds < 86400 ? RGB(196, 95, 0) : foreground;
            HBRUSH border = CreateSolidBrush(color);
            FrameRect(dc, &badge, border);
            DeleteObject(border);
            const std::wstring text = ResetBadge(expiry);
            HFONT font = CreateFontW(-sc(text.size() > 2 ? 8 : 9), 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            HGDIOBJ old = SelectObject(dc, font);
            const int saved_dc = SaveDC(dc);
            SetTextColor(dc, color);
            SetBkMode(dc, TRANSPARENT);
            // Center the visible glyphs, excluding the font's leading and side bearings.
            MAT2 identity{ { 0, 1 }, { 0, 0 }, { 0, 0 }, { 0, 1 } };
            RECT ink{};
            int advance = 0;
            bool measured = true;
            bool first_glyph = true;
            for (wchar_t character : text)
            {
                GLYPHMETRICS glyph{};
                if (GetGlyphOutlineW(dc, character, GGO_METRICS, &glyph, 0, nullptr, &identity) == GDI_ERROR)
                {
                    measured = false;
                    break;
                }
                RECT bounds{ advance + glyph.gmptGlyphOrigin.x, -glyph.gmptGlyphOrigin.y,
                    advance + glyph.gmptGlyphOrigin.x + static_cast<int>(glyph.gmBlackBoxX),
                    -glyph.gmptGlyphOrigin.y + static_cast<int>(glyph.gmBlackBoxY) };
                if (first_glyph) { ink = bounds; first_glyph = false; }
                else { UnionRect(&ink, &ink, &bounds); }
                advance += glyph.gmCellIncX;
            }
            if (measured && !first_glyph)
            {
                SetTextAlign(dc, TA_LEFT | TA_BASELINE | TA_NOUPDATECP);
                TextOutW(dc, (badge.left + badge.right - ink.left - ink.right) / 2,
                    (badge.top + badge.bottom - ink.top - ink.bottom) / 2,
                    text.c_str(), static_cast<int>(text.size()));
            }
            else
                ::DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &badge, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            RestoreDC(dc, saved_dc);
            SelectObject(dc, old); DeleteObject(font);
        }
    }
}

CCodexUsagePlugin& CCodexUsagePlugin::Instance()
{
    // Keep the process-wide plugin instance alive until process exit. Older
    // TrafficMonitor API versions do not notify plugins before unloading.
    static CCodexUsagePlugin* instance = new CCodexUsagePlugin();
    return *instance;
}

IPluginItem* CCodexUsagePlugin::GetItem(int index)
{
    return index == 0 ? &m_item : nullptr;
}

void CCodexUsagePlugin::DataRequired()
{
    // The independent worker owns network I/O; keep TrafficMonitor's sampling callback fast.
}

const wchar_t* CCodexUsagePlugin::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME: return L"Codex Usage";
    case TMI_DESCRIPTION: return L"Codex account limits and local token statistics";
    case TMI_AUTHOR: return L"Codex Usage Monitor contributors";
    case TMI_VERSION: return L"0.1.0";
    case TMI_URL: return L"https://github.com/wang-yichun/codex-usage-monitor";
    default: return L"";
    }
}

void CCodexUsagePlugin::OnInitialize(ITrafficMonitor* app)
{
    m_app = app;
    if (app != nullptr)
    {
        const LANGID language = app->GetLanguageId();
        // Automatic host language is neutral (0); use Windows' UI language in that case.
        m_is_chinese = PRIMARYLANGID(language == 0 ? GetUserDefaultUILanguage() : language) == LANG_CHINESE;
    }
    if (app && app->GetPluginConfigDir())
    {
        m_config_path = (std::filesystem::path(app->GetPluginConfigDir()) / L"CodexUsage.ini").wstring();
        auto read = [&](const wchar_t* key, int fallback) { return static_cast<int>(GetPrivateProfileIntW(L"Options", key, fallback, m_config_path.c_str())); };
        CodexOptions options;
        options.poll_seconds = read(L"PollSeconds", 60);
        if (options.poll_seconds != 60 && options.poll_seconds != 300 && options.poll_seconds != 900 && options.poll_seconds != 3600) options.poll_seconds = 60;
        options.theme = (std::clamp)(read(L"Theme", 0), 0, 2);
        options.bar_style = (std::clamp)(read(L"BarStyle", 0), 0, 1);
        options.text_style = (std::clamp)(read(L"TextStyle", 0), 0, 1);
        options.show_session = read(L"ShowSession", 1) != 0;
        options.show_weekly = read(L"ShowWeekly", 1) != 0;
        options.show_cards = read(L"ShowCards", 1) != 0;
        options.show_time_bar = read(L"ShowTimeBar", 1) != 0;
        options.rest_schedule = (std::clamp)(read(L"RestSchedule", 0), 0, 3);
        const auto valid_minute = [](int minute) { return minute >= 0 && minute < 24 * 60; };
        options.morning_start = read(L"MorningStart", 9 * 60 + 30);
        options.morning_end = read(L"MorningEnd", 12 * 60);
        options.afternoon_start = read(L"AfternoonStart", 13 * 60 + 30);
        options.afternoon_end = read(L"AfternoonEnd", 18 * 60 + 30);
        if (!valid_minute(options.morning_start) || !valid_minute(options.morning_end) ||
            !valid_minute(options.afternoon_start) || !valid_minute(options.afternoon_end) ||
            options.morning_start >= options.morning_end || options.morning_end > options.afternoon_start ||
            options.afternoon_start >= options.afternoon_end)
        {
            options.morning_start = 9 * 60 + 30; options.morning_end = 12 * 60;
            options.afternoon_start = 13 * 60 + 30; options.afternoon_end = 18 * 60 + 30;
        }
        if (!options.show_session && !options.show_weekly) options.show_session = true;
        std::lock_guard<std::mutex> lock(m_options_mutex);
        m_options = options;
    }
    m_stop.store(false);
    m_refresh_requested.store(false);
    if (!m_worker.joinable()) m_worker = std::thread(&CCodexUsagePlugin::PollLoop, this);
}

void CCodexUsagePlugin::OnShutdown()
{
    m_stop.store(true);
    m_worker_cv.notify_all();
    if (m_worker.joinable()) m_worker.join();
    DestroyDetailPopup();
    m_app = nullptr;
}

void CCodexUsagePlugin::PollLoop()
{
    while (!m_stop.load())
    {
        RefreshSnapshot();
        std::unique_lock<std::mutex> lock(m_worker_mutex);
        m_worker_cv.wait_for(lock, std::chrono::seconds(Options().poll_seconds), [this] {
            return m_stop.load() || m_refresh_requested.exchange(false);
        });
        if (m_stop.load()) break;
    }
}

void CCodexUsagePlugin::RefreshSnapshot()
{
    CodexSnapshot next;
    // Load beside this DLL, independently of the host's current working directory.
    // The worker refreshes the calendar; painting only reads the cached snapshot.
    HMODULE module{};
    wchar_t module_path[32768]{};
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&TMPluginGetInstance), &module) &&
        GetModuleFileNameW(module, module_path, static_cast<DWORD>(std::size(module_path))) > 0)
    {
        const auto folder = std::filesystem::path(module_path).parent_path() / L"calendar";
        std::error_code error;
        for (std::filesystem::directory_iterator it(folder, error), end; !error && it != end; it.increment(error))
        {
            if (!it->is_regular_file(error) || it->path().extension() != L".txt") continue;
            const std::string year = it->path().stem().u8string();
            if (year.size() != 4 || !std::all_of(year.begin(), year.end(), [](char ch) { return ch >= '0' && ch <= '9'; })) continue;
            std::ifstream file(it->path());
            if (file) next.calendar.ReadYear(file, std::stoi(year));
        }
    }
    Credentials credentials;
    if (!ReadCredentials(credentials))
    {
        next.status = m_is_chinese ? L"未找到 Codex 登录凭据" : L"Codex sign-in credentials not found";
    }
    else
    {
        std::wstring body;
        JsonValue root;
        if (HttpGet(kUsagePath, credentials, body) && ParseJson(body, root))
        {
            ParseUsage(root, next, m_is_chinese);
            next.status = (next.has_session || next.has_weekly) ? (m_is_chinese ? L"已连接" : L"Connected")
                : (m_is_chinese ? L"账号未返回额度数据" : L"No quota data returned");
            std::wstring credit_body;
            JsonValue credit_root;
            if (HttpGet(kCreditsPath, credentials, credit_body) && ParseJson(credit_body, credit_root))
            {
                const JsonValue* count = credit_root.Get(L"available_count");
                if (count && count->type == JsonValue::Type::Number)
                    next.reset_credits = static_cast<int>(count->number);
                const JsonValue* credit_items = credit_root.Get(L"credits");
                if (credit_items && credit_items->type == JsonValue::Type::Array)
                {
                    std::wostringstream details;
                    details << next.account_details << L"\r\n" << (m_is_chinese ? L"\r\n重置额度明细：\r\n" : L"\r\nReset credit details:\r\n");
                    int shown = 0;
                    for (const auto& item : credit_items->array)
                    {
                        const JsonValue* status = item.Get(L"status");
                        if (status && status->String() != L"available") continue;
                        const JsonValue* expires = item.Get(L"expires_at");
                        next.reset_credit_expiries.push_back(CreditExpiry(expires));
                        details << L"  " << (expires ? expires->String(m_is_chinese ? L"未知" : L"expiry unknown") : (m_is_chinese ? L"未知" : L"expiry unknown")) << L"\r\n";
                        if (++shown >= 8) break;
                    }
                    next.account_details = details.str();
                    std::sort(next.reset_credit_expiries.begin(), next.reset_credit_expiries.end(), [](long long a, long long b) {
                        return a != 0 && (b == 0 || a < b);
                    });
                }
            }
        }
        else
        {
            next.status = m_is_chinese ? L"Codex 接口请求失败" : L"Codex request failed";
        }
        if (!credentials.access_token.empty())
            SecureZeroMemory(credentials.access_token.data(), credentials.access_token.size() * sizeof(wchar_t));
    }

    ScanSessions(next);
    std::wostringstream details;
    if (m_is_chinese)
        details << next.account_details << L"\r\n今日 Codex Token 统计\r\n"
            << L"输入: " << FormatCount(next.input_tokens) << L"\r\n"
            << L"缓存输入: " << FormatCount(next.cached_input_tokens) << L"\r\n"
            << L"输出: " << FormatCount(next.output_tokens) << L"\r\n"
            << L"推理: " << FormatCount(next.reasoning_tokens) << L"\r\n"
            << L"总计: " << FormatTotalCount(next.total_tokens, true) << L"\r\n"
            << L"会话数: " << next.session_count << L"\r\n"
            << L"无法读取的日志: " << next.unreadable_sessions << L"\r\n"
            << L"状态: " << next.status;
    else
        details << next.account_details << L"\r\nToday's local Codex tokens\r\n"
            << L"Input: " << FormatCount(next.input_tokens) << L"\r\n"
            << L"Cached input: " << FormatCount(next.cached_input_tokens) << L"\r\n"
            << L"Output: " << FormatCount(next.output_tokens) << L"\r\n"
            << L"Reasoning: " << FormatCount(next.reasoning_tokens) << L"\r\n"
            << L"Total: " << FormatTotalCount(next.total_tokens, false) << L"\r\n"
            << L"Sessions: " << next.session_count << L"\r\n"
            << L"Unreadable logs: " << next.unreadable_sessions << L"\r\n"
            << L"Status: " << next.status;
    next.account_details = details.str();

    {
        std::lock_guard<std::mutex> lock(m_snapshot_mutex);
        m_snapshot = std::move(next);
    }
}

const wchar_t* CCodexUsagePlugin::GetTooltipInfo()
{
    thread_local std::wstring result;
    std::lock_guard<std::mutex> lock(m_snapshot_mutex);
    result = m_snapshot.status;
    const auto options = Options();
    if (options.show_session && m_snapshot.has_session)
    {
        result += m_is_chinese ? L" | 5小时余量 " : L" | 5h remaining ";
        result += std::to_wstring(static_cast<int>(std::lround((std::clamp)(100.0 - m_snapshot.session_used, 0.0, 100.0))));
        result += L"%";
    }
    if (options.show_weekly && m_snapshot.has_weekly)
    {
        result += m_is_chinese ? L" | 7天余量 " : L" | 7d remaining ";
        result += std::to_wstring(static_cast<int>(std::lround((std::clamp)(100.0 - m_snapshot.weekly_used, 0.0, 100.0))));
        result += L"%";
    }
    return result.c_str();
}

const wchar_t* CCodexUsagePlugin::GetCommandName(int command_index)
{
    if (m_is_chinese)
    {
        if (command_index == 0) return L"显示 Codex 用量与 Token 详情";
        if (command_index == 1) return L"立即刷新 Codex 用量";
    }
    else
    {
        if (command_index == 0) return L"Show Codex usage and token details";
        if (command_index == 1) return L"Refresh Codex usage now";
    }
    return nullptr;
}

void CCodexUsagePlugin::OnPluginCommand(int command_index, void* hWnd, void*)
{
    if (command_index == 0)
    {
        ShowDetailPopup(static_cast<HWND>(hWnd));
    }
    else if (command_index == 1)
    {
        RequestRefresh();
    }
}

void CCodexUsagePlugin::RequestRefresh()
{
    m_refresh_requested.store(true);
    m_worker_cv.notify_all();
}


CodexOptions CCodexUsagePlugin::Options() const
{
    std::lock_guard<std::mutex> lock(m_options_mutex);
    return m_options;
}

namespace
{
    std::wstring OptionsSection(const CodexOptions& options)
    {
        std::wstring section;
        const auto add = [&](const wchar_t* key, int value) {
            section += std::wstring(key) + L"=" + std::to_wstring(value);
            section.push_back(L'\0');
        };
        add(L"PollSeconds", options.poll_seconds);
        add(L"Theme", options.theme);
        add(L"BarStyle", options.bar_style);
        add(L"TextStyle", options.text_style);
        add(L"ShowSession", options.show_session);
        add(L"ShowWeekly", options.show_weekly);
        add(L"ShowCards", options.show_cards);
        add(L"ShowTimeBar", options.show_time_bar);
        add(L"RestSchedule", options.rest_schedule);
        add(L"MorningStart", options.morning_start);
        add(L"MorningEnd", options.morning_end);
        add(L"AfternoonStart", options.afternoon_start);
        add(L"AfternoonEnd", options.afternoon_end);
        section.push_back(L'\0');
        return section;
    }

    std::wstring FormatClock(int minute)
    {
        wchar_t text[6]{};
        swprintf_s(text, L"%02d:%02d", minute / 60, minute % 60);
        return text;
    }

    bool ParseClock(HWND dialog, int id, int& minute)
    {
        wchar_t text[16]{};
        GetDlgItemTextW(dialog, id, text, static_cast<int>(std::size(text)));
        if (wcslen(text) != 5 || text[2] != L':') return false;
        for (int i : {0, 1, 3, 4}) if (text[i] < L'0' || text[i] > L'9') return false;
        const int hour = (text[0] - L'0') * 10 + text[1] - L'0';
        const int mins = (text[3] - L'0') * 10 + text[4] - L'0';
        if (hour > 23 || mins > 59) return false;
        minute = hour * 60 + mins;
        return true;
    }

    INT_PTR CALLBACK OptionsDialogProc(HWND dialog, UINT message, WPARAM wp, LPARAM)
    {
        const bool chinese = CCodexUsagePlugin::Instance().IsChinese();
        if (message == WM_INITDIALOG)
        {
            const auto options = CCodexUsagePlugin::Instance().Options();
            const auto combo = [&](int id, std::initializer_list<const wchar_t*> labels, int selected) {
                for (const auto label : labels) SendDlgItemMessageW(dialog, id, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
                SendDlgItemMessageW(dialog, id, CB_SETCURSEL, selected, 0);
            };
            combo(IDC_POLL, chinese ? std::initializer_list<const wchar_t*>{L"1 分钟", L"5 分钟", L"15 分钟", L"1 小时"}
                : std::initializer_list<const wchar_t*>{L"1 minute", L"5 minutes", L"15 minutes", L"1 hour"},
                options.poll_seconds == 300 ? 1 : options.poll_seconds == 900 ? 2 : options.poll_seconds == 3600 ? 3 : 0);
            combo(IDC_THEME, chinese ? std::initializer_list<const wchar_t*>{L"跟随 TrafficMonitor", L"浅色", L"深色"}
                : std::initializer_list<const wchar_t*>{L"Follow TrafficMonitor", L"Light", L"Dark"}, options.theme);
            combo(IDC_BAR, chinese ? std::initializer_list<const wchar_t*>{L"分段", L"连续"}
                : std::initializer_list<const wchar_t*>{L"Segmented", L"Continuous"}, options.bar_style);
            combo(IDC_TEXT, chinese ? std::initializer_list<const wchar_t*>{L"简洁", L"详细"}
                : std::initializer_list<const wchar_t*>{L"Compact", L"Detailed"}, options.text_style);
            combo(IDC_REST_SCHEDULE, chinese
                ? std::initializer_list<const wchar_t*>{L"双休（周六、周日休）", L"单休（周日休）", L"单双轮休 A（A 周双休）", L"单双轮休 B（B 周双休）"}
                : std::initializer_list<const wchar_t*>{L"Two days off (Sat/Sun)", L"One day off (Sun)", L"Alternating A (A week: Sat/Sun off)", L"Alternating B (B week: Sat/Sun off)"}, options.rest_schedule);
            SetDlgItemTextW(dialog, IDC_MORNING_START, FormatClock(options.morning_start).c_str());
            SetDlgItemTextW(dialog, IDC_MORNING_END, FormatClock(options.morning_end).c_str());
            SetDlgItemTextW(dialog, IDC_AFTERNOON_START, FormatClock(options.afternoon_start).c_str());
            SetDlgItemTextW(dialog, IDC_AFTERNOON_END, FormatClock(options.afternoon_end).c_str());
            CheckDlgButton(dialog, IDC_SESSION, options.show_session ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(dialog, IDC_WEEKLY, options.show_weekly ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(dialog, IDC_CARDS, options.show_cards ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(dialog, IDC_TIME_BAR, options.show_time_bar ? BST_CHECKED : BST_UNCHECKED);
            if (chinese)
            {
                SetWindowTextW(dialog, L"Codex Usage 插件选项");
                const std::pair<int, const wchar_t*> labels[] = {
                    {IDC_POLL_LABEL, L"刷新频率"}, {IDC_THEME_LABEL, L"主题"}, {IDC_BAR_LABEL, L"额度进度条"},
                    {IDC_TEXT_LABEL, L"文字样式"}, {IDC_SESSION, L"显示 5 小时额度"}, {IDC_WEEKLY, L"显示 7 天额度"},
                    {IDC_CARDS, L"显示重置卡片"}, {IDC_TIME_BAR, L"显示时间进度条"},
                    {IDC_SCHEDULE_LABEL, L"每周休息安排"}, {IDC_MORNING_LABEL, L"上午时段"},
                    {IDC_AFTERNOON_LABEL, L"下午时段"}, {IDC_MORNING_TO, L"至"}, {IDC_AFTERNOON_TO, L"至"},
                    {IDC_HINT, L"窗口位置和背景由 TrafficMonitor 控制。至少保留一个额度窗口。"},
                    {IDOK, L"确定"}, {IDCANCEL, L"取消"}
                };
                for (const auto& label : labels) SetDlgItemTextW(dialog, label.first, label.second);
            }
            const bool time_bar_enabled = options.show_time_bar;
            for (int id : { IDC_SCHEDULE_LABEL, IDC_REST_SCHEDULE, IDC_MORNING_LABEL, IDC_MORNING_START,
                IDC_MORNING_TO, IDC_MORNING_END, IDC_AFTERNOON_LABEL, IDC_AFTERNOON_START, IDC_AFTERNOON_TO, IDC_AFTERNOON_END })
                EnableWindow(GetDlgItem(dialog, id), time_bar_enabled);
            RECT bounds{}, owner{};
            GetWindowRect(dialog, &bounds);
            GetWindowRect(GetParent(dialog) ? GetParent(dialog) : GetDesktopWindow(), &owner);
            SetWindowPos(dialog, nullptr, owner.left + (owner.right - owner.left - bounds.right + bounds.left) / 2,
                owner.top + (owner.bottom - owner.top - bounds.bottom + bounds.top) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            return TRUE;
        }
        if (message == WM_COMMAND)
        {
            if (LOWORD(wp) == IDC_TIME_BAR && HIWORD(wp) == BN_CLICKED)
            {
                const bool enabled = IsDlgButtonChecked(dialog, IDC_TIME_BAR) == BST_CHECKED;
                for (int id : { IDC_SCHEDULE_LABEL, IDC_REST_SCHEDULE, IDC_MORNING_LABEL, IDC_MORNING_START,
                    IDC_MORNING_TO, IDC_MORNING_END, IDC_AFTERNOON_LABEL, IDC_AFTERNOON_START, IDC_AFTERNOON_TO, IDC_AFTERNOON_END })
                    EnableWindow(GetDlgItem(dialog, id), enabled);
                return TRUE;
            }
            if (LOWORD(wp) == IDCANCEL) { EndDialog(dialog, IDCANCEL); return TRUE; }
            if (LOWORD(wp) == IDOK)
            {
                auto options = CCodexUsagePlugin::Instance().Options();
                const int intervals[] = {60, 300, 900, 3600};
                options.poll_seconds = intervals[(std::clamp)(static_cast<int>(SendDlgItemMessageW(dialog, IDC_POLL, CB_GETCURSEL, 0, 0)), 0, 3)];
                options.theme = static_cast<int>(SendDlgItemMessageW(dialog, IDC_THEME, CB_GETCURSEL, 0, 0));
                options.bar_style = static_cast<int>(SendDlgItemMessageW(dialog, IDC_BAR, CB_GETCURSEL, 0, 0));
                options.text_style = static_cast<int>(SendDlgItemMessageW(dialog, IDC_TEXT, CB_GETCURSEL, 0, 0));
                options.show_session = IsDlgButtonChecked(dialog, IDC_SESSION) == BST_CHECKED;
                options.show_weekly = IsDlgButtonChecked(dialog, IDC_WEEKLY) == BST_CHECKED;
                options.show_cards = IsDlgButtonChecked(dialog, IDC_CARDS) == BST_CHECKED;
                options.show_time_bar = IsDlgButtonChecked(dialog, IDC_TIME_BAR) == BST_CHECKED;
                options.rest_schedule = static_cast<int>(SendDlgItemMessageW(dialog, IDC_REST_SCHEDULE, CB_GETCURSEL, 0, 0));
                if (options.rest_schedule < 0 || options.rest_schedule > 3 ||
                    !ParseClock(dialog, IDC_MORNING_START, options.morning_start) ||
                    !ParseClock(dialog, IDC_MORNING_END, options.morning_end) ||
                    !ParseClock(dialog, IDC_AFTERNOON_START, options.afternoon_start) ||
                    !ParseClock(dialog, IDC_AFTERNOON_END, options.afternoon_end))
                {
                    MessageBoxW(dialog, chinese ? L"请输入有效时间，格式为 HH:mm。" : L"Enter valid times in HH:mm format.", L"Codex Usage", MB_OK | MB_ICONINFORMATION);
                    return TRUE;
                }
                if (options.morning_start >= options.morning_end || options.morning_end > options.afternoon_start ||
                    options.afternoon_start >= options.afternoon_end)
                {
                    MessageBoxW(dialog, chinese ? L"请确保上午开始 < 上午结束 ≤ 下午开始 < 下午结束。" : L"Times must satisfy morning start < morning end ≤ afternoon start < afternoon end.", L"Codex Usage", MB_OK | MB_ICONINFORMATION);
                    return TRUE;
                }
                if (!options.show_session && !options.show_weekly)
                {
                    MessageBoxW(dialog, chinese ? L"请至少选择一个额度窗口。" : L"Select at least one quota window.", L"Codex Usage", MB_OK | MB_ICONINFORMATION);
                    return TRUE;
                }
                if (OptionsSection(options) == OptionsSection(CCodexUsagePlugin::Instance().Options()))
                    EndDialog(dialog, IDCANCEL);
                else if (CCodexUsagePlugin::Instance().SaveOptions(options)) EndDialog(dialog, IDOK);
                else MessageBoxW(dialog, chinese ? L"无法保存配置，请检查配置目录是否可写。" : L"Unable to save settings. Check the configuration directory is writable.", L"Codex Usage", MB_OK | MB_ICONERROR);
                return TRUE;
            }
        }
        if (message == WM_CLOSE) { EndDialog(dialog, IDCANCEL); return TRUE; }
        return FALSE;
    }
}

bool CCodexUsagePlugin::SaveOptions(const CodexOptions& options)
{
    if (m_config_path.empty()) return false;
    std::error_code error;
    std::filesystem::create_directories(std::filesystem::path(m_config_path).parent_path(), error);
    if (error) return false;
    const auto section = OptionsSection(options);
    if (!WritePrivateProfileSectionW(L"Options", section.c_str(), m_config_path.c_str())) return false;
    {
        std::lock_guard<std::mutex> lock(m_options_mutex);
        m_options = options;
    }
    DestroyDetailPopup();
    RequestRefresh();
    return true;
}

ITMPlugin::OptionReturn CCodexUsagePlugin::ShowOptionsDialog(void* hParent)
{
    HMODULE module{};
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&TMPluginGetInstance), &module);
    const auto result = DialogBoxParamW(module, MAKEINTRESOURCEW(IDD_CODEX_OPTIONS), static_cast<HWND>(hParent), OptionsDialogProc, 0);
    if (result == -1) MessageBoxW(static_cast<HWND>(hParent), IsChinese() ? L"无法打开插件选项。" : L"Unable to open plugin options.", L"Codex Usage", MB_OK | MB_ICONERROR);
    return result == IDOK ? OR_OPTION_CHANGED : OR_OPTION_UNCHANGED;
}

CodexSnapshot CCodexUsagePlugin::Snapshot() const
{
    std::lock_guard<std::mutex> lock(m_snapshot_mutex);
    return m_snapshot;
}

const wchar_t* CCodexUsageItem::GetItemName() const { return CCodexUsagePlugin::Instance().IsChinese() ? L"Codex 额度" : L"Codex quota"; }
const wchar_t* CCodexUsageItem::GetItemId() const { return L"CodexUsageQuota9"; }
const wchar_t* CCodexUsageItem::GetItemLableText() const { return L""; }
const wchar_t* CCodexUsageItem::GetItemValueText() const { return L""; }
const wchar_t* CCodexUsageItem::GetItemValueSampleText() const { return L""; }

int CCodexUsageItem::GetItemWidthEx(void* hDC) const
{
    if (!hDC) return 0;
    HDC dc = static_cast<HDC>(hDC);
    TEXTMETRICW metrics{}; GetTextMetricsW(dc, &metrics);
    const auto sc = [&](int n) { return MulDiv(n, (std::max)(16L, metrics.tmHeight), 16); };
    const auto snapshot = CCodexUsagePlugin::Instance().Snapshot();
    const auto options = CCodexUsagePlugin::Instance().Options();
    int summary_width = 0;
    for (bool weekly : { false, true })
    {
        if (weekly ? !options.show_weekly : !options.show_session) continue;
        const auto text = QuotaSummary(snapshot, weekly);
        SIZE size{}; GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
        summary_width = (std::max)(summary_width, static_cast<int>(size.cx));
    }
    const int cards = options.show_cards ? (std::min)(8, (std::max)(0, snapshot.reset_credits)) : 0;
    const int columns = options.show_session && options.show_weekly ? (cards + 1) / 2 : cards;
    SIZE short_label{}, long_label{};
    GetTextExtentPoint32W(dc, L"5h", 2, &short_label);
    GetTextExtentPoint32W(dc, L"7d", 2, &long_label);
    return sc(2 + 8 + 89 + 8 + 8 + columns * 16) + (std::max)(short_label.cx, long_label.cx) + summary_width;
}

void CCodexUsageItem::DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode)
{
    if (hDC == nullptr) return;
    const CodexSnapshot snapshot = CCodexUsagePlugin::Instance().Snapshot();
    const auto options = CCodexUsagePlugin::Instance().Options();
    const int row_height = options.show_session && options.show_weekly ? h / 2 : h;
    HDC dc = static_cast<HDC>(hDC);
    if (options.show_session) DrawRow(dc, snapshot, false, x, y, w, row_height, dark_mode);
    if (options.show_weekly) DrawRow(dc, snapshot, true, x, y + (options.show_session ? row_height : 0), w,
        options.show_session ? h - row_height : h, dark_mode);
}

int CCodexUsageItem::OnMouseEvent(MouseEventType type, int, int, void* hWnd, int)
{
    if (type == MT_LCLICKED)
    {
        ShowDetailPopup(static_cast<HWND>(hWnd));
        return 1;
    }
    return 0;
}

extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance()
{
    return &CCodexUsagePlugin::Instance();
}
