
// PluginTesterDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "PluginTester.h"
#include "PluginTesterDlg.h"
#include "afxdialogex.h"
#include "../utilities/bass64/base64.h"
#include "../utilities/Common.h"
#include <fstream>
#include "PluginInfoDlg.h"
#include "DrawCommon.h"
#include <vector>
#include "../utilities/FilePathHelper.h"
#include "../utilities/IniHelper.h"
#include "WIC.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define TIMER_ID 1548

// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
    CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_ABOUTBOX };
#endif

    protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
    DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CPluginTesterDlg 对话框



CPluginTesterDlg::CPluginTesterDlg(CWnd* pParent /*=NULL*/)
    : CDialog(IDD_PLUGINTESTER_DIALOG, pParent)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CPluginTesterDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_SELECT_PLUGIN_COMBO, m_select_plugin_combo);
}

PluginInfo CPluginTesterDlg::LoadPlugin(const std::wstring& plugin_file_path, const std::wstring& config_dir)
{
    PluginInfo plugin_info;
    if (plugin_file_path.empty())
        return plugin_info;

    if (plugin_info.plugin_module != NULL)
        FreeLibrary(plugin_info.plugin_module);

    //插件dll的路径
    plugin_info.file_path = plugin_file_path;

    //载入dll
    plugin_info.plugin_module = LoadLibrary(plugin_info.file_path.c_str());
    if (plugin_info.plugin_module == NULL)
    {
        return plugin_info;
    }

    //获取函数的入口地址
    pfTMPluginGetInstance TMPluginGetInstance = (pfTMPluginGetInstance)::GetProcAddress(plugin_info.plugin_module, "TMPluginGetInstance");
    if (TMPluginGetInstance == NULL)
    {
        return plugin_info;
    }

    //创建插件对象
    plugin_info.plugin = TMPluginGetInstance();
    if (plugin_info.plugin == nullptr)
        return plugin_info;

    ////检查插件版本
    //int version = plugin_info.plugin->GetAPIVersion();
    //if (version <= PLUGIN_UNSUPPORT_VERSION)
    //{
    //    plugin_info.state = PluginState::PS_VERSION_NOT_SUPPORT;
    //    continue;
    //}

    if (plugin_info.plugin != nullptr)
        plugin_info.plugin->OnExtenedInfo(ITMPlugin::EI_CONFIG_DIR, config_dir.c_str());

    //获取插件信息
    for (int i{}; i < ITMPlugin::TMI_MAX; i++)
    {
        ITMPlugin::PluginInfoIndex index{ static_cast<ITMPlugin::PluginInfoIndex>(i) };
        plugin_info.properties[index] = plugin_info.plugin->GetInfo(index);
    }

    //获取插件显示项目
    int index = 0;
    for (int i = 0; i < 99; i++)
    {
        IPluginItem* item = plugin_info.plugin->GetItem(index);
        if (item == nullptr)
            break;
        plugin_info.plugin_items.push_back(item);
        index++;
    }

    return plugin_info;
}

void CPluginTesterDlg::EnableControl()
{
    CString str;
    m_select_plugin_combo.GetWindowText(str);
    bool enable = (!str.IsEmpty());
    GetDlgItem(IDC_OPTION_BUTTON)->EnableWindow(enable);
    GetDlgItem(IDC_DETAIL_BUTTON)->EnableWindow(enable);

    bool cur_dir_checked = IsDlgButtonChecked(IDC_CUR_DIR_RADIO);
    GetDlgItem(IDC_EDIT1)->EnableWindow(!cur_dir_checked);
    GetDlgItem(IDC_BROWSE_BUTTON)->EnableWindow(!cur_dir_checked);

    GetDlgItem(IDC_ITEM_WIDTH_EDIT)->EnableWindow(IsDlgButtonChecked(IDC_SPECIFY_WIDTH_CHECK));

    bool plugin_command_exist = false;
    PluginInfo cur_plugin = GetCurrentPlugin();
    if (cur_plugin.plugin != nullptr && cur_plugin.plugin->GetAPIVersion() >= 5)
    {
        plugin_command_exist = cur_plugin.plugin->GetCommandCount() > 0;
    }
    GetDlgItem(IDC_PLUGIN_COMMANDS_BUTTON)->EnableWindow(plugin_command_exist);
}

