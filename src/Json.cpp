#include "Json.h"

#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace musuka {

namespace {

const JsonValue& StaticNull() {
    static JsonValue value = JsonValue::Null();
    return value;
}

class Parser {
public:
    explicit Parser(const std::string& text) : text_(text) {}

    bool Parse(JsonValue& out, std::string& error) {
        SkipWhitespace();
        if (!ParseValue(out, error, 0)) {
            return false;
        }
        SkipWhitespace();
        if (pos_ != text_.size()) {
            error = "Unexpected trailing characters.";
            return false;
        }
        return true;
    }

private:
    bool ParseValue(JsonValue& out, std::string& error, int depth) {
        if (depth > 64) {
            error = "JSON nesting too deep.";
            return false;
        }
        SkipWhitespace();
        if (pos_ >= text_.size()) {
            error = "Unexpected end of JSON.";
            return false;
        }
        const char ch = text_[pos_];
        if (ch == 'n') {
            return ParseLiteral("null", JsonValue::Null(), out, error);
        }
        if (ch == 't') {
            return ParseLiteral("true", JsonValue::Bool(true), out, error);
        }
        if (ch == 'f') {
            return ParseLiteral("false", JsonValue::Bool(false), out, error);
        }
        if (ch == '"') {
            std::string value;
            if (!ParseString(value, error)) {
                return false;
            }
            out = JsonValue::String(value);
            return true;
        }
        if (ch == '[') {
            return ParseArray(out, error, depth);
        }
        if (ch == '{') {
            return ParseObject(out, error, depth);
        }
        if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch))) {
            return ParseNumber(out, error);
        }
        error = "Unexpected JSON token.";
        return false;
    }

    bool ParseLiteral(const char* literal, const JsonValue& value, JsonValue& out, std::string& error) {
        const std::string expected(literal);
        if (text_.compare(pos_, expected.size(), expected) != 0) {
            error = "Invalid JSON literal.";
            return false;
        }
        pos_ += expected.size();
        out = value;
        return true;
    }

    bool ParseString(std::string& out, std::string& error) {
        if (text_[pos_] != '"') {
            error = "Expected string.";
            return false;
        }
        ++pos_;
        out.clear();
        while (pos_ < text_.size()) {
            const char ch = text_[pos_++];
            if (ch == '"') {
                return true;
            }
            if (ch == '\\') {
                if (pos_ >= text_.size()) {
                    error = "Invalid escape sequence.";
                    return false;
                }
                const char esc = text_[pos_++];
                switch (esc) {
                case '"':
                case '\\':
                case '/':
                    out.push_back(esc);
                    break;
                case 'b':
                    out.push_back('\b');
                    break;
                case 'f':
                    out.push_back('\f');
                    break;
                case 'n':
                    out.push_back('\n');
                    break;
                case 'r':
                    out.push_back('\r');
                    break;
                case 't':
                    out.push_back('\t');
                    break;
                case 'u':
                    if (!ParseUnicodeEscape(out, error)) {
                        return false;
                    }
                    break;
                default:
                    error = "Unsupported escape sequence.";
                    return false;
                }
            } else {
                out.push_back(ch);
            }
        }
        error = "Unterminated string.";
        return false;
    }

    bool ParseUnicodeEscape(std::string& out, std::string& error) {
        if (pos_ + 4 > text_.size()) {
            error = "Invalid unicode escape.";
            return false;
        }
        unsigned int code = 0;
        for (int i = 0; i < 4; ++i) {
            const char ch = text_[pos_++];
            code <<= 4;
            if (ch >= '0' && ch <= '9') {
                code += ch - '0';
            } else if (ch >= 'a' && ch <= 'f') {
                code += 10 + ch - 'a';
            } else if (ch >= 'A' && ch <= 'F') {
                code += 10 + ch - 'A';
            } else {
                error = "Invalid unicode escape.";
                return false;
            }
        }

        if (code <= 0x7F) {
            out.push_back(static_cast<char>(code));
        } else if (code <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | ((code >> 6) & 0x1F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xE0 | ((code >> 12) & 0x0F)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
        return true;
    }

    bool ParseNumber(JsonValue& out, std::string& error) {
        const size_t start = pos_;
        if (text_[pos_] == '-') {
            ++pos_;
        }
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
        if (pos_ < text_.size() && text_[pos_] == '.') {
            ++pos_;
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                ++pos_;
            }
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) {
                ++pos_;
            }
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                ++pos_;
            }
        }

        try {
            out = JsonValue::Number(std::stod(text_.substr(start, pos_ - start)));
        } catch (...) {
            error = "Invalid number.";
            return false;
        }
        return true;
    }

    bool ParseArray(JsonValue& out, std::string& error, int depth) {
        ++pos_;
        JsonValue::Array values;
        SkipWhitespace();
        if (Consume(']')) {
            out = JsonValue::ArrayValue(std::move(values));
            return true;
        }
        constexpr size_t kMaxArraySize = 65536;
        for (;;) {
            if (values.size() >= kMaxArraySize) {
                error = "JSON array too large.";
                return false;
            }
            JsonValue item;
            if (!ParseValue(item, error, depth + 1)) {
                return false;
            }
            values.push_back(std::move(item));
            SkipWhitespace();
            if (Consume(']')) {
                out = JsonValue::ArrayValue(std::move(values));
                return true;
            }
            if (!Consume(',')) {
                error = "Expected comma in array.";
                return false;
            }
        }
    }

    bool ParseObject(JsonValue& out, std::string& error, int depth) {
        ++pos_;
        JsonValue::Object values;
        SkipWhitespace();
        if (Consume('}')) {
            out = JsonValue::ObjectValue(std::move(values));
            return true;
        }
        constexpr size_t kMaxObjectMembers = 4096;
        for (;;) {
            if (values.size() >= kMaxObjectMembers) {
                error = "JSON object has too many members.";
                return false;
            }
            std::string key;
            if (!ParseString(key, error)) {
                return false;
            }
            SkipWhitespace();
            if (!Consume(':')) {
                error = "Expected colon in object.";
                return false;
            }
            JsonValue item;
            if (!ParseValue(item, error, depth + 1)) {
                return false;
            }
            values[key] = std::move(item);
            SkipWhitespace();
            if (Consume('}')) {
                out = JsonValue::ObjectValue(std::move(values));
                return true;
            }
            if (!Consume(',')) {
                error = "Expected comma in object.";
                return false;
            }
            SkipWhitespace();
        }
    }

    bool Consume(char expected) {
        SkipWhitespace();
        if (pos_ < text_.size() && text_[pos_] == expected) {
            ++pos_;
            return true;
        }
        return false;
    }

    void SkipWhitespace() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
    }

    const std::string& text_;
    size_t pos_ = 0;
};

