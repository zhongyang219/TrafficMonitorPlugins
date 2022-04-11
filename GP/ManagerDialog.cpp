// ManagerDialog.cpp: 实现文件
//

#include "pch.h"
#include "GP.h"
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
	DDX_Control(pDX, IDC_MGR_LIST, m_gp_listbox);
}


BEGIN_MESSAGE_MAP(CManagerDialog, CDialog)
    ON_LBN_SELCHANGE(IDC_MGR_LIST, &CManagerDialog::OnListItemClick)
    ON_BN_CLICKED(IDC_MGR_DEL_BTN, &CManagerDialog::OnDelBtnClick)
    ON_BN_CLICKED(IDC_MGR_ADD_BTN, &CManagerDialog::OnAddBtnClick)
    ON_BN_CLICKED(IDC_FULL_DAY_CHECK, &CManagerDialog::OnClickedFullDayCheck)
    ON_BN_CLICKED(IDOK, &CManagerDialog::OnBnClickedOk)
    ON_BN_CLICKED(IDCANCEL, &CManagerDialog::OnBnClickedCancel)
END_MESSAGE_MAP()


// CManagerDialog 消息处理程序

BOOL CManagerDialog::OnInitDialog()
{
    CDialog::OnInitDialog();

    for (CString gpCode : m_data.m_gp_codes)
    {
        m_gp_listbox.AddString(gpCode);
    }

    CheckDlgButton(IDC_FULL_DAY_CHECK, m_data.m_full_day);

    return TRUE;  // return TRUE unless you set the focus to a control
                  // 异常: OCX 属性页应返回 FALSE
}


void CManagerDialog::OnListItemClick()
{
    CString curSelTxt;
    int curSelPos;

    curSelPos = m_gp_listbox.GetCurSel();
    m_gp_listbox.GetText(curSelPos, curSelTxt);
    Log1("OnListItemClick: %s\n", curSelTxt);
}


void CManagerDialog::OnDelBtnClick()
{
    int curSelPos = m_gp_listbox.GetCurSel();
    Log1("OnDelBtnClick: %d\n", curSelPos);
    if (curSelPos < 0 || curSelPos > m_data.m_gp_codes.size())
    {
        return;
    }
    m_gp_listbox.DeleteString(curSelPos);
    m_data.m_gp_codes.erase(m_data.m_gp_codes.begin() + curSelPos);
    m_data.updateAllCodeStr();
}


void CManagerDialog::OnAddBtnClick()
{
    if (m_data.m_gp_codes.size() >= GP_ITEM_MAX)
    {
        MessageBox(TEXT("由于TrafficMonitor无法动态创建Item，已达到设定上限"), TEXT(""), MB_ICONWARNING | MB_OK);
        return;
    }
    COptionsDlg dlg;
    auto rtn = dlg.DoModal();
    if (rtn == IDOK)
    {
        CString newGpCode = dlg.m_gp_code;
        if (!newGpCode.IsEmpty())
        {
            if (count(m_data.m_gp_codes.begin(), m_data.m_gp_codes.end(), newGpCode))
            {
                Log1("OnAddBtnClick: ignore %s\n", newGpCode);
                return;
            }
            Log1("OnAddBtnClick: %s\n", newGpCode);
            m_data.m_gp_codes.push_back(newGpCode);
            m_data.updateAllCodeStr();
            m_gp_listbox.AddString(newGpCode);
        }
    }
}

void CManagerDialog::OnClickedFullDayCheck()
{
    m_data.m_full_day = (IsDlgButtonChecked(IDC_FULL_DAY_CHECK) != 0);
}

void CManagerDialog::OnBnClickedOk()
{
    bool gp_code_changed{ g_data.m_setting_data.m_all_gp_code_str != m_data.m_all_gp_code_str };
    if (gp_code_changed)
    {
        g_data.m_setting_data = m_data;
        g_data.SaveConfig();
        GP::Instance().SendGPInfoQequest();
        MessageBox(TEXT("如果界面未刷新请重启TrafficMonitor"), TEXT(""), MB_ICONWARNING | MB_OK);
    }
    CDialog::OnOK();
}

void CManagerDialog::OnBnClickedCancel()
{
    CDialog::OnCancel();
}