PluginInfo CPluginTesterDlg::GetCurrentPlugin()
{
    if (m_cur_index >= 0 && m_cur_index < m_plugins.size())
        return m_plugins[m_cur_index];
    else
        return PluginInfo();
}

BEGIN_MESSAGE_MAP(CPluginTesterDlg, CDialog)
    ON_WM_SYSCOMMAND()
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_COMMAND(ID_BASS64_ENCODE, &CPluginTesterDlg::OnBass64Encode)
    ON_COMMAND(ID_BASE64_DECODE, &CPluginTesterDlg::OnBase64Decode)
    ON_COMMAND(ID_APP_ABOUT, &CPluginTesterDlg::OnAppAbout)
    ON_CBN_SELCHANGE(IDC_SELECT_PLUGIN_COMBO, &CPluginTesterDlg::OnCbnSelchangeSelectPluginCombo)
    ON_BN_CLICKED(IDC_OPTION_BUTTON, &CPluginTesterDlg::OnBnClickedOptionButton)
    ON_BN_CLICKED(IDC_DETAIL_BUTTON, &CPluginTesterDlg::OnBnClickedDetailButton)
    ON_WM_TIMER()
    ON_WM_LBUTTONUP()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_RBUTTONUP()
    ON_WM_SIZE()
    ON_BN_CLICKED(IDC_DARK_BACKGROUND_CHECK, &CPluginTesterDlg::OnBnClickedDarkBackgroundCheck)
    ON_BN_CLICKED(IDC_DOUBLE_LINE_CHECK, &CPluginTesterDlg::OnBnClickedDoubleLineCheck)
    ON_BN_CLICKED(IDC_CUR_DIR_RADIO, &CPluginTesterDlg::OnBnClickedCurDirRadio)
    ON_BN_CLICKED(IDC_USER_DEFINED_RADIO, &CPluginTesterDlg::OnBnClickedUserDefinedRadio)
    ON_BN_CLICKED(IDC_BROWSE_BUTTON, &CPluginTesterDlg::OnBnClickedBrowseButton)
    ON_WM_DESTROY()
    ON_EN_CHANGE(IDC_ITEM_WIDTH_EDIT, &CPluginTesterDlg::OnEnChangeEdit2)
    ON_BN_CLICKED(IDC_SPECIFY_WIDTH_CHECK, &CPluginTesterDlg::OnBnClickedSpecifyWidthCheck)
    ON_COMMAND(ID_LANGUAGE_CHINESE, &CPluginTesterDlg::OnLanguageChinese)
    ON_COMMAND(ID_LANGUAGE_ENGLISH, &CPluginTesterDlg::OnLanguageEnglish)
    ON_COMMAND(ID_LANGUAGE_FOLLOWING_SYSTEM, &CPluginTesterDlg::OnLanguageFollowingSystem)
    ON_WM_INITMENU()
    ON_BN_CLICKED(IDC_PLUGIN_COMMANDS_BUTTON, &CPluginTesterDlg::OnBnClickedPluginCommandsButton)
END_MESSAGE_MAP()


// CPluginTesterDlg 消息处理程序