std::string EscapeString(const std::string& value) {
    std::ostringstream out;
    out << '"';
    for (char ch : value) {
        switch (ch) {
        case '"':
            out << "\\\"";
            break;
        case '\\':
            out << "\\\\";
            break;
        case '\b':
            out << "\\b";
            break;
        case '\f':
            out << "\\f";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            if (static_cast<unsigned char>(ch) < 0x20) {
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                    << static_cast<int>(static_cast<unsigned char>(ch)) << std::dec;
            } else {
                out << ch;
            }
            break;
        }
    }
    out << '"';
    return out.str();
}

std::string Indent(int indent) {
    return std::string(static_cast<size_t>(indent), ' ');
}

} // namespace

JsonValue::JsonValue() = default;

JsonValue JsonValue::Null() {
    return JsonValue();
}

JsonValue JsonValue::Bool(bool value) {
    JsonValue result;
    result.type_ = Type::Bool;
    result.boolValue_ = value;
    return result;
}

JsonValue JsonValue::Number(double value) {
    JsonValue result;
    result.type_ = Type::Number;
    result.numberValue_ = value;
    return result;
}

JsonValue JsonValue::String(std::string value) {
    JsonValue result;
    result.type_ = Type::String;
    result.stringValue_ = std::move(value);
    return result;
}

