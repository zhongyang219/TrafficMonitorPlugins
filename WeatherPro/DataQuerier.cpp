#include "pch.h"
#include "DataQuerier.h"

#include "Common.h"
#include <afxinet.h>
#include <ctime>
#include <regex>

#include <utilities/yyjson/yyjson.h>

namespace query
{
    using wslist = std::vector<std::wstring>;

    wslist _RegexExtractAll(const std::wstring &text, const std::wregex& pattern)
    {
        std::wsmatch matches;
        wslist results;

        auto itr1 = text.cbegin();
        auto itr2 = text.cend();

        while (std::regex_search(itr1, itr2, matches, pattern))
        {
            if (!matches.empty())
                results.push_back(matches[1].str());

            itr1 = matches[0].second;
        }

        return results;
    }

    wslist _StringSplit(const std::wstring &text, wchar_t sp)
    {
        std::size_t pos1{ 0 }, pos2{ std::wstring::npos };

        wslist results;
        do
        {
            pos2 = text.find_first_of(sp, pos1);
            results.push_back(text.substr(pos1, pos2 - pos1));

            pos1 = pos2 + 1;
        } while (pos2 != std::wstring::npos);

        return results;
    }

    wslist _ExtractCityInfoList(const std::wstring &s)
    {
        static std::wregex pattern{ L"\\{\"ref\":\"(.+?)\"\\}" };

        wslist results;
        try
        {
            results = _RegexExtractAll(s, pattern);
        }
        catch (const std::exception &e)
        {
            results.clear();
        }
        
        return results;
    }

    bool _IsSpot(const std::wstring &sNO)
    {
        static std::wregex pattern{ L"\\D+" };
        static std::wsmatch matches;

        bool result = true;
        try
        {
            result = std::regex_search(sNO, matches, pattern);
        }
        catch (const std::exception &e)
        {
            result = true;
        }

        return result;
    }

    SCityInfo _ExtractCityInfo(const std::wstring &s)
    {
        SCityInfo result;

        wslist infoFields = _StringSplit(s, L'~');
        if (infoFields.empty()) return result;

        if (!_IsSpot(infoFields[0]))
        {
            result.CityNO = infoFields[0];
            result.CityName = infoFields[2];

            if (infoFields[2] == infoFields[4])
                result.CityAdministrativeOwnership = infoFields[2] + L"-" + infoFields[9];
            else
                result.CityAdministrativeOwnership = infoFields[2] + L"-" + infoFields[4] + L"-" + infoFields[9];
        }

        return result;
    }