BOOL CPluginTesterDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // 将“关于...”菜单项添加到系统菜单中。

    // IDM_ABOUTBOX 必须在系统命令范围内。
    ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
    ASSERT(IDM_ABOUTBOX < 0xF000);

    CMenu* pSysMenu = GetSystemMenu(FALSE);
    if (pSysMenu != NULL)
    {
        BOOL bNameValid;
        CString strAboutMenu;
        bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
        ASSERT(bNameValid);
        if (!strAboutMenu.IsEmpty())
        {
            pSysMenu->AppendMenu(MF_SEPARATOR);
            pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
#ifdef _DEBUG
            pSysMenu->AppendMenu(MF_STRING, IDM_TEST_CMD, _T("Test Command"));
#endif
        }
    }

    // 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
    //  执行此操作
    SetIcon(m_hIcon, TRUE);			// 设置大图标
    SetIcon(m_hIcon, FALSE);		// 设置小图标

    // TODO: 在此添加额外的初始化代码

    LoadConfig();

    InitPlugins();

    //为菜单添加图标
    HMENU menu = GetMenu()->GetSafeHmenu();
    if (menu != NULL)
    {
        CMenuIcon::AddIconToMenuItem(menu, ID_APP_EXIT, FALSE, theApp.GetIcon(IDI_EXIT));
        CMenuIcon::AddIconToMenuItem(menu, ID_HELP, FALSE, theApp.GetIcon(IDI_HELP));
        CMenuIcon::AddIconToMenuItem(menu, ID_APP_ABOUT, FALSE, theApp.GetIcon(IDR_MAINFRAME));
    }

    //初始化预览视图
    m_view = (CDrawScrollView*)RUNTIME_CLASS(CDrawScrollView)->CreateObject();
    m_proview_top_pos = CalculatePreviewTopPos();
    CRect scroll_view_rect;
    GetClientRect(scroll_view_rect);
    scroll_view_rect.top = m_proview_top_pos;
    m_view->Create(NULL, NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL, scroll_view_rect, this, 3000);
    m_view->InitialUpdate();
    m_view->ShowWindow(SW_SHOW);
    m_view->SetSize(CalculatePreviewSize());

    SetTimer(TIMER_ID, 1000, nullptr);

    return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CPluginTesterDlg::InitPlugins()
{
    //加载所有插件，初始化下拉列表
    m_plugin_dir = GetPluginDir();

    std::vector<std::wstring> files;
    utilities::CCommon::GetFiles((m_plugin_dir + L"*.dll").c_str(), files);
    for (const auto& file_name : files)
    {
        m_select_plugin_combo.AddString(file_name.c_str());
        m_plugins.push_back(LoadPlugin(m_plugin_dir + file_name, m_plugin_dir));
        ITMPlugin* plugin = m_plugins.back().plugin;
        //if (plugin != nullptr)
        //    plugin->OnExtenedInfo(ITMPlugin::EI_CONFIG_DIR, plugin_dir.c_str());
    }

    m_select_plugin_combo.SetCurSel(0);
    OnCbnSelchangeSelectPluginCombo();
}

void CPluginTesterDlg::LoadConfig()
{
    utilities::CIniHelper ini(theApp.m_config_path);
    bool is_cur_dir = ini.GetBool(L"config", L"is_cur_dir", true);
    std::wstring plugin_dir = ini.GetString(L"config", L"plugin_dir");
    bool dark_mode = ini.GetBool(L"config", L"dark_mode");
    bool double_line = ini.GetBool(L"config", L"double_line");
    bool specify_item_width = ini.GetBool(L"config", L"specify_item_width");
    int item_width = ini.GetInt(L"config", L"item_width", 120);

    if (is_cur_dir)
        CheckDlgButton(IDC_CUR_DIR_RADIO, TRUE);
    else
        CheckDlgButton(IDC_USER_DEFINED_RADIO, TRUE);
    SetDlgItemText(IDC_EDIT1, plugin_dir.c_str());
    CheckDlgButton(IDC_DARK_BACKGROUND_CHECK, dark_mode);
    CheckDlgButton(IDC_DOUBLE_LINE_CHECK, double_line);
    CheckDlgButton(IDC_SPECIFY_WIDTH_CHECK, specify_item_width);

    SetDlgItemText(IDC_ITEM_WIDTH_EDIT, std::to_wstring(item_width).c_str());
}

