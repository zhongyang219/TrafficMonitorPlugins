#include "pch.h"
#include "DataManager.h"
#include "Common.h"
#include <vector>
#include <sstream>
#include "../utilities/IniHelper.h"
#include <iomanip>
#include <afxinet.h>

constexpr auto WEB_USERAGENT = _T("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/135.0.0.0 Safari/537.36 Edg/135.0.0.0");

CDataManager CDataManager::m_instance;

CDataManager::CDataManager()
{
    // 初始化DPI
    HDC hDC = ::GetDC(HWND_DESKTOP);
    m_dpi = GetDeviceCaps(hDC, LOGPIXELSY);
    ::ReleaseDC(HWND_DESKTOP, hDC);
}

CDataManager::~CDataManager()
{
    SaveConfig();
}

CDataManager &CDataManager::Instance()
{
    return m_instance;
}

void CDataManager::ResetText()
{
    stockMarket.ClearRealtimeData();
}

static void WritePrivateProfileInt(const wchar_t *app_name, const wchar_t *key_name, int value, const wchar_t *file_path)
{
    wchar_t buff[16];
    swprintf_s(buff, L"%d", value);
    WritePrivateProfileString(app_name, key_name, buff, file_path);
}

void CDataManager::LoadConfig(const std::wstring &config_dir)
{
    // 获取模块的路径
    HMODULE hModule = reinterpret_cast<HMODULE>(&__ImageBase);
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(hModule, path, MAX_PATH);
    std::wstring module_path = path;
    m_config_path = module_path;
    m_log_path = module_path;
    if (!config_dir.empty())
    {
        size_t index = module_path.find_last_of(L"\\/");
        // 模块的文件名
        std::wstring module_file_name = module_path.substr(index + 1);
        module_file_name = module_file_name.substr(0, module_file_name.find_last_of(L"."));
        m_config_path = config_dir + module_file_name;
        m_log_path = config_dir + module_file_name;
    }
    m_config_path += L".ini";
    m_log_path += L".log";

    utilities::CIniHelper ini(m_config_path);
    ini.GetStringList(L"config", L"stock_code", m_setting_data.m_stock_codes, std::vector<std::wstring>{});
    m_setting_data.m_full_day = ini.GetBool(L"config", L"full_day", true);
    m_setting_data.m_show_stock_name = ini.GetBool(L"config", L"show_stock_name", true);
    m_setting_data.m_color_with_price = ini.GetBool(L"config", L"color_with_price", true);
    m_setting_data.m_kline_width = ini.GetInt(L"config", L"kline_width", 450);
    m_setting_data.m_kline_height = ini.GetInt(L"config", L"kline_height", 210);
}

void CDataManager::SaveConfig()
{
    if (!m_config_path.empty())
    {
        utilities::CIniHelper ini(m_config_path);
        ini.WriteStringList(L"config", L"stock_code", m_setting_data.m_stock_codes);
        ini.WriteBool(L"config", L"full_day", m_setting_data.m_full_day);
        ini.WriteBool(L"config", L"show_stock_name", m_setting_data.m_show_stock_name);
        ini.WriteBool(L"config", L"color_with_price", m_setting_data.m_color_with_price);
        ini.WriteInt(L"config", L"kline_width", m_setting_data.m_kline_width);
        ini.WriteInt(L"config", L"kline_height", m_setting_data.m_kline_height);
        ini.Save();
    }
}

const CString &CDataManager::StringRes(UINT id)
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

void CDataManager::DPIFromWindow(CWnd *pWnd)
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

// std::wstring StockInfo::ToString(bool include_name) const
// {
//     std::wstringstream wss;
//     if (include_name)
//         wss << name << ": ";
//     wss << p << ' ' << pc;
//     return wss.str();
// }

// bool StockInfo::IsEmpty() const
// {
//     return (pc.empty() || pc == L"--%")
//         && (p.empty() || p == L"--")
//         && name.empty();
// }

std::shared_ptr<StockData> CDataManager::GetStockData(const std::wstring &code)
{
    return stockMarket.getStock(code);
}

static double generateRandomDouble()
{
    srand(time(nullptr)); // 设置随机种子
    double random = (double)rand() / RAND_MAX;
    std::cout << std::fixed << std::setprecision(16);
    return random;
}

void CDataManager::RequestRealtimeData()
{
    TRACE(L"RequestRealtimeData...\n");
    std::vector<std::wstring> codes = m_setting_data.m_stock_codes;
    // https://hq.sinajs.cn/?_=0.1155744778269292&list=sz002497
    std::wstring url{L"https://hq.sinajs.cn/?"};
    std::vector<std::wstring> params;
    params.push_back(L"_=" + std::to_wstring(generateRandomDouble()));
    params.push_back(L"list=" + CCommon::vectorJoinString(codes, L","));

    url += CCommon::vectorJoinString(params, L"&");
    CString strHeaders = _T("Referer: https://finance.sina.com.cn");
    CCommon::WriteLog(url.c_str(), g_data.m_log_path.c_str());

    std::string Stock_data;
    if (CCommon::GetURL(url, Stock_data, false, WEB_USERAGENT, strHeaders, strHeaders.GetLength()))
    {
        stockMarket.LoadRealtimeDataByJson(Stock_data);
    }
}

void CDataManager::RequestTimelineData(std::wstring stock_id)
{
    try
    {
        TRACE(L"RequestTimelineData...\n");

        std::wstring url{L"https://cn.finance.sina.com.cn/minline/getMinlineData?"};
        // https://cn.finance.sina.com.cn/minline/getMinlineData?symbol=sz000100&version=7.11.0&dpc=1
        std::vector<std::wstring> params;
        params.push_back(L"symbol=" + stock_id);
        params.push_back(L"version=7.11.0");
        params.push_back(L"dpc=1");

        url += CCommon::vectorJoinString(params, L"&");
        CCommon::WriteLog(url.c_str(), g_data.m_log_path.c_str());

        // CString strHeaders = L"Referer: https://finance.sina.com.cn/realstock/company/" + m_stock_id + L"/nc.shtml";
        std::wstring strHeaders{L"Referer: https://finance.sina.com.cn/realstock/company/"};
        strHeaders += stock_id;
        strHeaders += L"/nc.shtml";
        CString headers = strHeaders.c_str();

        CInternetSession *session = new CInternetSession(WEB_USERAGENT);
        CHttpFile *pFile = (CHttpFile *)session->OpenURL(url.c_str(), 1, INTERNET_FLAG_TRANSFER_ASCII, headers, headers.GetLength());

        DWORD dwStatusCode;
        pFile->QueryInfoStatusCode(dwStatusCode);

        if (dwStatusCode == HTTP_STATUS_OK)
        {
            CString strData;
            char szBuffer[1025];
            int nRead;
            while ((nRead = pFile->Read(szBuffer, 1024)) > 0)
            {
                szBuffer[nRead] = 0;
                strData += CString(szBuffer);
            }

            stockMarket.LoadTimelineDataByJson(stock_id, &strData);
        }

        // 清理资源
        pFile->Close();
        delete pFile;
        session->Close();
    }
    catch (CInternetException *e)
    {
        e->Delete();
        stockMarket.LoadTimelineDataByJson(stock_id, NULL);
    }
}