JsonValue JsonValue::ArrayValue(Array value) {
    JsonValue result;
    result.type_ = Type::Array;
    result.arrayValue_ = std::move(value);
    return result;
}

JsonValue JsonValue::ObjectValue(Object value) {
    JsonValue result;
    result.type_ = Type::Object;
    result.objectValue_ = std::move(value);
    return result;
}

bool JsonValue::AsBool(bool fallback) const {
    return type_ == Type::Bool ? boolValue_ : fallback;
}

double JsonValue::AsNumber(double fallback) const {
    return type_ == Type::Number ? numberValue_ : fallback;
}

const std::string& JsonValue::AsString() const {
    if (type_ != Type::String) {
        static const std::string empty;
        return empty;
    }
    return stringValue_;
}

std::string JsonValue::AsStringOr(std::string fallback) const {
    return type_ == Type::String ? stringValue_ : fallback;
}

const JsonValue::Array& JsonValue::AsArray() const {
    if (type_ != Type::Array) {
        static const Array empty;
        return empty;
    }
    return arrayValue_;
}

JsonValue::Array& JsonValue::AsArray() {
    if (type_ != Type::Array) {
        type_ = Type::Array;
        arrayValue_.clear();
    }
    return arrayValue_;
}

const JsonValue::Object& JsonValue::AsObject() const {
    if (type_ != Type::Object) {
        static const Object empty;
        return empty;
    }
    return objectValue_;
}

JsonValue::Object& JsonValue::AsObject() {
    if (type_ != Type::Object) {
        type_ = Type::Object;
        objectValue_.clear();
    }
    return objectValue_;
}

bool JsonValue::Has(const std::string& key) const {
    return type_ == Type::Object && objectValue_.find(key) != objectValue_.end();
}

const JsonValue& JsonValue::At(const std::string& key) const {
    if (type_ != Type::Object) {
        return StaticNull();
    }
    const auto it = objectValue_.find(key);
    return it == objectValue_.end() ? StaticNull() : it->second;
}

JsonValue& JsonValue::operator[](const std::string& key) {
    if (type_ != Type::Object) {
        type_ = Type::Object;
        objectValue_.clear();
    }
    return objectValue_[key];
}

bool ParseJson(const std::string& text, JsonValue& outValue, std::string& outError) {
    Parser parser(text);
    return parser.Parse(outValue, outError);
}

std::string StringifyJson(const JsonValue& value, int indent) {
    switch (value.type()) {
    case JsonValue::Type::Null:
        return "null";
    case JsonValue::Type::Bool:
        return value.AsBool() ? "true" : "false";
    case JsonValue::Type::Number: {
        const double number = value.AsNumber();
        if (std::fabs(number - std::round(number)) < 0.0000001) {
            return std::to_string(static_cast<long long>(std::llround(number)));
        }
        std::ostringstream out;
        out << number;
        return out.str();
    }
    case JsonValue::Type::String:
        return EscapeString(value.AsString());
    case JsonValue::Type::Array: {
        const auto& array = value.AsArray();
        if (array.empty()) {
            return "[]";
        }
        std::ostringstream out;
        out << "[\n";
        for (size_t i = 0; i < array.size(); ++i) {
            out << Indent(indent + 2) << StringifyJson(array[i], indent + 2);
            if (i + 1 < array.size()) {
                out << ",";
            }
            out << "\n";
        }
        out << Indent(indent) << "]";
        return out.str();
    }
    case JsonValue::Type::Object: {
        const auto& object = value.AsObject();
        if (object.empty()) {
            return "{}";
        }
        std::ostringstream out;
        out << "{\n";
        size_t index = 0;
        for (const auto& [key, item] : object) {
            out << Indent(indent + 2) << EscapeString(key) << ": " << StringifyJson(item, indent + 2);
            if (++index < object.size()) {
                out << ",";
            }
            out << "\n";
        }
        out << Indent(indent) << "}";
        return out.str();
    }
    }
    return "null";
}

} // namespace musuka