void CPluginTesterDlg::SaveConfig() const
{
    utilities::CIniHelper ini(theApp.m_config_path);
    ini.WriteBool(L"config", L"is_cur_dir", IsDlgButtonChecked(IDC_CUR_DIR_RADIO));
    CString plugin_dir;
    GetDlgItemText(IDC_EDIT1, plugin_dir);
    ini.WriteString(L"config", L"plugin_dir", plugin_dir.GetString());
    ini.WriteBool(L"config", L"dark_mode", IsDlgButtonChecked(IDC_DARK_BACKGROUND_CHECK));
    ini.WriteBool(L"config", L"double_line", IsDlgButtonChecked(IDC_DOUBLE_LINE_CHECK));
    ini.WriteBool(L"config", L"specify_item_width", IsDlgButtonChecked(IDC_SPECIFY_WIDTH_CHECK));
    CString item_width;
    GetDlgItemText(IDC_ITEM_WIDTH_EDIT, item_width);
    ini.WriteString(L"config", L"item_width", item_width.GetString());

    ini.Save();
}

void CPluginTesterDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
    if (nID == IDM_TEST_CMD)
    {
        std::string str_base64 = utilities::Base64Encode(utilities::StringHelper::UnicodeToStr(L"测试文本abcde", true));

        std::wstring str_ori = utilities::StringHelper::StrToUnicode(utilities::Base64Decode(str_base64).c_str(), true);

        bool is_base64 = utilities::IsBase64Code(str_base64);

        int a = 0;
    }
    else if ((nID & 0xFFF0) == IDM_ABOUTBOX)
    {
        CAboutDlg dlgAbout;
        dlgAbout.DoModal();
    }
    else
    {
        CDialog::OnSysCommand(nID, lParam);
    }
}

int CPluginTesterDlg::GetItemWidth(IPluginItem* pItem, CDC* pDC)
{

    int width = 0;
    if (!IsDlgButtonChecked(IDC_SPECIFY_WIDTH_CHECK))
    {
        ITMPlugin* plugin = GetCurrentPlugin().plugin;
        if (plugin != nullptr && plugin->GetAPIVersion() >= 3)
        {
            width = pItem->GetItemWidthEx(pDC->GetSafeHdc());       //优先使用GetItemWidthEx接口获取宽度
        }
        if (width == 0)
        {
            width = theApp.DPI(pItem->GetItemWidth());
        }
    }
    else
    {
        CString str;
        GetDlgItemText(IDC_ITEM_WIDTH_EDIT, str);
        width = _ttoi(str.GetString());
    }
    return width;

}

bool CPluginTesterDlg::IsDarkmodeChecked() const
{
    return IsDlgButtonChecked(IDC_DARK_BACKGROUND_CHECK);
}


bool CPluginTesterDlg::IsDoubleLineChecked() const
{
    return IsDlgButtonChecked(IDC_DOUBLE_LINE_CHECK);
}


int CPluginTesterDlg::CalculatePreviewTopPos()
{
    int pos = 0;
    std::vector<UINT> control_id{ IDC_SELECT_PLUGIN_COMBO, IDC_OPTION_BUTTON, IDC_DETAIL_BUTTON, IDC_DARK_BACKGROUND_CHECK,
        IDC_SPECIFY_WIDTH_CHECK, IDC_ITEM_WIDTH_EDIT };
    for (const auto& id : control_id)
    {
        CWnd* control = GetDlgItem(id);
        if (control != nullptr)
        {
            CRect rect;
            control->GetWindowRect(rect);
            ScreenToClient(&rect);
            if (pos < rect.bottom)
                pos = rect.bottom;
        }
    }
    return pos + theApp.DPI(8);
}

CSize CPluginTesterDlg::CalculatePreviewSize()
{
    CSize size;
    size.cx = theApp.DPI(152);

    PluginInfo plugin_info = GetCurrentPlugin();
    int item_num = static_cast<int>(plugin_info.plugin_items.size());
    size.cy = theApp.DPI(16) + item_num * (IsDoubleLineChecked() ? theApp.DPI(64) : theApp.DPI(56));
    //if (IsDoubleLineChecked())
    //    size.cy += theApp.DPI(8);

    return size;
}


