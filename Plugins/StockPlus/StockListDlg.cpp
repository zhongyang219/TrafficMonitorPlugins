// StockListDlg.cpp: 实现文件
//

#include "pch.h"
#include "Stock.h"
#include "afxdialogex.h"
#include "StockListDlg.h"
#include "DataManager.h"
#include "Common.h"

IMPLEMENT_DYNAMIC(CStockListDlg, CDialog)

CStockListDlg::CStockListDlg(CWnd *pParent /*=nullptr*/)
    : CDialog(IDD_STOCK_LIST_DIALOG, pParent)
{
}

CStockListDlg::~CStockListDlg()
{
}

void CStockListDlg::DoDataExchange(CDataExchange *pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_STOCK_LIST, m_list);
}

BEGIN_MESSAGE_MAP(CStockListDlg, CDialog)
ON_WM_DESTROY()
ON_WM_TIMER()
ON_BN_CLICKED(IDC_REFRESH_BTN, &CStockListDlg::OnBnClickedRefresh)
ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, &CStockListDlg::OnCustomDraw)
END_MESSAGE_MAP()

// CStockListDlg 消息处理程序

BOOL CStockListDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    HICON hIcon = g_data.GetIcon(IDI_STOCK);
    SetIcon(hIcon, FALSE);

    m_list.SetExtendedStyle(m_list.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    m_list.InsertColumn(0, g_data.StringRes(IDS_COL_NAME), LVCFMT_LEFT, 100);
    m_list.InsertColumn(1, g_data.StringRes(IDS_COL_PRICE), LVCFMT_RIGHT, 70);
    m_list.InsertColumn(2, g_data.StringRes(IDS_COL_CHANGE), LVCFMT_RIGHT, 70);
    m_list.InsertColumn(3, g_data.StringRes(IDS_COL_PERCENT), LVCFMT_RIGHT, 70);
    m_list.InsertColumn(4, g_data.StringRes(IDS_COL_OPEN), LVCFMT_RIGHT, 70);
    m_list.InsertColumn(5, g_data.StringRes(IDS_COL_HIGH), LVCFMT_RIGHT, 70);
    m_list.InsertColumn(6, g_data.StringRes(IDS_COL_LOW), LVCFMT_RIGHT, 70);
    m_list.InsertColumn(7, g_data.StringRes(IDS_COL_VOLUME), LVCFMT_RIGHT, 90);
    m_list.InsertColumn(8, g_data.StringRes(IDS_COL_TURNOVER), LVCFMT_RIGHT, 90);

    RefreshList();
    SetTimer(TIMER_REFRESH, 3000, nullptr);
    return TRUE;
}

void CStockListDlg::OnDestroy()
{
    KillTimer(TIMER_REFRESH);
    CDialog::OnDestroy();
}

void CStockListDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == TIMER_REFRESH)
        RefreshList();
    CDialog::OnTimer(nIDEvent);
}

void CStockListDlg::OnBnClickedRefresh()
{
    Stock::Instance().SendStockInfoRequest();
    RefreshList();
}

static CString FormatVolume(long long v)
{
    CString s;
    if (v >= 100000000)
        s.Format(_T("%.2f亿股"), v / 100000000.0);
    else if (v >= 10000)
        s.Format(_T("%.2f万股"), v / 10000.0);
    else
        s.Format(_T("%lld股"), v);
    return s;
}

static CString FormatAmount(double v)
{
    CString s;
    if (v >= 100000000)
        s.Format(_T("%.2f亿"), v / 100000000.0);
    else if (v >= 10000)
        s.Format(_T("%.2f万"), v / 10000.0);
    else
        s.Format(_T("%.2f"), v);
    return s;
}

void CStockListDlg::RefreshList()
{
    m_list.DeleteAllItems();
    m_is_down.clear();

    const auto &codes = g_data.m_setting_data.m_stock_codes;
    int row = 0;
    for (const auto &code : codes)
    {
        auto pData = g_data.GetStockData(code);
        bool ok = (pData && pData->info.is_ok && !pData->info.displayName.empty());
        const auto &rt = pData ? pData->realTimeData : STOCK::RealTimeData{};

        CString name = ok ? CString(pData->info.displayName.c_str()) : CString(code.c_str());
        CString price = ok ? CString(rt.displayPrice.c_str()) : _T("--");

        CString change;
        if (ok && rt.currentPrice > 0 && rt.prevClosePrice > 0)
            change.Format(_T("%+.2f"), rt.currentPrice - rt.prevClosePrice);
        else
            change = _T("--");

        CString percent = ok ? CString(rt.displayFluctuation.c_str()) : _T("--");
        CString open, high, low;
        if (ok)
        {
            open.Format(_T("%.2f"), rt.openPrice);
            high.Format(_T("%.2f"), rt.highPrice);
            low.Format(_T("%.2f"), rt.lowPrice);
        }
        else
        {
            open = high = low = _T("--");
        }
        CString volume = ok ? FormatVolume(rt.volume) : _T("--");
        CString turnover = ok ? FormatAmount(rt.turnover) : _T("--");

        bool isDown = ok && (rt.displayFluctuation.find(L'-') != std::wstring::npos);

        m_list.InsertItem(row, name);
        m_list.SetItemText(row, 1, price);
        m_list.SetItemText(row, 2, change);
        m_list.SetItemText(row, 3, percent);
        m_list.SetItemText(row, 4, open);
        m_list.SetItemText(row, 5, high);
        m_list.SetItemText(row, 6, low);
        m_list.SetItemText(row, 7, volume);
        m_list.SetItemText(row, 8, turnover);
        m_is_down.push_back(isDown);
        row++;
    }
}

void CStockListDlg::OnCustomDraw(NMHDR *pNMHDR, LRESULT *pResult)
{
    LPNMLVCUSTOMDRAW pCD = reinterpret_cast<LPNMLVCUSTOMDRAW>(pNMHDR);
    *pResult = CDRF_DODEFAULT;

    switch (pCD->nmcd.dwDrawStage)
    {
    case CDDS_PREPAINT:
        *pResult = CDRF_NOTIFYITEMDRAW;
        break;
    case CDDS_ITEMPREPAINT:
        *pResult = CDRF_NOTIFYSUBITEMDRAW;
        break;
    case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
    {
        int row = (int)pCD->nmcd.dwItemSpec;
        int col = pCD->iSubItem;
        // 现价/涨跌额/涨跌幅 三列上红绿
        if (col == 1 || col == 2 || col == 3)
        {
            if (row >= 0 && row < (int)m_is_down.size())
            {
                pCD->clrText = m_is_down[row] ? RGB(46, 139, 87) : RGB(195, 0, 0);
            }
        }
        break;
    }
    default:
        break;
    }
}
