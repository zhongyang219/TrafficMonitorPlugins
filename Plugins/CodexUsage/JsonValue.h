#pragma once

#include <map>
#include <cwchar>
#include <string>
#include <vector>

// Small, dependency-free JSON reader for the handful of fields used by the plugin.
struct JsonValue
{
    enum class Type { Null, Boolean, Number, String, Object, Array };
    Type type{ Type::Null };
    bool boolean{};
    double number{};
    std::wstring string;
    std::map<std::wstring, JsonValue> object;
    std::vector<JsonValue> array;

    const JsonValue* Get(const wchar_t* key) const
    {
        if (type != Type::Object || key == nullptr) return nullptr;
        const auto it = object.find(key);
        return it == object.end() ? nullptr : &it->second;
    }
    std::wstring String(const wchar_t* fallback = L"") const
    {
        return type == Type::String ? string : fallback;
    }
    double Number(double fallback = 0.0) const
    {
        return type == Type::Number ? number : fallback;
    }
};

class JsonParser
{
public:
    explicit JsonParser(const std::wstring& input) : m_input(input) {}

    bool Parse(JsonValue& result)
    {
        SkipSpace();
        if (!Value(result, 0)) return false;
        SkipSpace();
        return m_pos == m_input.size();
    }

private:
    void SkipSpace()
    {
        while (m_pos < m_input.size() && (m_input[m_pos] == L' ' || m_input[m_pos] == L'\t' ||
            m_input[m_pos] == L'\r' || m_input[m_pos] == L'\n')) ++m_pos;
    }

    bool Value(JsonValue& out, unsigned depth)
    {
        if (depth > 96) return false;
        SkipSpace();
        if (m_pos >= m_input.size()) return false;
        const wchar_t ch = m_input[m_pos];
        if (ch == L'{') return Object(out, depth + 1);
        if (ch == L'[') return Array(out, depth + 1);
        if (ch == L'"')
        {
            out.type = JsonValue::Type::String;
            return String(out.string);
        }
        if (ch == L't' && Literal(L"true")) { out.type = JsonValue::Type::Boolean; out.boolean = true; return true; }
        if (ch == L'f' && Literal(L"false")) { out.type = JsonValue::Type::Boolean; out.boolean = false; return true; }
        if (ch == L'n' && Literal(L"null")) { out.type = JsonValue::Type::Null; return true; }
        return Number(out);
    }

    bool Literal(const wchar_t* literal)
    {
        size_t i = 0;
        while (literal[i] != 0)
        {
            if (m_pos + i >= m_input.size() || m_input[m_pos + i] != literal[i]) return false;
            ++i;
        }
        m_pos += i;
        return true;
    }

    static void AppendCodepoint(std::wstring& out, unsigned codepoint)
    {
        if (codepoint <= 0xFFFF)
            out.push_back(static_cast<wchar_t>(codepoint));
        else
        {
            codepoint -= 0x10000;
            out.push_back(static_cast<wchar_t>(0xD800 + (codepoint >> 10)));
            out.push_back(static_cast<wchar_t>(0xDC00 + (codepoint & 0x3FF)));
        }
    }

    bool Hex4(unsigned& value)
    {
        if (m_pos + 4 > m_input.size()) return false;
        value = 0;
        for (int i = 0; i < 4; ++i)
        {
            const wchar_t ch = m_input[m_pos++];
            value <<= 4;
            if (ch >= L'0' && ch <= L'9') value += ch - L'0';
            else if (ch >= L'a' && ch <= L'f') value += ch - L'a' + 10;
            else if (ch >= L'A' && ch <= L'F') value += ch - L'A' + 10;
            else return false;
        }
        return true;
    }

