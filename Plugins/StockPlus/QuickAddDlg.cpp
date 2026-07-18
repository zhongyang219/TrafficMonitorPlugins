// QuickAddDlg.cpp: 实现文件
//

#include "pch.h"
#include "Stock.h"
#include "afxdialogex.h"
#include "QuickAddDlg.h"
#include "DataManager.h"
#include "Common.h"
#include <afxinet.h>
#include "../utilities/JsonHelper.h"

constexpr auto WEB_USERAGENT = _T("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/135.0.0.0 Safari/537.36 Edg/135.0.0.0");

// 搜索上下文（堆分配，传入工作线程；线程只持有 HWND，绝不访问对话框 C++ 对象）
struct SearchCtx
{
    HWND hWnd;
    std::wstring keyword;
};

// CQuickAddDlg 对话框

IMPLEMENT_DYNAMIC(CQuickAddDlg, CDialog)

CQuickAddDlg::CQuickAddDlg(CWnd *pParent /*=nullptr*/)
    : CDialog(IDD_QUICK_ADD_DIALOG, pParent)
{
}

CQuickAddDlg::~CQuickAddDlg()
{
}

void CQuickAddDlg::DoDataExchange(CDataExchange *pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_QA_SEARCH, m_search_edit);
    DDX_Control(pDX, IDC_QA_LIST, m_result_list);
}

BEGIN_MESSAGE_MAP(CQuickAddDlg, CDialog)
ON_EN_CHANGE(IDC_QA_SEARCH, &CQuickAddDlg::OnChangeSearch)
ON_WM_TIMER()
ON_LBN_DBLCLK(IDC_QA_LIST, &CQuickAddDlg::OnLbnDblclkList)
ON_BN_CLICKED(IDOK, &CQuickAddDlg::OnBnClickedOk)
ON_MESSAGE(WM_QA_RESULTS, &CQuickAddDlg::OnResults)
END_MESSAGE_MAP()

// CQuickAddDlg 消息处理程序

// CreateThread 兼容包装
static DWORD WINAPI SearchThreadProcWrapper(LPVOID pParam)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return CQuickAddDlg::SearchThreadProc(pParam);
}

BOOL CQuickAddDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    HICON hIcon = g_data.GetIcon(IDI_STOCK);
    SetIcon(hIcon, FALSE);

    m_search_edit.SetFocus();
    return FALSE; // 已手动设置焦点
}

void CQuickAddDlg::OnChangeSearch()
{
    // 防抖：每次输入变化重置 300ms 定时器
    SetTimer(TIMER_SEARCH, 300, nullptr);
}

void CQuickAddDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == TIMER_SEARCH)
    {
        KillTimer(TIMER_SEARCH);
        CString keyword;
        m_search_edit.GetWindowText(keyword);
        if (keyword.IsEmpty())
        {
            m_result_list.ResetContent();
            m_results.clear();
            return;
        }
        if (m_searching)
        {
            // 当前仍在搜索，稍后重试
            SetTimer(TIMER_SEARCH, 200, nullptr);
            return;
        }

        // 立即给出"搜索中"反馈
        m_result_list.ResetContent();
        m_result_list.AddString(g_data.StringRes(IDS_QA_SEARCHING));
        m_searching = true;

        auto *ctx = new SearchCtx{GetSafeHwnd(), std::wstring(keyword.GetString())};
        // 用 CreateThread 避免 MFC 线程追踪
        HANDLE hThread = CreateThread(nullptr, 0, SearchThreadProcWrapper, ctx, 0, nullptr);
        if (hThread) CloseHandle(hThread);
    }
    CDialog::OnTimer(nIDEvent);
}

UINT CQuickAddDlg::SearchThreadProc(LPVOID pParam)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    std::unique_ptr<SearchCtx> ctx(reinterpret_cast<SearchCtx *>(pParam));

    auto *results = new std::vector<StockSearchResult>();

    std::wstring url = L"https://proxy.finance.qq.com/ifzqgtimg/appstock/smartbox/search/get?q=" + CCommon::URLEncode(ctx->keyword);

    std::string json;
    if (CCommon::GetURL(url, json, true, WEB_USERAGENT, nullptr, 0))
    {
        yyjson_doc *doc = yyjson_read(json.data(), json.size(), 0);
        if (doc)
        {
            yyjson_val *root = yyjson_doc_get_root(doc);
            yyjson_val *data = yyjson_obj_get(root, "data");
            if (data && yyjson_is_obj(data))
            {
                yyjson_val *stock = yyjson_obj_get(data, "stock");
                if (stock && yyjson_is_arr(stock))
                {
                    size_t n = yyjson_arr_size(stock);
                    for (size_t i = 0; i < n; i++)
                    {
                        yyjson_val *item = yyjson_arr_get(stock, i);
                        if (!yyjson_is_arr(item))
                            continue;
                        if (yyjson_arr_size(item) < 3)
                            continue;
                        yyjson_val *vMarket = yyjson_arr_get(item, 0);
                        yyjson_val *vCode = yyjson_arr_get(item, 1);
                        yyjson_val *vName = yyjson_arr_get(item, 2);
                        if (!yyjson_is_str(vMarket) || !yyjson_is_str(vCode) || !yyjson_is_str(vName))
                            continue;
                        std::string market = yyjson_get_str(vMarket);
                        // 仅 A 股
                        if (market != "sh" && market != "sz" && market != "bj")
                            continue;
                        StockSearchResult r;
                        r.prefix = CCommon::StrToUnicode(market.c_str(), true);
                        r.code = CCommon::StrToUnicode(yyjson_get_str(vCode), true);
                        r.name = CCommon::StrToUnicode(yyjson_get_str(vName), true);
                        results->push_back(r);
                    }
                }
            }
            yyjson_doc_free(doc);
        }
    }

    // 通过 PostMessage 把结果交回 UI 线程；窗口已销毁则自行释放
    if (!::PostMessage(ctx->hWnd, WM_QA_RESULTS, 0, reinterpret_cast<LPARAM>(results)))
    {
        delete results;
    }
    return 0;
}

LRESULT CQuickAddDlg::OnResults(WPARAM wParam, LPARAM lParam)
{
    std::unique_ptr<std::vector<StockSearchResult>> results(reinterpret_cast<std::vector<StockSearchResult> *>(lParam));
    m_searching = false;
    m_results = std::move(*results);

    m_result_list.ResetContent();
    if (m_results.empty())
    {
        m_result_list.AddString(g_data.StringRes(IDS_QA_NO_RESULT));
        return 0;
    }
    for (const auto &r : m_results)
    {
        CString line;
        line.Format(_T("%s  %s(%s)"), r.name.c_str(), r.code.c_str(), r.prefix.c_str());
        m_result_list.AddString(line);
    }
    return 0;
}

void CQuickAddDlg::OnLbnDblclkList()
{
    // 双击列表项等同于确定
    OnBnClickedOk();
}

void CQuickAddDlg::OnBnClickedOk()
{
    int curSel = m_result_list.GetCurSel();
    if (curSel < 0 || curSel >= (int)m_results.size())
        return; // 未选中有效项，不关闭

    m_selected = m_results[curSel].StockId();
    CDialog::OnOK();
}
