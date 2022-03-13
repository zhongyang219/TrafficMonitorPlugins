#pragma once
#include <map>

class CDataManager
{
private:
    CDataManager();
    ~CDataManager();
public:
    static CDataManager& Instance();

    const CString& StringRes(UINT id);

    int DPI(int pixel) const;
    float DPIF(float pixel) const;
    int RDPI(int pixel) const;

private:
    static CDataManager m_instance;
    int m_dpi;
    std::map<UINT, CString> m_string_resources;
};
