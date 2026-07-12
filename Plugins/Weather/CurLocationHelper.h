#pragma once
#include <string>

class CCurLocationHelper
{
public:
    CCurLocationHelper();
    ~CCurLocationHelper();

    static std::wstring GetCurrentCity();
    static int FindCityCodeItem(std::wstring city_name);

};