    bool _CallInternet(const CString &url, const CString &headers,
                       std::wstring &content)
    {
        content.clear();

        CString agent = L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/99.0.4844.51 Safari/537.36 Edg/99.0.1150.39";

        bool succeed = false;

        CInternetSession *session{ nullptr };
        CHttpFile *httpFile{ nullptr };

        char *multi_byte_content_buffer{ nullptr };

        try
        {
            session = new CInternetSession(agent);
            httpFile = (CHttpFile*)session->OpenURL(url, 1, INTERNET_FLAG_TRANSFER_ASCII, headers);
            DWORD dwStatusCode;
            httpFile->QueryInfoStatusCode(dwStatusCode);
            if (dwStatusCode == HTTP_STATUS_OK)
            {
                auto offset = httpFile->Seek(0, CFile::end);
                multi_byte_content_buffer = new char[offset + 1]{ 0 };

                httpFile->Seek(0, CFile::begin);
                httpFile->Read(multi_byte_content_buffer, offset + 1);

                content = CCommon::StrToUnicode(multi_byte_content_buffer, true);
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

        delete[] multi_byte_content_buffer;
        multi_byte_content_buffer = nullptr;

        delete httpFile;
        httpFile = nullptr;

        delete session;
        session = nullptr;

        return succeed;
    }

    bool QueryCityInfo(const std::wstring &qName, CityInfoList &city_list)
    {
        city_list.clear();

        auto qNameEncoded = CCommon::URLEncode(qName);
        auto timeStamp = std::time(0);

        CString q_url;
        q_url.Format(L"http://toy1.weather.com.cn/search?cityname=%s&callback=success_jsonpCallback&_=%I64d",
                     qNameEncoded.c_str(), timeStamp);

        CString qHeaders = L"Host: toy1.weather.com.cn\r\nReferer: http://www.weather.com.cn/";

        std::wstring content;
        bool succeed = _CallInternet(q_url, qHeaders, content);

        CityInfoList cityInfoList;
        if (succeed && !content.empty())
        {
            // parse string
            auto infoList = _ExtractCityInfoList(content);

            for (const auto &info : infoList)
            {
                auto cityInfo = _ExtractCityInfo(info);
                if (!cityInfo.CityNO.empty())
                    cityInfoList.push_back(cityInfo);
            }
        }

        return succeed;
    }

    bool QueryRealTimeWeather(const std::wstring &city_code, SRealTimeWeather &rt_weather)
    {
        rt_weather = SRealTimeWeather();

        auto timeStamp = std::time(0);

        CString q_url;
        q_url.Format(L"http://d1.weather.com.cn/sk_2d/%s.html?_=%I64d", city_code.c_str(), timeStamp);

        CString qHeaders = L"Host: d1.weather.com.cn\r\nReferer: http://www.weather.com.cn/";

        std::wstring content;
        auto succeed = _CallInternet(q_url, qHeaders, content);

        if (succeed && !content.empty())
        {
            auto data = content.substr(content.find(L'{'));
            if (!data.empty())
            {
                auto data_utf8 = CCommon::UnicodeToStr(data.c_str(), true);

                yyjson_doc *doc = yyjson_read(data_utf8.c_str(), data_utf8.size(), 0);
                if (doc != nullptr)
                {
                    auto *root = yyjson_doc_get_root(doc);

                    auto get_value = [root](const std::string &key) {
                        auto *obj = yyjson_obj_get(root, key.c_str());
                        return CCommon::StrToUnicode(yyjson_get_str(obj), true);
                    };

                    rt_weather.Temperature = get_value("temp");
                    rt_weather.UpdateTime = get_value("time");
                    rt_weather.Weather = get_value("weather");
                    rt_weather.WeatherCode = get_value("weathercode");
                    rt_weather.AqiPM25 = get_value("aqi_pm25");
                    rt_weather.WindDirection = get_value("WD");
                    rt_weather.WindStrength = get_value("WS");
                }
                else
                    succeed = false;

                yyjson_doc_free(doc);
            }
            else
                succeed = false;
        }

        return succeed;
    }

    bool QueryWeatherAlerts(const std::wstring &city_code, WeatherAlertList &alerts)
    {
        alerts.clear();

        auto timeStamp = std::time(0);

        CString q_url;
        q_url.Format(L"http://d1.weather.com.cn/dingzhi/%s.html?_=%I64d", city_code.c_str(), timeStamp);

        CString qHeaders = L"Host: d1.weather.com.cn\r\nReferer: http://www.weather.com.cn/";

        std::wstring content;
        auto succeed = _CallInternet(q_url, qHeaders, content);

        if (succeed && !content.empty())
        {
            auto data = content.substr(content.find(L"var alarmDZ"));
            data = data.substr(data.find(L'{'));

            auto data_utf8 = CCommon::UnicodeToStr(data.c_str(), true);

            auto *jdoc = yyjson_read(data_utf8.c_str(), data_utf8.size(), 0);
            if (jdoc != nullptr)
            {
                auto *jroot = yyjson_doc_get_root(jdoc);
                auto *jalert_arr = yyjson_obj_get(jroot, "w");

                auto num_alerts = yyjson_arr_size(jalert_arr);
                if (num_alerts > 0)
                {
                    auto get_value = [](yyjson_val *jval, const char *key) {
                        auto *obj = yyjson_obj_get(jval, key);
                        return CCommon::StrToUnicode(yyjson_get_str(obj), true);
                    };

                    auto get_alert = [get_value](yyjson_val *jval) {
                        SWeatherAlert alert;
                        alert.Type = get_value(jval, "w5");
                        alert.Level = get_value(jval, "w7");
                        alert.BriefMessage = get_value(jval, "w13");
                        alert.DetailedMessage = get_value(jval, "w9");
                        alert.PublishTime = get_value(jval, "w8");

                        return alert;
                    };

                    for (decltype(num_alerts) i = 0; i < num_alerts; ++i)
                        alerts.push_back(get_alert(yyjson_arr_get(jalert_arr, i)));
                }
            }
            else
                succeed = false;

            yyjson_doc_free(jdoc);
        }

        return succeed;
    }
}
