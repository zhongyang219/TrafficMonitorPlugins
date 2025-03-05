#pragma once
#include "yyjson/yyjson.h"
#include "Variant.h"

namespace utilities
{
    class JsonHelper
    {
    public:
        static std::string GetJsonString(yyjson_val* obj, const char* key);
        static std::wstring GetJsonWString(yyjson_val* obj, const char* key);
        static float GetJsonFloat(yyjson_val* obj, const char* key);
    };
}

