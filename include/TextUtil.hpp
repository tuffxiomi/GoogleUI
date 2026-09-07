#pragma once

#include "CoreTypes.hpp"
#include <jni.h>

namespace google_ui::text {

bool equals(const char* left, const char* right) noexcept;
bool contains(const char* text, const char* needle) noexcept;
bool startsWith(const char* text, const char* prefix) noexcept;
int parseInt(const char* value, int fallback) noexcept;
bool parseBool(const char* value, bool fallback) noexcept;
int clampInt(int value, int minimum, int maximum) noexcept;
bool copy(char* output, usize capacity, const char* value) noexcept;

struct Builder {
    char* data;
    usize capacity;
    usize length;

    void reset() noexcept;
    bool append(const char* value) noexcept;
    bool appendChar(char value) noexcept;
    bool appendInt(int value) noexcept;
    bool appendBool(bool value) noexcept;
    bool appendJsonEscaped(const char* value) noexcept;
};

bool jstringEquals(JNIEnv* env, jstring value, const char* expected) noexcept;
int jstringToInt(JNIEnv* env, jstring value, int fallback) noexcept;
bool jstringToBool(JNIEnv* env, jstring value, bool fallback) noexcept;
bool jstringToUtf8(JNIEnv* env, jstring value, char* output, usize capacity) noexcept;

} // namespace google_ui::text
