#include "pch.h"
#include "DataQuerier.h"

#include "Common.h"
#include <afxinet.h>
#include <ctime>
#include <regex>
#include <sstream>
#include <map>

#include <utilities/yyjson/yyjson.h>

namespace query
{
    using wslist = std::vector<std::wstring>;
    using wsListList = std::vector<wslist>;

    wsListList _RegexExtractAll(const std::wstring &text, const std::wregex& pattern)
    {
        std::wsmatch matches;
        wsListList results;

        auto itr1 = text.cbegin();
        auto itr2 = text.cend();

        while (std::regex_search(itr1, itr2, matches, pattern))
        {
            wslist sub_matches;
            for (const auto &sm : matches)
                sub_matches.push_back(sm.str());

            results.push_back(sub_matches);

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
        static const std::wregex pattern{ L"\\{\"ref\":\"(.+?)\"\\}" };

        wslist results;
        try
        {
            auto match_results = _RegexExtractAll(s, pattern);
            for (const auto &m : match_results)
                if (m.size() > 1)
                    results.push_back(m[1]);
        }
        catch (const std::exception &)
        {
            results.clear();
        }
        
        return results;
    }

    bool _IsSpot(const std::wstring &sNO)
    {
        static const std::wregex pattern{ L"\\D+" };
        static std::wsmatch matches;

        bool result = true;
        try
        {
            result = std::regex_search(sNO, matches, pattern);
        }
        catch (const std::exception &)
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
                httpFile->Read(multi_byte_content_buffer, static_cast<UINT>(offset + 1));

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

        if (succeed && !content.empty())
        {
            // parse string
            auto infoList = _ExtractCityInfoList(content);

            for (const auto &info : infoList)
            {
                auto cityInfo = _ExtractCityInfo(info);
                if (!cityInfo.CityNO.empty())
                    city_list.push_back(cityInfo);
            }
        }

        return succeed;
    }

    bool _queryRealTimeWeatherCity(const std::wstring& city_code, SRealTimeWeather& rt_weather)
    {
        rt_weather = SRealTimeWeather();

        auto timeStamp = std::time(0);

        CString q_url;
        q_url.Format(L"http://d1.weather.com.cn/sk_2d/%s.html?_=%I64d", city_code.c_str(), timeStamp);

        CString qHeaders = L"Host: d1.weather.com.cn\r\nReferer: http://www.weather.com.cn/";

        std::wstring content;
        auto succeed = _CallInternet(q_url, qHeaders, content);

        if (content.find(L'{') == std::wstring::npos)
            succeed = false;

        if (succeed && !content.empty())
        {
            auto data = content.substr(content.find(L'{'));
            if (!data.empty())
            {
                auto data_utf8 = CCommon::UnicodeToStr(data.c_str(), true);

                yyjson_doc* doc = yyjson_read(data_utf8.c_str(), data_utf8.size(), 0);
                if (doc != nullptr)
                {
                    auto* root = yyjson_doc_get_root(doc);

                    auto get_value = [root](const char *key) {
                        auto* obj = yyjson_obj_get(root, key);
                        return CCommon::StrToUnicode(yyjson_get_str(obj), true);
                    };

                    rt_weather.Temperature = get_value("temp");
                    rt_weather.UpdateTime = get_value("time");
                    rt_weather.Weather = get_value("weather");
                    rt_weather.WeatherCode = get_value("weathercode");
                    rt_weather.AqiPM25 = get_value("aqi_pm25");
                    rt_weather.WindDirection = get_value("WD");
                    rt_weather.WindStrength = get_value("WS");

                    // correct weather code to 'night' if time during 20:00 to 06:00
                    auto h = std::stoi(rt_weather.UpdateTime.substr(0, 2));
                    if (h >= 20 || h < 6)
                        rt_weather.WeatherCode[0] = L'n';
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

    std::wstring _convertWeatherNameToCode(const std::wstring& w_name)
    {
        static const std::map<std::wstring, std::wstring> d_map{
            {L"晴", L"d00"},
            {L"多云", L"d01"},
            {L"阴", L"d02"},
            {L"阵雨", L"d03"},
            {L"雷阵雨", L"d04"},
            {L"雷阵雨伴有冰雹", L"d05"},
            {L"雨夹雪", L"d06"},
            {L"小雨", L"d07"},
            {L"中雨", L"d08"},
            {L"大雨", L"d09"},
            {L"暴雨", L"d10"},
            {L"大暴雨", L"d11"},
            {L"特大暴雨", L"d12"},
            {L"阵雪", L"d13"},
            {L"小雪", L"d14"},
            {L"中雪", L"d15"},
            {L"大雪", L"d16"},
            {L"暴雪", L"d17"},
            {L"雾", L"d18"},
            {L"冻雨", L"d19"},
            {L"沙尘暴", L"d20"},
            {L"小到中雨", L"d21"},
            {L"中到大雨", L"d22"},
            {L"大到暴雨", L"d23"},
            {L"暴雨到大暴雨", L"d24"},
            {L"大暴雨到特大暴雨", L"d25"},
            {L"小到中雪", L"d26"},
            {L"中到大雪", L"d27"},
            {L"大到暴雪", L"d28"},
            {L"浮尘", L"d29"},
            {L"扬沙", L"d30"},
            {L"强沙尘暴", L"d31"},
            {L"霾", L"d53"},
            {L"无", L"d99"},
            {L"浓雾", L"d32"},
            {L"强浓雾", L"d49"},
            {L"中度霾", L"d54"},
            {L"重度霾", L"d55"},
            {L"严重霾", L"d56"},
            {L"大雾", L"d57"},
            {L"特强浓雾", L"d58"},
            {L"雨", L"d97"},
            {L"雪", L"d98"},
        };

        auto itr = d_map.find(w_name);
        if (itr != d_map.end())
            return itr->second;
        else
            return L"d99";
    }

    bool _queryRealTimeWeatherTownOrStreet(const std::wstring& code, SRealTimeWeather& rt_weather)
    {
        rt_weather = SRealTimeWeather();

        CString q_url;
        q_url.Format(L"http://forecast.weather.com.cn/town/weather1dn/%s.shtml", code.c_str());

        CString qHeaders = L"Host: forecast.weather.com.cn\r\nReferer: http://www.weather.com.cn/";

        std::wstring content;
        auto succeed = _CallInternet(q_url, qHeaders, content);

        if (succeed && !content.empty())
        {
            static const std::wregex pattern{ L"var forecast_default.*?(?=;)" };

            auto data_list = _RegexExtractAll(content, pattern);
            if (!data_list.empty())
            {
                const auto &data_str = data_list[0][0];
                if (data_str.find(L'{') != std::wstring::npos)
                {
                    auto data = data_str.substr(data_str.find(L'{'));

                    auto data_utf8 = CCommon::UnicodeToStr(data.c_str(), true);

                    yyjson_doc* doc = yyjson_read(data_utf8.c_str(), data_utf8.size(), 0);
                    if (doc != nullptr)
                    {
                        auto* root = yyjson_doc_get_root(doc);

                        auto get_str_value = [root](const char *key) {
                            auto* obj = yyjson_obj_get(root, key);
                            return CCommon::StrToUnicode(yyjson_get_str(obj), true);
                        };

                        auto get_int_value = [root](const char *key) {
                            auto* obj = yyjson_obj_get(root, key);
                            return yyjson_get_int(obj);
                        };

                        rt_weather.Temperature = std::to_wstring(get_int_value("temp"));
                        rt_weather.UpdateTime = get_str_value("time");
                        rt_weather.Weather = get_str_value("weather");

                        rt_weather.WeatherCode = _convertWeatherNameToCode(rt_weather.Weather);
                        // correct weather code to 'night' if time during 20:00 to 06:00
                        auto h = std::stoi(rt_weather.UpdateTime.substr(0, 2));
                        if (h >= 20 || h < 6)
                            rt_weather.WeatherCode[0] = L'n';

                        rt_weather.AqiPM25 = L"--";

                        auto wind_ds = get_str_value("wind");
                        auto idx = wind_ds.find(L' ');
                        if (idx != std::wstring::npos)
                        {
                            rt_weather.WindDirection = wind_ds.substr(0, idx);
                            rt_weather.WindStrength = wind_ds.substr(idx + 1);
                        }
                        else
                            rt_weather.WindDirection = wind_ds;
                    }
                    else
                        succeed = false;
                }
                else
                    succeed = false;
            }
            else
                succeed = false;
        }

        return succeed;
    }

    bool QueryRealTimeWeather(const std::wstring &code, SRealTimeWeather &rt_weather)
    {
        if (code.size() == 9)
            return _queryRealTimeWeatherCity(code, rt_weather);
        else
            return _queryRealTimeWeatherTownOrStreet(code, rt_weather);
    }

    bool QueryWeatherAlerts(const std::wstring &code, WeatherAlertList &alerts)
    {
        alerts.clear();

        std::wstring target_code{ code };
        if (code.size() > 9)
            target_code = code.substr(0, 9);

        auto timeStamp = std::time(0);

        CString q_url;
        q_url.Format(L"http://d1.weather.com.cn/dingzhi/%s.html?_=%I64d", target_code.c_str(), timeStamp);

        CString qHeaders = L"Host: d1.weather.com.cn\r\nReferer: http://www.weather.com.cn/";

        std::wstring content;
        auto succeed = _CallInternet(q_url, qHeaders, content);

        auto data_idx = content.find(L"var alarmDZ");
        if (data_idx == std::wstring::npos)
            succeed = false;
        else
        {
            content = content.substr(data_idx);
            if (content.find(L'{') == std::wstring::npos)
                succeed = false;
        }

        if (succeed && !content.empty())
        {
            auto data = content.substr(content.find(L'{'));

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

    bool QueryForecastWeather(const std::wstring &code, SWeatherInfo &weather_td, SWeatherInfo &weather_tm)
    {
        weather_td = SWeatherInfo();
        weather_tm = SWeatherInfo();

        CString q_url;
        CString qHeaders;

        if (code.size() == 9)
        {
            q_url.Format(L"http://www.weather.com.cn/weathern/%s.shtml", code.c_str());
            qHeaders.Format(L"Host: www.weather.com.cn\r\nReferer: http://www.weather.com.cn/weather1dn/%s.shtml", code.c_str());
        }
        else
        {
            q_url.Format(L"http://forecast.weather.com.cn/town/weathern/%s.shtml", code.c_str());
            qHeaders.Format(L"Host: forecast.weather.com.cn\r\nReferer: http://forecast.weather.com.cn/town/weather1dn/%s.shtml", code.c_str());
        }

        std::wstring content;
        auto succeed = _CallInternet(q_url, qHeaders, content);

        if (succeed && !content.empty())
        {
            // 解析温度数据
            static const std::wregex pattern_temp_day{ L"var eventDay.*?(?=;)" };
            static const std::wregex pattern_temp_night{ L"var eventNight.*?(?=;)" };
            static const std::wregex pattern_temp_values{ LR"((-?\d+))" };

            {
                auto match_results = _RegexExtractAll(content, pattern_temp_day);
                if (!match_results.empty())
                {
                    const auto &temp_day_str = match_results[0][0];
                    auto temp_values = _RegexExtractAll(temp_day_str, pattern_temp_values);
                    if (temp_values.size() >= 3)
                    {
                        weather_td.TemperatureDay = temp_values[1][1];
                        weather_tm.TemperatureDay = temp_values[2][1];
                    }
                    else
                        succeed = false;
                }
                else
                    succeed = false;
            }

            if (succeed)
            {
                auto match_results = _RegexExtractAll(content, pattern_temp_night);
                if (!match_results.empty())
                {
                    const auto &temp_night_str = match_results[0][0];
                    auto temp_values = _RegexExtractAll(temp_night_str, pattern_temp_values);
                    if (temp_values.size() >= 3)
                    {
                        weather_td.TemperatureNight = temp_values[1][1];
                        weather_tm.TemperatureNight = temp_values[2][1];
                    }
                    else
                        succeed = false;
                }
                else
                    succeed = false;
            }

            // 解析天气
            static const std::wregex pattern_weather{ LR"(<i class=\"item-icon ([dn]\d+) icons_bg" title=\"(.+?)\">)" };

            if (succeed)
            {
                auto match_results = _RegexExtractAll(content, pattern_weather);
                if (match_results.size() >= 4)
                {
                    weather_td.WeatherCodeDay = match_results[0][1];
                    weather_td.WeatherDay = match_results[0][2];

                    weather_td.WeatherCodeNight = match_results[1][1];
                    weather_td.WeatherNight = match_results[1][2];

                    weather_tm.WeatherCodeDay = match_results[2][1];
                    weather_tm.WeatherDay = match_results[2][2];

                    weather_tm.WeatherCodeNight = match_results[3][1];
                    weather_tm.WeatherNight = match_results[3][2];
                }
                else
                    succeed = false;
            }
        }

        return succeed;
    }
}

std::wstring SRealTimeWeather::ToString(bool brief) const
{
    std::wstringstream wss;

    wss << Weather << " " << Temperature << L"℃ (" << UpdateTime << L')';
    if (!brief)
    {
        wss << L" PM2.5: " << AqiPM25 << "  " << WindDirection << WindStrength;
    }

    return wss.str();
}

std::wstring SRealTimeWeather::GetTemperature() const
{
    return Temperature + L"℃";
}

std::wstring SWeatherAlert::ToString(bool brief) const
{
    std::wstringstream wss;

    if (brief)
    {
        wss << BriefMessage << L" (" << PublishTime << L')';
    }
    else
    {
        wss << DetailedMessage;
    }

    return wss.str();
}

std::wstring SWeatherInfo::ToString() const
{
    std::wstringstream wss;

    if (WeatherDay == WeatherNight)
        wss << WeatherDay << L' ';
    else
        wss << WeatherDay << L"转" << WeatherNight << L' ';

    wss << TemperatureNight << L"~" << TemperatureDay << L"℃";

    return wss.str();
}

std::wstring SWeatherInfo::GetTemperature() const
{
    std::wstringstream wss;
    wss << TemperatureNight << L"~" << TemperatureDay << L"℃";

    return wss.str();
}
