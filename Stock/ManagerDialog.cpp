// ManagerDialog.cpp: 实现文件
//

#include "pch.h"
#include "Stock.h"
#include "afxdialogex.h"
#include "ManagerDialog.h"
#include "Common.h"
#include "OptionsDlg.h"
#include <Windows.h>

// CManagerDialog 对话框

IMPLEMENT_DYNAMIC(CManagerDialog, CDialog)

CManagerDialog::CManagerDialog(CWnd* pParent /*=nullptr*/)
	: CDialog(IDD_MANAGER_DIALOG, pParent)
{
}

CManagerDialog::~CManagerDialog()
{
}

void CManagerDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_MGR_LIST, m_Stock_listbox);
}


BEGIN_MESSAGE_MAP(CManagerDialog, CDialog)
    ON_LBN_SELCHANGE(IDC_MGR_LIST, &CManagerDialog::OnListItemClick)
    ON_BN_CLICKED(IDC_MGR_DEL_BTN, &CManagerDialog::OnDelBtnClick)
    ON_BN_CLICKED(IDC_MGR_ADD_BTN, &CManagerDialog::OnAddBtnClick)
    ON_BN_CLICKED(IDC_FULL_DAY_CHECK, &CManagerDialog::OnClickedFullDayCheck)
    ON_BN_CLICKED(IDOK, &CManagerDialog::OnBnClickedOk)
    ON_BN_CLICKED(IDCANCEL, &CManagerDialog::OnBnClickedCancel)
    ON_LBN_DBLCLK(IDC_MGR_LIST, &CManagerDialog::OnLbnDblclkMgrList)
END_MESSAGE_MAP()


// CManagerDialog 消息处理程序

BOOL CManagerDialog::OnInitDialog()
{
    CDialog::OnInitDialog();

    for (CString StockCode : m_data.m_Stock_codes)
    {
        m_Stock_listbox.AddString(StockCode);
    }

    for (int i = 0; i < m_Stock_listbox.GetCount(); i++)
    {
        m_Stock_listbox.SetItemHeight(i, g_data.DPI(22));
    }

    CheckDlgButton(IDC_FULL_DAY_CHECK, m_data.m_full_day);

    return TRUE;  // return TRUE unless you set the focus to a control
                  // 异常: OCX 属性页应返回 FALSE
}


void CManagerDialog::OnListItemClick()
{
    CString curSelTxt;
    int curSelPos;

    curSelPos = m_Stock_listbox.GetCurSel();
    m_Stock_listbox.GetText(curSelPos, curSelTxt);
    Log1("OnListItemClick: %s\n", curSelTxt.GetString());
}


void CManagerDialog::OnDelBtnClick()
{
    int curSelPos = m_Stock_listbox.GetCurSel();
    Log1("OnDelBtnClick: %d\n", curSelPos);
    if (curSelPos < 0 || curSelPos > m_data.m_Stock_codes.size())
    {
        return;
    }
    m_Stock_listbox.DeleteString(curSelPos);
    m_data.m_Stock_codes.erase(m_data.m_Stock_codes.begin() + curSelPos);
    m_data.updateAllCodeStr();
}


void CManagerDialog::OnAddBtnClick()
{
    if (m_data.m_Stock_codes.size() >= Stock_ITEM_MAX)
    {
        MessageBox(TEXT("由于TrafficMonitor无法动态创建Item，已达到设定上限"), TEXT(""), MB_ICONWARNING | MB_OK);
        return;
    }
    COptionsDlg dlg;
    auto rtn = dlg.DoModal();
    if (rtn == IDOK)
    {
        CString newStockCode = dlg.m_Stock_code;
        if (!newStockCode.IsEmpty())
        {
            if (count(m_data.m_Stock_codes.begin(), m_data.m_Stock_codes.end(), newStockCode))
            {
                Log1("OnAddBtnClick: ignore %s\n", newStockCode.GetString());
                return;
            }
            Log1("OnAddBtnClick: %s\n", newStockCode.GetString());
            m_data.m_Stock_codes.push_back(newStockCode);
            m_data.updateAllCodeStr();
            m_Stock_listbox.AddString(newStockCode);
        }
    }
}

void CManagerDialog::OnClickedFullDayCheck()
{
    m_data.m_full_day = (IsDlgButtonChecked(IDC_FULL_DAY_CHECK) != 0);
}

void CManagerDialog::OnBnClickedOk()
{
    bool Stock_code_changed{ g_data.m_setting_data.m_all_Stock_code_str != m_data.m_all_Stock_code_str };
    if (Stock_code_changed)
    {
        g_data.m_setting_data = m_data;
        g_data.SaveConfig();
        Stock::Instance().SendStockInfoQequest();
        MessageBox(TEXT("如果界面未刷新请重启TrafficMonitor"), TEXT(""), MB_ICONWARNING | MB_OK);
    }
    CDialog::OnOK();
}

void CManagerDialog::OnBnClickedCancel()
{
    CDialog::OnCancel();
}


void CManagerDialog::OnLbnDblclkMgrList()
{
    int index = m_Stock_listbox.GetCurSel();
    if (index >= 0 && index < m_data.m_Stock_codes.size())
    {
        COptionsDlg dlg(m_data.m_Stock_codes[index].GetString());
        auto rtn = dlg.DoModal();
        if (rtn == IDOK)
        {
            if (!dlg.m_Stock_code.IsEmpty())
            {
                m_data.m_Stock_codes[index] = dlg.m_Stock_code;
                m_data.updateAllCodeStr();
                m_Stock_listbox.DeleteString(index);
                m_Stock_listbox.InsertString(index, dlg.m_Stock_code);
            }
        }
    }
}