std::wstring CPluginTesterDlg::GetPluginDir()
{
    std::wstring plugin_dir;
    //选择了当前目录
    if (IsDlgButtonChecked(IDC_CUR_DIR_RADIO))
    {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(NULL, path, MAX_PATH);
        plugin_dir = utilities::CFilePathHelper(path).GetDir();
    }
    //选择了自定义目录
    else
    {
        CString str;
        GetDlgItemText(IDC_EDIT1, str);
        plugin_dir = str.GetString();
    }
    if (!plugin_dir.empty() && plugin_dir.back() != L'/' && plugin_dir.back() != L'\\')
        plugin_dir.push_back('\\');
    return plugin_dir;
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CPluginTesterDlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}


void CPluginTesterDlg::OnBass64Encode()
{
    //打开一个文件
    CFileDialog fileDlg(TRUE, NULL, NULL, OFN_ALLOWMULTISELECT, _T("(*.*)|*.*||"), this);
    if (fileDlg.DoModal() == IDOK)
    {
        CString file_name = fileDlg.GetPathName();
        std::string file_contents;
        utilities::CCommon::GetFileContent(file_name.GetString(), file_contents);
        std::string base64_contents = utilities::Base64Encode(file_contents);

        //保存文件
        CString file_name_base64 = file_name += _T(".base64.txt");
        std::ofstream file_stream(file_name_base64.GetString());
        file_stream << base64_contents;
        file_stream.close();
    }
}


void CPluginTesterDlg::OnBase64Decode()
{
    //打开一个文件
    CFileDialog fileDlg(TRUE, NULL, NULL, OFN_ALLOWMULTISELECT, _T("(*.*)|*.*||"), this);
    if (fileDlg.DoModal() == IDOK)
    {
        CString file_name = fileDlg.GetPathName();
        std::string file_contents;
        utilities::CCommon::GetFileContent(file_name.GetString(), file_contents);
        std::string contents_decoded = utilities::Base64Decode(file_contents);

        //保存文件
        CString file_name_decoded = file_name += _T(".decoded.txt");
        std::ofstream file_stream(file_name_decoded.GetString());
        file_stream << contents_decoded;
        file_stream.close();
    }
}


void CPluginTesterDlg::OnAppAbout()
{
    // TODO: 在此添加命令处理程序代码
    CAboutDlg dlg;
    dlg.DoModal();
}


void CPluginTesterDlg::OnCbnSelchangeSelectPluginCombo()
{
    m_cur_index = m_select_plugin_combo.GetCurSel();
    if (IsWindow(m_view->GetSafeHwnd()))
    {
        m_view->Invalidate();
        m_view->SetSize(CalculatePreviewSize());
    }
    EnableControl();

    //设置插件图标
    PluginInfo cur_plugin = GetCurrentPlugin();
    HICON hIcon{};
    if (cur_plugin.plugin != nullptr && cur_plugin.plugin->GetAPIVersion() >= 5)
    {
        hIcon = (HICON)cur_plugin.plugin->GetPluginIcon();
    }
    CButton* btn = (CButton*)(GetDlgItem(IDC_PLUGIN_COMMANDS_BUTTON));
    if (btn != nullptr)
        btn->SetIcon(hIcon);
}


void CPluginTesterDlg::OnBnClickedOptionButton()
{
    PluginInfo cur_plugin = GetCurrentPlugin();
    if (cur_plugin.plugin != nullptr)
    {
        ITMPlugin::OptionReturn rtn = cur_plugin.plugin->ShowOptionsDialog(m_hWnd);
    }
}


void CPluginTesterDlg::OnBnClickedDetailButton()
{
    CPluginInfoDlg dlg(GetCurrentPlugin());
    dlg.DoModal();
}


void CPluginTesterDlg::OnTimer(UINT_PTR nIDEvent)
{
    // TODO: 在此添加消息处理程序代码和/或调用默认值
    if (nIDEvent == TIMER_ID)
    {
        PluginInfo cur_plugin = GetCurrentPlugin();
        if (cur_plugin.plugin != nullptr)
        {
            cur_plugin.plugin->DataRequired();
            if (IsWindow(m_view->GetSafeHwnd()))
            {
                m_view->Invalidate(FALSE);
                m_view->UpdateMouseToolTip();
            }
        }
    }

    CDialog::OnTimer(nIDEvent);
}

void CPluginTesterDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialog::OnSize(nType, cx, cy);

    // TODO: 在此处添加消息处理程序代码
    if (IsWindow(m_view->GetSafeHwnd()))
    {
        CRect rect;
        GetClientRect(rect);
        rect.top = m_proview_top_pos;
        m_view->MoveWindow(rect);
    }
}


void CPluginTesterDlg::OnBnClickedDarkBackgroundCheck()
{
    // TODO: 在此添加控件通知处理程序代码
    if (IsWindow(m_view->GetSafeHwnd()))
        m_view->Invalidate();
}


void CPluginTesterDlg::OnBnClickedDoubleLineCheck()
{
    // TODO: 在此添加控件通知处理程序代码
    if (IsWindow(m_view->GetSafeHwnd()))
    {
        m_view->Invalidate();
        CSize view_size = CalculatePreviewSize();
        view_size.cx = m_view->GetSize().cx;        //切换双行显示时不改变视图的宽度
        m_view->SetSize(view_size);
    }
}


void CPluginTesterDlg::OnBnClickedCurDirRadio()
{
    EnableControl();
    ShowRestartMessage();
}


void CPluginTesterDlg::ShowRestartMessage()
{
    if (m_plugin_dir != GetPluginDir())
    {
        MessageBox(theApp.LoadText(IDS_RESTART_TO_CHANGE_PLUGIN_DIR_INFO), NULL, MB_ICONINFORMATION | MB_OK);
    }
}

void CPluginTesterDlg::OnBnClickedUserDefinedRadio()
{
    EnableControl();
    CString str;
    GetDlgItemText(IDC_EDIT1, str);
    if (!str.IsEmpty())
        ShowRestartMessage();
}


void CPluginTesterDlg::OnBnClickedBrowseButton()
{
    CFolderPickerDialog dlg;
    if (dlg.DoModal() == IDOK)
    {
        SetDlgItemText(IDC_EDIT1, dlg.GetPathName());
        ShowRestartMessage();
    }
}


void CPluginTesterDlg::OnDestroy()
{
    CDialog::OnDestroy();

    SaveConfig();
}


void CPluginTesterDlg::OnEnChangeEdit2()
{
    // TODO:  如果该控件是 RICHEDIT 控件，它将不
    // 发送此通知，除非重写 CDialog::OnInitDialog()
    // 函数并调用 CRichEditCtrl().SetEventMask()，
    // 同时将 ENM_CHANGE 标志“或”运算到掩码中。

    // TODO:  在此添加控件通知处理程序代码

    if (IsWindow(m_view->GetSafeHwnd()))
        m_view->Invalidate();

}


void CPluginTesterDlg::OnBnClickedSpecifyWidthCheck()
{
    // TODO: 在此添加控件通知处理程序代码
    if (IsWindow(m_view->GetSafeHwnd()))
        m_view->Invalidate();
    EnableControl();
}


void CPluginTesterDlg::OnLanguageChinese()
{
    if (theApp.m_language != Language::SIMPLIFIED_CHINESE)
    {
        theApp.m_language = Language::SIMPLIFIED_CHINESE;
        SaveConfig();
        MessageBox(theApp.LoadText(IDS_LANGUAGE_CHANGE), NULL, MB_ICONINFORMATION);
    }
}


void CPluginTesterDlg::OnLanguageEnglish()
{
    if (theApp.m_language != Language::ENGLISH)
    {
        theApp.m_language = Language::ENGLISH;
        SaveConfig();
        MessageBox(theApp.LoadText(IDS_LANGUAGE_CHANGE), NULL, MB_ICONINFORMATION);
    }
}


void CPluginTesterDlg::OnLanguageFollowingSystem()
{
    if (theApp.m_language != Language::FOLLOWING_SYSTEM)
    {
        theApp.m_language = Language::FOLLOWING_SYSTEM;
        SaveConfig();
        MessageBox(theApp.LoadText(IDS_LANGUAGE_CHANGE), NULL, MB_ICONINFORMATION);
    }
}


