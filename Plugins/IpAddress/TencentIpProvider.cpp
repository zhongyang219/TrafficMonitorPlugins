#include "pch.h"
#include "TencentIpProvider.h"
#include <wininet.h>
#include <string>
#include "../utilities/yyjson/yyjson.h"
#include "../utilities/JsonHelper.h"

std::wstring CTencentIpProvider::GetName() const
{
    return L"Tencent";
}

bool CTencentIpProvider::GetExternalIp(std::wstring& ip) const
{
    bool result = false;
    HINTERNET hInternet = InternetOpen(L"TrafficMonitor", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (hInternet)
    {
        HINTERNET hUrl = InternetOpenUrl(hInternet, L"https://vv.video.qq.com/checktime?otype=ojson", NULL, 0, INTERNET_FLAG_RELOAD, 0);
        if (hUrl)
        {
            char buffer[1024];
            DWORD bytesRead = 0;
            std::string response;
            while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0)
            {
                response.append(buffer, bytesRead);
            }

            yyjson_doc* doc = yyjson_read(response.c_str(), response.length(), 0);
            if (doc)
            {
                yyjson_val* root = yyjson_doc_get_root(doc);
                ip = utilities::JsonHelper::GetJsonWString(root, "ip");
                if (!ip.empty())
                {
                    result = true;
                }
                yyjson_doc_free(doc);
            }
            InternetCloseHandle(hUrl);
        }
        InternetCloseHandle(hInternet);
    }
    return result;
}
