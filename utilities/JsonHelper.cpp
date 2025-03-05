#include "JsonHelper.h"
#include "Common.h"

namespace utilities
{
    std::string JsonHelper::GetJsonString(yyjson_val* obj, const char* key)
    {
        if (obj != nullptr)
        {
            yyjson_val* value = yyjson_obj_get(obj, key);
            if (value != nullptr)
            {
                const char* str = yyjson_get_str(value);
                if (str != nullptr)
                    return str;
            }
        }
        return std::string();
    }

    std::wstring JsonHelper::GetJsonWString(yyjson_val* obj, const char* key)
    {
        std::string str{ GetJsonString(obj, key) };
        return StringHelper::StrToUnicode(str.c_str(), true);
    }

    float JsonHelper::GetJsonFloat(yyjson_val* obj, const char* key)
    {
        if (obj != nullptr)
        {
            yyjson_val* value = yyjson_obj_get(obj, key);
            if (value != nullptr)
            {
                if (yyjson_is_real(value))
                {
                    return static_cast<float>(yyjson_get_real(value));
                }
            }
        }
        return 0.0f;
    }
}