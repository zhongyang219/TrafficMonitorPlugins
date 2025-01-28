
// PluginTesterDlg.h : ͷ�ļ�
//

#pragma once
#include "afxwin.h"
#include "CommonData.h"
#include <map>
#include "PreviewScrollView.h"

// CPluginTesterDlg �Ի���
class CPluginTesterDlg : public CDialog
{
// ����
public:
    CPluginTesterDlg(CWnd* pParent = NULL);	// ��׼���캯��

    PluginInfo GetCurrentPlugin();
    int GetItemWidth(IPluginItem* pItem, CDC* pDC);
    bool IsDarkmodeChecked() const;
    bool IsDoubleLineChecked() const;

// �Ի�������
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_PLUGINTESTER_DIALOG };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV ֧��

private:
    PluginInfo LoadPlugin(const std::wstring& plugin_file_path, const std::wstring& config_dir);
    void EnableControl();
    int CalculatePreviewTopPos();       //����Ԥ��ͼ����λ��
    CSize CalculatePreviewSize();       //����Ԥ������Ĵ�С
    std::wstring GetPluginDir();             //��ȡ���Ŀ¼

// ʵ��
protected:
    HICON m_hIcon;
    std::wstring m_plugin_dir;
    std::vector<PluginInfo> m_plugins;
    int m_cur_index{ -1 };
    CDrawScrollView* m_view{};    //Ԥ������ͼ��
    int m_proview_top_pos{};
    CMenu m_plugin_command_menu;

    // ���ɵ���Ϣӳ�亯��
    virtual BOOL OnInitDialog();

    void InitPlugins();

    void LoadConfig();
    void SaveConfig() const;

    void ShowRestartMessage();

    afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
    afx_msg HCURSOR OnQueryDragIcon();
    DECLARE_MESSAGE_MAP()
public:
    afx_msg void OnBass64Encode();
    afx_msg void OnBase64Decode();
    afx_msg void OnAppAbout();
    afx_msg void OnCbnSelchangeSelectPluginCombo();
    afx_msg void OnBnClickedOptionButton();
    CComboBox m_select_plugin_combo;
    afx_msg void OnBnClickedDetailButton();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnBnClickedDarkBackgroundCheck();
    afx_msg void OnBnClickedDoubleLineCheck();
    afx_msg void OnBnClickedCurDirRadio();
    afx_msg void OnBnClickedUserDefinedRadio();
    afx_msg void OnBnClickedBrowseButton();
    afx_msg void OnDestroy();
    afx_msg void OnEnChangeEdit2();
    afx_msg void OnBnClickedSpecifyWidthCheck();
    afx_msg void OnLanguageChinese();
    afx_msg void OnLanguageEnglish();
    afx_msg void OnLanguageRussian();
    afx_msg void OnLanguageFollowingSystem();
    afx_msg void OnInitMenu(CMenu* pMenu);
    afx_msg void OnBnClickedPluginCommandsButton();
    virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
};
