#include "pch.h"
#include "DataQuerier.h"

#include "Common.h"
#include <afxinet.h>
#include <ctime>
#include <regex>

namespace query
{
    using wslist = std::vector<std::wstring>;

    wslist _ExtractCityInfoList(const std::wstring &s)
    {
        static std::wregex pattern{ L"\\{\"ref\":\"(.+?)\"\\}" };
        static std::wsmatch matches;

        wslist results;

        auto itr1 = s.cbegin();
        auto itr2 = s.cend();
        while (std::regex_search(itr1, itr2, matches, pattern))
        {
            if (!matches.empty())
                results.push_back(matches[1].str());

            itr1 = matches[0].second;
        }

        return results;
    }

    bool _IsSpot(const std::wstring &sNO)
    {
        static std::wregex pattern{ L"\\D+" };
        static std::wsmatch matches;

        return std::regex_search(sNO, matches, pattern);
    }

    SCityInfo _ExtractCityInfo(const std::wstring &s)
    {
        std::size_t pos1{ 0 }, pos2{ std::wstring::npos };

        wslist infoFields;
        do 
        {
            pos2 = s.find_first_of(L'~', pos1);
            infoFields.push_back(s.substr(pos1, pos2 - pos1));

            pos1 = pos2 + 1;
        } while (pos2 != std::wstring::npos);

        SCityInfo result;

        if (!_IsSpot(infoFields[0]))
        {
            result.CityNO = CString(infoFields[0].c_str());
            result.CityName = CString(infoFields[2].c_str());
            result.CityAdministrativeOwnership.Format(L"%s-%s-%s", infoFields[2].c_str(), infoFields[4].c_str(), infoFields[9].c_str());
        }

        return result;
    }

    CityInfoList QueryCityInfo(const CString &qName)
    {
        auto qNameEncoded = CCommon::URLEncode(std::wstring(qName));
        auto timeStamp = std::time(0);

        CString q_url;
        q_url.Format(L"http://toy1.weather.com.cn/search?cityname=%s&callback=success_jsonpCallback&_=%I64d",
                     qNameEncoded.c_str(), timeStamp);

        CString qHeaders = L"Host: toy1.weather.com.cn\r\nReferer: http://www.weather.com.cn/";
        CString qAgent = L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/99.0.4844.51 Safari/537.36 Edg/99.0.1150.39";

        bool succeed = false;

        CInternetSession *session{ nullptr };
        CHttpFile *httpFile{ nullptr };

        char *multi_byte_content_buffer{ nullptr };

        try
        {
            session = new CInternetSession(qAgent);
            httpFile = (CHttpFile*)session->OpenURL(q_url, 1, INTERNET_FLAG_TRANSFER_ASCII, qHeaders);
            DWORD dwStatusCode;
            httpFile->QueryInfoStatusCode(dwStatusCode);
            if (dwStatusCode == HTTP_STATUS_OK)
            {
                auto offset = httpFile->Seek(0, CFile::end);
                multi_byte_content_buffer = new char[offset + 1]{ 0 };

                httpFile->Seek(0, CFile::begin);
                httpFile->Read(multi_byte_content_buffer, offset + 1);

                succeed = true;
            }

            httpFile->Close();
            session->Close();
        }
        catch (CInternetException* e)
        {
            if (httpFile != nullptr) httpFile->Close();
            if (session != nullptr) session->Close();

            succeed = false;
            e->Delete();
        }

        auto content_wstr = CCommon::StrToUnicode(multi_byte_content_buffer, true);
        CString content(content_wstr.c_str());

        delete[] multi_byte_content_buffer;
        multi_byte_content_buffer = nullptr;

        delete httpFile;
        httpFile = nullptr;

        delete session;
        session = nullptr;

        CityInfoList cityInfoList;
        if (succeed && !content.IsEmpty())
        {
            // parse string
            auto infoList = _ExtractCityInfoList(std::wstring(content));

            for (const auto &info : infoList)
            {
                auto cityInfo = _ExtractCityInfo(info);
                if (!cityInfo.CityNO.IsEmpty())
                    cityInfoList.push_back(cityInfo);
            }
        }

        return cityInfoList;
    }
}