    bool String(std::wstring& out)
    {
        if (m_input[m_pos++] != L'"') return false;
        while (m_pos < m_input.size())
        {
            wchar_t ch = m_input[m_pos++];
            if (ch == L'"') return true;
            if (ch < 0x20) return false;
            if (ch != L'\\') { out.push_back(ch); continue; }
            if (m_pos >= m_input.size()) return false;
            ch = m_input[m_pos++];
            switch (ch)
            {
            case L'"': out.push_back(L'"'); break;
            case L'\\': out.push_back(L'\\'); break;
            case L'/': out.push_back(L'/'); break;
            case L'b': out.push_back(L'\b'); break;
            case L'f': out.push_back(L'\f'); break;
            case L'n': out.push_back(L'\n'); break;
            case L'r': out.push_back(L'\r'); break;
            case L't': out.push_back(L'\t'); break;
            case L'u':
            {
                unsigned codepoint{};
                if (!Hex4(codepoint)) return false;
                if (codepoint >= 0xD800 && codepoint <= 0xDBFF && m_pos + 6 <= m_input.size() &&
                    m_input[m_pos] == L'\\' && m_input[m_pos + 1] == L'u')
                {
                    m_pos += 2;
                    unsigned low{};
                    if (!Hex4(low) || low < 0xDC00 || low > 0xDFFF) return false;
                    codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
                }
                AppendCodepoint(out, codepoint);
                break;
            }
            default: return false;
            }
        }
        return false;
    }

    bool Object(JsonValue& out, unsigned depth)
    {
        ++m_pos;
        out.type = JsonValue::Type::Object;
        SkipSpace();
        if (m_pos < m_input.size() && m_input[m_pos] == L'}') { ++m_pos; return true; }
        for (;;)
        {
            SkipSpace();
            if (m_pos >= m_input.size() || m_input[m_pos] != L'"') return false;
            std::wstring key;
            if (!String(key)) return false;
            SkipSpace();
            if (m_pos >= m_input.size() || m_input[m_pos++] != L':') return false;
            JsonValue value;
            if (!Value(value, depth)) return false;
            out.object[std::move(key)] = std::move(value);
            SkipSpace();
            if (m_pos >= m_input.size()) return false;
            const wchar_t sep = m_input[m_pos++];
            if (sep == L'}') return true;
            if (sep != L',') return false;
        }
    }

    bool Array(JsonValue& out, unsigned depth)
    {
        ++m_pos;
        out.type = JsonValue::Type::Array;
        SkipSpace();
        if (m_pos < m_input.size() && m_input[m_pos] == L']') { ++m_pos; return true; }
        for (;;)
        {
            JsonValue value;
            if (!Value(value, depth)) return false;
            out.array.push_back(std::move(value));
            SkipSpace();
            if (m_pos >= m_input.size()) return false;
            const wchar_t sep = m_input[m_pos++];
            if (sep == L']') return true;
            if (sep != L',') return false;
        }
    }

    bool Number(JsonValue& out)
    {
        const size_t start = m_pos;
        if (m_input[m_pos] == L'-') ++m_pos;
        while (m_pos < m_input.size() && m_input[m_pos] >= L'0' && m_input[m_pos] <= L'9') ++m_pos;
        if (m_pos < m_input.size() && m_input[m_pos] == L'.')
        {
            ++m_pos;
            while (m_pos < m_input.size() && m_input[m_pos] >= L'0' && m_input[m_pos] <= L'9') ++m_pos;
        }
        if (m_pos < m_input.size() && (m_input[m_pos] == L'e' || m_input[m_pos] == L'E'))
        {
            ++m_pos;
            if (m_pos < m_input.size() && (m_input[m_pos] == L'+' || m_input[m_pos] == L'-')) ++m_pos;
            while (m_pos < m_input.size() && m_input[m_pos] >= L'0' && m_input[m_pos] <= L'9') ++m_pos;
        }
        if (start == m_pos) return false;
        wchar_t* end{};
        out.number = wcstod(m_input.c_str() + start, &end);
        if (end != m_input.c_str() + m_pos) return false;
        out.type = JsonValue::Type::Number;
        return true;
    }

    const std::wstring& m_input;
    size_t m_pos{};
};

inline bool ParseJson(const std::wstring& input, JsonValue& result)
{
    return JsonParser(input).Parse(result);
}
