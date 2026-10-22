#include "pch.h"
#include "DataManager.h"
#include "Common.h"
#include <vector>
#include <sstream>
#include <fstream>
#include "../utilities/bass64/base64.h"
#include "../utilities/FilePathHelper.h"
#include "../utilities/Common.h"
#include "HtmlToText.h"

CDataManager CDataManager::m_instance;

CDataManager::CDataManager()
{
    //初始化DPI
    HDC hDC = ::GetDC(HWND_DESKTOP);
    m_dpi = GetDeviceCaps(hDC, LOGPIXELSY);
    ::ReleaseDC(HWND_DESKTOP, hDC);
}

CDataManager::~CDataManager()
{
    SaveConfig();
}

CDataManager& CDataManager::Instance()
{
    return m_instance;
}

static void WritePrivateProfileInt(const wchar_t* app_name, const wchar_t* key_name, int value, const wchar_t* file_path)
{
    wchar_t buff[16];
    swprintf_s(buff, L"%d", value);
    WritePrivateProfileString(app_name, key_name, buff, file_path);
}

void CDataManager::LoadConfig(const std::wstring& config_dir)
{
    //获取模块的路径
    HMODULE hModule = reinterpret_cast<HMODULE>(&__ImageBase);
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(hModule, path, MAX_PATH);
    std::wstring module_path = path;
    m_config_path = module_path;
    if (!config_dir.empty())
    {
        size_t index = module_path.find_last_of(L"\\/");
        //模块的文件名
        std::wstring module_file_name = module_path.substr(index + 1);
        m_config_path = config_dir + module_file_name;
    }
    m_config_path += L".ini";
    //TODO: 在此添加载入配置的代码
    TCHAR buff[MAX_PATH];
    GetPrivateProfileString(_T("config"), _T("file_path"), _T(""), buff, MAX_PATH, m_config_path.c_str());
    m_setting_data.file_path = buff;
    LoadTextContents(m_setting_data.file_path.c_str());

    m_setting_data.current_position = GetPrivateProfileInt(_T("config"), _T("current_position"), 0, m_config_path.c_str());
    m_setting_data.window_width = GetPrivateProfileInt(_T("config"), _T("window_width"), 180, m_config_path.c_str());
    m_setting_data.show_in_tooltips = GetPrivateProfileInt(_T("config"), _T("show_in_tooltips"), 0, m_config_path.c_str());
    m_setting_data.enable_mulit_line = GetPrivateProfileInt(_T("config"), _T("enable_mulit_line"), 0, m_config_path.c_str());
    m_setting_data.hide_when_lose_focus = GetPrivateProfileInt(_T("config"), _T("hide_when_lose_focus"), 0, m_config_path.c_str());
    m_setting_data.auto_read = GetPrivateProfileInt(_T("config"), _T("auto_read"), 0, m_config_path.c_str());
    m_setting_data.auto_read_timer_interval = GetPrivateProfileInt(_T("config"), _T("auto_read_timer_interval"), 2000, m_config_path.c_str());
    m_setting_data.auto_decode_base64 = GetPrivateProfileInt(_T("config"), _T("auto_decode_base64"), 1, m_config_path.c_str());
    m_setting_data.use_own_context_menu = GetPrivateProfileInt(_T("config"), _T("use_own_context_menu"), 1, m_config_path.c_str());
    m_setting_data.restart_at_end = GetPrivateProfileInt(_T("config"), _T("restart_at_end"), 0, m_config_path.c_str());
    m_setting_data.auto_reload_when_file_changed = GetPrivateProfileInt(_T("config"), _T("auto_reload_when_file_changed"), 0, m_config_path.c_str());
    m_setting_data.mouse_wheel_read = GetPrivateProfileInt(_T("config"), _T("mouse_wheel_read"), 0, m_config_path.c_str());
    m_setting_data.mouse_wheel_charactors = GetPrivateProfileInt(_T("config"), _T("mouse_wheel_charactors"), 3, m_config_path.c_str());
    m_setting_data.mouse_wheel_read_page = GetPrivateProfileInt(_T("config"), _T("mouse_wheel_read_page"), 0, m_config_path.c_str());

    //载入书签
    m_bookmark_mgr.LoadFromConfig(m_config_path);
}

