#include "TextUtil.hpp"

namespace google_ui::text {

bool equals(const char* left, const char* right) noexcept {
    if (left == right) {
        return true;
    }
    if (!left || !right) {
        return false;
    }
    while (*left && *right) {
        if (*left != *right) {
            return false;
        }
        ++left;
        ++right;
    }
    return *left == *right;
}

bool startsWith(const char* value, const char* prefix) noexcept {
    if (!value || !prefix) {
        return false;
    }
    while (*prefix) {
        if (*value++ != *prefix++) {
            return false;
        }
    }
    return true;
}

bool contains(const char* value, const char* needle) noexcept {
    if (!value || !needle || !*needle) {
        return false;
    }
    for (const char* cursor = value; *cursor; ++cursor) {
        const char* left = cursor;
        const char* right = needle;
        while (*left && *right && *left == *right) {
            ++left;
            ++right;
        }
        if (!*right) {
            return true;
        }
    }
    return false;
}

int parseInt(const char* value, int fallback) noexcept {
    if (!value || !*value) {
        return fallback;
    }
    bool negative = false;
    if (*value == '-') {
        negative = true;
        ++value;
    }
    if (!*value) {
        return fallback;
    }
    int result = 0;
    bool foundDigit = false;
    while (*value) {
        const char c = *value++;
        if (c < '0' || c > '9') {
            return fallback;
        }
        foundDigit = true;
        const int digit = c - '0';
        if (result > 214748364 || (result == 214748364 && digit > 7)) {
            return fallback;
        }
        result = result * 10 + digit;
    }
    if (!foundDigit) {
        return fallback;
    }
    return negative ? -result : result;
}

bool parseBool(const char* value, bool fallback) noexcept {
    if (!value) {
        return fallback;
    }
    if (equals(value, "true") || equals(value, "1") || equals(value, "on")) {
        return true;
    }
    if (equals(value, "false") || equals(value, "0") || equals(value, "off")) {
        return false;
    }
    return fallback;
}

bool copy(char* output, usize capacity, const char* value) noexcept {
    if (!output || capacity == 0) {
        return false;
    }
    usize index = 0;
    while (value && value[index] && index + 1 < capacity) {
        output[index] = value[index];
        ++index;
    }
    output[index] = '\0';
    return !value || value[index] == '\0';
}

int clampInt(int value, int minimum, int maximum) noexcept {
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

void Builder::reset() noexcept {
    length = 0;
    if (data && capacity) {
        data[0] = '\0';
    }
}

bool Builder::append(const char* value) noexcept {
    if (!data || capacity == 0 || !value) {
        return false;
    }
    while (*value) {
        if (length + 1 >= capacity) {
            data[capacity - 1] = '\0';
            return false;
        }
        data[length++] = *value++;
    }
    data[length] = '\0';
    return true;
}

bool Builder::appendChar(char value) noexcept {
    if (!data || capacity == 0 || length + 1 >= capacity) {
        if (data && capacity) {
            data[capacity - 1] = '\0';
        }
        return false;
    }
    data[length++] = value;
    data[length] = '\0';
    return true;
}

bool Builder::appendInt(int value) noexcept {
    char digits[16];
    unsigned int magnitude;
    if (value < 0) {
        if (!append("-")) {
            return false;
        }
        magnitude = static_cast<unsigned int>(-(value + 1)) + 1U;
    } else {
        magnitude = static_cast<unsigned int>(value);
    }

    int count = 0;
    do {
        digits[count++] = static_cast<char>('0' + (magnitude % 10U));
        magnitude /= 10U;
    } while (magnitude && count < static_cast<int>(sizeof(digits)));

    while (count > 0) {
        char one[2] = {digits[--count], '\0'};
        if (!append(one)) {
            return false;
        }
    }
    return true;
}

bool Builder::appendBool(bool value) noexcept {
    return append(value ? "true" : "false");
}

bool Builder::appendJsonEscaped(const char* value) noexcept {
    if (!append("\"")) {
        return false;
    }
    for (usize index = 0; value && value[index]; ++index) {
        const unsigned char current = static_cast<unsigned char>(value[index]);
        if (current == '\"' || current == '\\') {
            if (!appendChar('\\') || !appendChar(static_cast<char>(current))) {
                return false;
            }
        } else if (current == '\n') {
            if (!append("\\n")) return false;
        } else if (current == '\r') {
            if (!append("\\r")) return false;
        } else if (current == '\t') {
            if (!append("\\t")) return false;
        } else if (current < 0x20U) {
            if (!append("?")) return false;
        } else if (!appendChar(static_cast<char>(current))) {
            return false;
        }
    }
    return append("\"");
}

bool jstringEquals(JNIEnv* env, jstring value, const char* expected) noexcept {
    if (!env || !value || !expected) {
        return false;
    }
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (!chars) {
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
        return false;
    }
    const bool result = equals(chars, expected);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

int jstringToInt(JNIEnv* env, jstring value, int fallback) noexcept {
    if (!env || !value) {
        return fallback;
    }
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (!chars) {
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
        return fallback;
    }
    const int result = parseInt(chars, fallback);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

bool jstringToBool(JNIEnv* env, jstring value, bool fallback) noexcept {
    if (!env || !value) {
        return fallback;
    }
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (!chars) {
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
        return fallback;
    }
    const bool result = parseBool(chars, fallback);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}


bool jstringToUtf8(JNIEnv* env, jstring value, char* output, usize capacity) noexcept {
    if (!env || !value || !output || capacity == 0) {
        return false;
    }
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (!chars) {
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
        output[0] = '\0';
        return false;
    }
    const bool result = copy(output, capacity, chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

} // namespace google_ui::text