void CPluginTesterDlg::OnInitMenu(CMenu* pMenu)
{
    CDialog::OnInitMenu(pMenu);

    switch (theApp.m_language)
    {
    case Language::ENGLISH: pMenu->CheckMenuRadioItem(ID_LANGUAGE_FOLLOWING_SYSTEM, ID_LANGUAGE_ENGLISH, ID_LANGUAGE_ENGLISH, MF_BYCOMMAND | MF_CHECKED); break;
    case Language::SIMPLIFIED_CHINESE: pMenu->CheckMenuRadioItem(ID_LANGUAGE_FOLLOWING_SYSTEM, ID_LANGUAGE_ENGLISH, ID_LANGUAGE_CHINESE, MF_BYCOMMAND | MF_CHECKED); break;
    default: pMenu->CheckMenuRadioItem(ID_LANGUAGE_FOLLOWING_SYSTEM, ID_LANGUAGE_ENGLISH, ID_LANGUAGE_FOLLOWING_SYSTEM, MF_BYCOMMAND | MF_CHECKED); break;
    }

    //设置插件命令的勾选状态
    ITMPlugin* plugin = GetCurrentPlugin().plugin;
    if (plugin != nullptr && plugin->GetAPIVersion() >= 5)
    {
        for (int i = ID_PLUGIN_COMMAND_START; i <= ID_PLUGIN_COMMAND_MAX; i++)
        {
            bool checked = (plugin->IsCommandChecked(i - ID_PLUGIN_COMMAND_START) != 0);
            pMenu->CheckMenuItem(i, MF_BYCOMMAND | (checked ? MF_CHECKED : MF_UNCHECKED));
        }
    }
}


void CPluginTesterDlg::OnBnClickedPluginCommandsButton()
{
    // TODO: 在此添加控件通知处理程序代码
    PluginInfo cur_plugin = GetCurrentPlugin();
    if (cur_plugin.plugin != nullptr && cur_plugin.plugin->GetAPIVersion() >= 5)
    {
        int command_num = cur_plugin.plugin->GetCommandCount();
        if (command_num > 0)
        {
            //初始化菜单
            m_plugin_command_menu.DestroyMenu();
            m_plugin_command_menu.CreatePopupMenu();
            for (int i = 0; i < command_num; i++)
            {
                const wchar_t* cmd_name = cur_plugin.plugin->GetCommandName(i);
                m_plugin_command_menu.AppendMenu(MF_STRING | MF_ENABLED, ID_PLUGIN_COMMAND_START + i, cmd_name);
                HICON command_icon = (HICON)cur_plugin.plugin->GetCommandIcon(i);
                if (command_icon != nullptr)
                    CMenuIcon::AddIconToMenuItem(m_plugin_command_menu.m_hMenu, ID_PLUGIN_COMMAND_START + i, FALSE, command_icon);
            }
            //获取弹出菜单的位置
            CRect btn_rect;
            ::GetWindowRect(GetDlgItem(IDC_PLUGIN_COMMANDS_BUTTON)->GetSafeHwnd(), btn_rect);
            //弹出菜单
            m_plugin_command_menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, btn_rect.left, btn_rect.bottom, this); //在指定位置显示弹出菜单
        }
    }

}


BOOL CPluginTesterDlg::OnCommand(WPARAM wParam, LPARAM lParam)
{
    // TODO: 在此添加专用代码和/或调用基类
    UINT uMsg = LOWORD(wParam);
    //选择了插件命令
    if (uMsg >= ID_PLUGIN_COMMAND_START && uMsg <= ID_PLUGIN_COMMAND_MAX)
    {
        int index = uMsg - ID_PLUGIN_COMMAND_START;
        PluginInfo cur_plugin = GetCurrentPlugin();
        if (cur_plugin.plugin != nullptr && cur_plugin.plugin->GetAPIVersion() >= 5)
        {
            cur_plugin.plugin->OnPluginCommand(index, m_hWnd, nullptr);
        }
    }

    return CDialog::OnCommand(wParam, lParam);
}