void CDataManager::SaveConfig() const
{
    if (!m_config_path.empty())
    {
        //TODO: 在此添加保存配置的代码
        WritePrivateProfileString(_T("config"), _T("file_path"), m_setting_data.file_path.c_str(), m_config_path.c_str());
        WritePrivateProfileInt(_T("config"), _T("current_position"), m_setting_data.current_position, m_config_path.c_str());
        WritePrivateProfileInt(_T("config"), _T("window_width"), m_setting_data.window_width, m_config_path.c_str());
        WritePrivateProfileInt(_T("config"), _T("show_in_tooltips"), m_setting_data.show_in_tooltips, m_config_path.c_str());
        WritePrivateProfileInt(_T("config"), _T("enable_mulit_line"), m_setting_data.enable_mulit_line, m_config_path.c_str());
        WritePrivateProfileInt(_T("config"), _T("hide_when_lose_focus"), m_setting_data.hide_when_lose_focus, m_config_path.c_str());
        WritePrivateProfileInt(_T("config"), _T("auto_read"), m_setting_data.auto_read, m_config_path.c_str());
        WritePrivateProfileInt(_T("config"), _T("auto_read_timer_interval"), m_setting_data.auto_read_timer_interval, m_config_path.c_str());
        WritePrivateProfileInt(_T("config"), _T("auto_decode_base64"), m_setting_data.auto_decode_base64, m_config_path.c_str());
        WritePrivateProfileInt(_T("config"), _T("use_own_context_menu"), m_setting_data.use_own_context_menu, m_config_path.c_str());
        WritePrivateProfileInt(_T("config"), _T("restart_at_end"), m_setting_data.restart_at_end, m_config_path.c_str());
        WritePrivateProfileInt(_T("config"), _T("auto_reload_when_file_changed"), m_setting_data.auto_reload_when_file_changed, m_config_path.c_str());
        WritePrivateProfileInt(_T("config"), _T("mouse_wheel_read"), m_setting_data.mouse_wheel_read, m_config_path.c_str());
        WritePrivateProfileInt(_T("config"), _T("mouse_wheel_charactors"), m_setting_data.mouse_wheel_charactors, m_config_path.c_str());
        WritePrivateProfileInt(_T("config"), _T("mouse_wheel_read_page"), m_setting_data.mouse_wheel_read_page, m_config_path.c_str());

        //保存书签
        m_bookmark_mgr.SaveToConfig(m_config_path);
    }
}

const CString& CDataManager::StringRes(UINT id)
{
    auto iter = m_string_table.find(id);
    if (iter != m_string_table.end())
    {
        return iter->second;
    }
    else
    {
        AFX_MANAGE_STATE(AfxGetStaticModuleState());
        m_string_table[id].LoadString(id);
        return m_string_table[id];
    }
}

void CDataManager::DPIFromWindow(CWnd* pWnd)
{
    CWindowDC dc(pWnd);
    HDC hDC = dc.GetSafeHdc();
    m_dpi = GetDeviceCaps(hDC, LOGPIXELSY);
}

int CDataManager::DPI(int pixel)
{
    return m_dpi * pixel / 96;
}

float CDataManager::DPIF(float pixel)
{
    return m_dpi * pixel / 96;
}

int CDataManager::RDPI(int pixel)
{
    return pixel * 96 / m_dpi;
}

HICON CDataManager::GetIcon(UINT id)
{
    auto iter = m_icons.find(id);
    if (iter != m_icons.end())
    {
        return iter->second;
    }
    else
    {
        AFX_MANAGE_STATE(AfxGetStaticModuleState());
        HICON hIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(id), IMAGE_ICON, DPI(16), DPI(16), 0);
        m_icons[id] = hIcon;
        return hIcon;
    }
}


bool CDataManager::LoadTextContents(LPCTSTR file_path)
{
    //打开url
    if (CCommon::IsURL(file_path))
    {
        static std::wstring url;
        url = file_path;
        //在后台线程中打开url
        if (!m_is_thread_runing)
            AfxBeginThread(ThreadCallback, (LPVOID)url.c_str());
    }
    //打开本地文件
    else
    {
        std::ifstream file{ file_path, std::ios::binary };
        if (file.fail())
        {
            return false;
        }

        //保存文件的上次修改时间
        CCommon::GetFileLastModified(file_path, m_file_last_modified);

        std::wstring ext = utilities::CFilePathHelper(file_path).GetFileExtension();
        utilities::StringHelper::StringTransform(ext, false);
        //打开html文件
        if (ext == L"html" || ext == L"htm")
        {
            CHtmlToText html_to_text;
            html_to_text.ParseFromFile(file_path);
            m_text_contents = html_to_text.GetText();
        }
        //打开文本文件
        else
        {
            //获取文件长度
            file.seekg(0, file.end);
            size_t length = file.tellg();
            file.seekg(0, file.beg);
            if (length > MAX_FILE_SIZE)	//限制文件大小不超过MAX_FILE_SIZE
            {
                length = MAX_FILE_SIZE;
            }
            if (length <= 0)
                return false;

            //读取数据
            char* buff = new char[length + 1];
            file.read(buff, length);
            file.close();
            buff[length] = '\0';
            std::string str_contents(buff, length);
            delete[] buff;

            //判断是否是base64编码
            const int BASE64_MAX_LENGTH = 1048576;
            if (g_data.m_setting_data.auto_decode_base64 && utilities::IsBase64Code(str_contents, BASE64_MAX_LENGTH))
            {
                str_contents = utilities::Base64Decode(str_contents);
            }

            bool is_utf8 = CCommon::IsUTF8Bytes(str_contents.c_str());                              //判断编码类型
            m_text_contents = CCommon::StrToUnicode(str_contents.c_str(), is_utf8);	                //转换成Unicode
        }

        //解析章节
        m_chapter_parser.Parse();
    }

    return true;
}


const std::wstring& CDataManager::GetTextContexts() const
{
    return m_text_contents;
}

int CDataManager::GetPageStep(CDC* dc)
{
    if (dc == nullptr)
        return 1;
    return GetStepFromPosition(dc, m_setting_data.current_position);
}

int CDataManager::GetStepFromPosition(CDC* dc, int pos)
{
    const int text_len = static_cast<int>(m_text_contents.size());
    if (dc == nullptr || pos < 0 || pos >= text_len || m_draw_width <= 0)
        return 1;

    if (IsMultiLine())
    {
        //多行模式下DrawText遇到换行符会强制换行，且换行符在GetTextExtent中不占用宽度，
        //如果仍按文本宽度估算步长，翻页时会跨过未被显示的段落，导致部分文字无法显示。
        //因此这里按DrawText的实际排版结果计算第一行实际容纳的字符数，保证下一页的起始位置正好是上一页第二行的起始位置。
        int remain = text_len - pos;
        if (remain > 100)
            remain = 100;       //与DrawItem中截取的字符数保持一致
        //获取单行文本的高度
        TEXTMETRIC tm;
        dc->GetTextMetrics(&tm);
        const int line_height = tm.tmHeight + tm.tmExternalLeading;
        //二分查找：在一行高度内能够容纳的最大字符数
        int step = 1;
        int lo = 1, hi = remain;
        while (lo <= hi)
        {
            int mid = (lo + hi) / 2;
            CRect rect(0, 0, m_draw_width, 0);
            int height = dc->DrawText(m_text_contents.c_str() + pos, mid, rect, DT_NOPREFIX | DT_WORDBREAK | DT_CALCRECT);
            if (height <= line_height)
            {
                step = mid;
                lo = mid + 1;
            }
            else
            {
                hi = mid - 1;
            }
        }
        //跳过行尾的换行符，使下一页从有实际内容的位置开始
        while (pos + step < text_len && (m_text_contents[pos + step] == L'\r' || m_text_contents[pos + step] == L'\n'))
            step++;
        return step;
    }

    //单行模式：按文本宽度计算
    int step = 1;
    while (true)
    {
        if (pos + step >= text_len)
            break;
        int text_width = dc->GetTextExtent(m_text_contents.c_str() + pos, step).cx;
        if (text_width > m_draw_width)
        {
            return (step - 1);
        }
        step++;
    }
    return step;
}

int CDataManager::GetPrevPageStep(CDC* dc)
{
    if (dc == nullptr || !IsMultiLine())
        return 0;
    const int text_len = static_cast<int>(m_text_contents.size());
    const int pos = m_setting_data.current_position;
    if (pos <= 0 || pos > text_len)
        return 0;

    auto FindPrevSeg = [&](int seg_start) -> int {
        int i = seg_start - 1;
        while (i >= 0 && (m_text_contents[i] == L'\r' || m_text_contents[i] == L'\n')) i--;
        while (i >= 0 && m_text_contents[i] != L'\r' && m_text_contents[i] != L'\n') i--;
        if (i < 0) return 0;
        int j = i + 1;
        while (j < seg_start && (m_text_contents[j] == L'\r' || m_text_contents[j] == L'\n')) j++;
        return j;
    };

    int seg_start = 0;
    for (int idx = pos - 1; idx >= 0; --idx) {
        if (m_text_contents[idx] == L'\r' || m_text_contents[idx] == L'\n') {
            seg_start = idx + 1;
            while (seg_start < text_len && (m_text_contents[seg_start] == L'\r' || m_text_contents[seg_start] == L'\n'))
                seg_start++;
            if (seg_start <= pos) break;
            seg_start = 0;
        }
    }

    if (seg_start < pos) {
        int prev_p = seg_start;
        int p = seg_start;
        while (p < pos && p < text_len) {
            int sf = GetStepFromPosition(dc, p);
            if (sf <= 0 || p + sf <= p) break;
            int next_p = p + sf;
            if (next_p >= pos) break;
            prev_p = p;
            p = next_p;
        }
        return pos - prev_p;
    }

    if (seg_start == 0) return pos;

    int prev_seg_start = FindPrevSeg(seg_start);

    int q = prev_seg_start;
    while (q < text_len) {
        int sf = GetStepFromPosition(dc, q);
        if (sf <= 0 || q + sf <= q) break;
        int next_q = q + sf;
        if (next_q >= seg_start) break;
        q = next_q;
    }

    return pos - q;
}

void CDataManager::PageUp(int step)
{
    if (m_boss_key_pressed)
        return;
    if (step <= 0)
    {
        //多行模式下使用上一行的长度，保证上下翻页互为逆操作，避免翻页位置漂移
        if (IsMultiLine())
            step = m_prev_page_step;
        if (step <= 0)
            step = m_page_step;     //未找到上一行时回退为原步长
    }
    if (step <= 0)
        return;
    if (m_setting_data.current_position > 0)
        m_setting_data.current_position -= step;
    if (m_setting_data.current_position < 0)
        m_setting_data.current_position = 0;
    m_position_modified = true;
}

void CDataManager::PageDown(int step)
{
    if (step <= 0)
        step = m_page_step;
    if (m_boss_key_pressed)
        return;
    const int MAX_POS = static_cast<int>(GetTextContexts().size() - 2);
    if (m_setting_data.current_position < MAX_POS || m_setting_data.restart_at_end)
        m_setting_data.current_position += step;
    if (m_setting_data.current_position > MAX_POS)
    {
        if (m_setting_data.restart_at_end)
            m_setting_data.current_position = 0;
        else
            m_setting_data.current_position = MAX_POS;
    }
    if (m_setting_data.current_position < 0)
        m_setting_data.current_position = 0;
    m_position_modified = true;
}

void CDataManager::AddBookmark()
{
    m_bookmark_mgr.AddBookmark(m_setting_data.file_path, m_setting_data.current_position);
    g_data.SaveConfig();
}

bool CDataManager::IsMultiLine() const
{
    return m_multi_line && m_setting_data.enable_mulit_line;
}

bool CDataManager::HasFocus() const
{
    return GetForegroundWindow() == m_wnd;
}

CChapterParser & CDataManager::GetChapter()
{
    return m_chapter_parser;
}

const std::set<int>& CDataManager::GetBookmark()
{
    return m_bookmark_mgr.GetBookmark(m_setting_data.file_path);
}

void CDataManager::SaveReadPosition()
{
    if (m_position_modified)
    {
        SaveConfig();
        m_position_modified = false;
    }
}

void CDataManager::CheckFileChange()
{
    if (m_setting_data.auto_reload_when_file_changed && !m_setting_data.file_path.empty() && !CCommon::IsURL(m_setting_data.file_path))
    {
        //检查文件的最后修改时间
        unsigned __int64 last_modified{};
        if (CCommon::GetFileLastModified(m_setting_data.file_path, last_modified))
        {
            //如果文件的最后修改时间比打开的时候新，则重新打开文件
            if (last_modified > m_file_last_modified)
            {
                m_file_last_modified = last_modified;
                LoadTextContents(m_setting_data.file_path.c_str());
            }
        }
    }
}

UINT CDataManager::ThreadCallback(LPVOID dwUser)
{
    m_instance.m_is_thread_runing = true;
    const wchar_t* url = (const wchar_t*)dwUser;
    std::string url_contents;
    CHtmlToText html_to_text;
    html_to_text.ParseFromUrl(url);
    m_instance.m_text_contents = html_to_text.GetText();
    m_instance.m_chapter_parser.Parse();

    m_instance.m_is_thread_runing = false;
    return 0;
}
