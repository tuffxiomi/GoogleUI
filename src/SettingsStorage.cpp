#include "SettingsStorage.hpp"

#include "JniHelpers.hpp"
#include "RuntimeState.hpp"
#include "TextUtil.hpp"

namespace google_ui::settings {
namespace {

inline constexpr const char* kConfigSuffix = "/config/config.json";
inline constexpr usize kConfigBufferCapacity = 8192;

bool isSpace(char value) noexcept {
    return value == ' ' || value == '\t' || value == '\n' || value == '\r';
}

usize stringLength(const char* value) noexcept {
    usize length = 0;
    while (value && value[length]) {
        ++length;
    }
    return length;
}

bool buildConfigPath(char* output, usize capacity) noexcept {
    if (!output || capacity == 0 || !gState.modRootPath[0]) {
        return false;
    }
    text::Builder builder{output, capacity, 0};
    builder.reset();
    return builder.append(gState.modRootPath) && builder.append(kConfigSuffix);
}

void closeRandomAccessFile(JNIEnv* env, jobject file, jclass fileClass) noexcept {
    if (!env || !file || !fileClass) {
        return;
    }
    const jmethodID close = env->GetMethodID(fileClass, "close", "()V");
    if (close && !jni::clearException(env)) {
        env->CallVoidMethod(file, close);
        (void)jni::clearException(env);
    } else {
        (void)jni::clearException(env);
    }
}

bool readConfigFile(JNIEnv* env, char* output, usize capacity) noexcept {
    if (!env || !output || capacity < 2) {
        return false;
    }
    output[0] = '\0';
    char path[1200];
    if (!buildConfigPath(path, sizeof(path))) {
        return false;
    }

    jclass fileClass = env->FindClass("java/io/RandomAccessFile");
    const jmethodID constructor = fileClass
        ? env->GetMethodID(
              fileClass,
              "<init>",
              "(Ljava/lang/String;Ljava/lang/String;)V")
        : nullptr;
    jstring pathValue = jni::makeString(env, path);
    jstring modeValue = jni::makeString(env, "r");
    jobject file = fileClass && constructor && pathValue && modeValue
        ? env->NewObject(fileClass, constructor, pathValue, modeValue)
        : nullptr;
    bool ok = fileClass && constructor && pathValue && modeValue && file &&
              !jni::clearException(env);

    const jmethodID lengthMethod = ok
        ? env->GetMethodID(fileClass, "length", "()J")
        : nullptr;
    const jmethodID readFully = ok
        ? env->GetMethodID(fileClass, "readFully", "([B)V")
        : nullptr;
    const jlong length = lengthMethod
        ? env->CallLongMethod(file, lengthMethod)
        : 0;
    ok = ok && lengthMethod && readFully && length > 0 &&
         length < static_cast<jlong>(capacity) && !jni::clearException(env);

    jbyteArray bytes = ok
        ? env->NewByteArray(static_cast<jsize>(length))
        : nullptr;
    if (ok && bytes) {
        env->CallVoidMethod(file, readFully, bytes);
        ok = !jni::clearException(env);
    } else {
        ok = false;
    }
    if (ok) {
        env->GetByteArrayRegion(
            bytes,
            0,
            static_cast<jsize>(length),
            reinterpret_cast<jbyte*>(output));
        ok = !jni::clearException(env);
        if (ok) {
            output[static_cast<usize>(length)] = '\0';
        }
    }

    closeRandomAccessFile(env, file, fileClass);
    jni::deleteLocal(env, bytes);
    jni::deleteLocal(env, file);
    jni::deleteLocal(env, modeValue);
    jni::deleteLocal(env, pathValue);
    jni::deleteLocal(env, fileClass);
    return ok;
}

bool writeConfigFile(JNIEnv* env, const char* data, usize length) noexcept {
    if (!env || !data || length == 0 || length > 65535) {
        return false;
    }
    char path[1200];
    if (!buildConfigPath(path, sizeof(path))) {
        return false;
    }

    jclass fileClass = env->FindClass("java/io/RandomAccessFile");
    const jmethodID constructor = fileClass
        ? env->GetMethodID(
              fileClass,
              "<init>",
              "(Ljava/lang/String;Ljava/lang/String;)V")
        : nullptr;
    jstring pathValue = jni::makeString(env, path);
    jstring modeValue = jni::makeString(env, "rw");
    jobject file = fileClass && constructor && pathValue && modeValue
        ? env->NewObject(fileClass, constructor, pathValue, modeValue)
        : nullptr;
    bool ok = fileClass && constructor && pathValue && modeValue && file &&
              !jni::clearException(env);

    const jmethodID setLength = ok
        ? env->GetMethodID(fileClass, "setLength", "(J)V")
        : nullptr;
    const jmethodID write = ok
        ? env->GetMethodID(fileClass, "write", "([B)V")
        : nullptr;
    jbyteArray bytes = ok
        ? env->NewByteArray(static_cast<jsize>(length))
        : nullptr;
    ok = ok && setLength && write && bytes && !jni::clearException(env);
    if (ok) {
        env->SetByteArrayRegion(
            bytes,
            0,
            static_cast<jsize>(length),
            reinterpret_cast<const jbyte*>(data));
        ok = !jni::clearException(env);
    }
    if (ok) {
        env->CallVoidMethod(file, setLength, static_cast<jlong>(0));
        env->CallVoidMethod(file, write, bytes);
        ok = !jni::clearException(env);
    }

    closeRandomAccessFile(env, file, fileClass);
    jni::deleteLocal(env, bytes);
    jni::deleteLocal(env, file);
    jni::deleteLocal(env, modeValue);
    jni::deleteLocal(env, pathValue);
    jni::deleteLocal(env, fileClass);
    return ok;
}

const char* findKeyValue(const char* json, const char* key) noexcept {
    if (!json || !key) {
        return nullptr;
    }
    const usize keyLength = stringLength(key);
    for (usize offset = 0; json[offset]; ++offset) {
        if (json[offset] != '"') {
            continue;
        }
        usize index = 0;
        while (index < keyLength && json[offset + 1 + index] == key[index]) {
            ++index;
        }
        if (index != keyLength || json[offset + 1 + index] != '"') {
            continue;
        }
        const char* cursor = json + offset + keyLength + 2;
        while (*cursor && isSpace(*cursor)) {
            ++cursor;
        }
        if (*cursor != ':') {
            continue;
        }
        ++cursor;
        while (*cursor && isSpace(*cursor)) {
            ++cursor;
        }
        return cursor;
    }
    return nullptr;
}

bool parseJsonString(
    const char* json,
    const char* key,
    char* output,
    usize capacity) noexcept {
    const char* cursor = findKeyValue(json, key);
    if (!cursor || *cursor != '"' || !output || capacity < 2) {
        return false;
    }
    ++cursor;
    usize length = 0;
    while (*cursor && *cursor != '"') {
        char current = *cursor++;
        if (current == '\\') {
            const char escaped = *cursor++;
            if (!escaped) {
                return false;
            }
            if (escaped == 'n') current = '\n';
            else if (escaped == 'r') current = '\r';
            else if (escaped == 't') current = '\t';
            else if (escaped == '"' || escaped == '\\' || escaped == '/') current = escaped;
            else return false;
        }
        if (length + 1 >= capacity) {
            return false;
        }
        output[length++] = current;
    }
    if (*cursor != '"') {
        return false;
    }
    output[length] = '\0';
    return length > 0;
}

bool normalizeWebsiteUrl(const char* source, char* output, usize capacity) noexcept {
    if (!source || !output || capacity < 10) {
        return false;
    }
    while (*source && isSpace(*source)) {
        ++source;
    }
    usize length = stringLength(source);
    while (length > 0 && isSpace(source[length - 1])) {
        --length;
    }
    if (length == 0 || length + 9 >= capacity) {
        return false;
    }
    for (usize index = 0; index < length; ++index) {
        if (isSpace(source[index]) || source[index] == '"' || source[index] == '\\') {
            return false;
        }
    }

    char trimmed[kWebsiteUrlCapacity];
    if (length >= sizeof(trimmed)) {
        return false;
    }
    for (usize index = 0; index < length; ++index) {
        trimmed[index] = source[index];
    }
    trimmed[length] = '\0';

    if (text::startsWith(trimmed, "https://") || text::startsWith(trimmed, "http://")) {
        return text::copy(output, capacity, trimmed);
    }
    if (text::contains(trimmed, "://") || text::startsWith(trimmed, "javascript:") ||
        text::startsWith(trimmed, "file:") || text::startsWith(trimmed, "content:")) {
        return false;
    }
    text::Builder builder{output, capacity, 0};
    builder.reset();
    return builder.append("https://") && builder.append(trimmed);
}

void applyDefaults() noexcept {
    (void)text::copy(
        gState.websiteUrl,
        sizeof(gState.websiteUrl),
        kDefaultWebsiteUrl);
}

} // namespace

bool load(JNIEnv* env) noexcept {
    if (gState.settingsLoaded) {
        return true;
    }
    applyDefaults();
    if (!env) {
        return false;
    }

    char data[kConfigBufferCapacity];
    if (readConfigFile(env, data, sizeof(data))) {
        char urlValue[kWebsiteUrlCapacity];
        char normalized[kWebsiteUrlCapacity];
        if (parseJsonString(data, "button_url", urlValue, sizeof(urlValue)) &&
            normalizeWebsiteUrl(urlValue, normalized, sizeof(normalized))) {
            (void)text::copy(gState.websiteUrl, sizeof(gState.websiteUrl), normalized);
        }
    }

    gState.settingsLoaded = true;
    return true;
}

bool save(JNIEnv* env) noexcept {
    if (!env) {
        return false;
    }
    char data[kConfigBufferCapacity];
    text::Builder builder{data, sizeof(data), 0};
    builder.reset();
    bool ok = builder.append("{\n  \"default_enabled\": true,\n") &&
              builder.append("  \"auto_resolution\": true,\n") &&
              builder.append("  \"ad_blocking\": true,\n") &&
              builder.append("  \"button_url\": ") &&
              builder.appendJsonEscaped(gState.websiteUrl) &&
              builder.append("\n}\n");
    return ok && writeConfigFile(env, data, builder.length);
}

void resetLoadedState() noexcept {
    gState.settingsLoaded = false;
}

bool setWebsiteUrl(const char* value) noexcept {
    char normalized[kWebsiteUrlCapacity];
    if (!normalizeWebsiteUrl(value, normalized, sizeof(normalized))) {
        return false;
    }
    return text::copy(gState.websiteUrl, sizeof(gState.websiteUrl), normalized);
}

bool deriveWebsiteActionName(
    const char* url,
    char* output,
    usize capacity) noexcept {
    if (!url || !output || capacity < 22) {
        return false;
    }

    const char* host = url;
    const char* scheme = nullptr;
    for (const char* cursor = url; *cursor; ++cursor) {
        if (cursor[0] == ':' && cursor[1] == '/' && cursor[2] == '/') {
            scheme = cursor;
            break;
        }
    }
    if (scheme) {
        host = scheme + 3;
    }
    const char* hostEnd = host;
    while (*hostEnd && *hostEnd != '/' && *hostEnd != ':' &&
           *hostEnd != '?' && *hostEnd != '#') {
        ++hostEnd;
    }
    while (hostEnd > host && hostEnd[-1] == '.') {
        --hostEnd;
    }
    if (hostEnd <= host) {
        return text::copy(output, capacity, "OPEN / HIDE WEBSITE");
    }

    const char* lastDot = nullptr;
    const char* previousDot = nullptr;
    for (const char* cursor = host; cursor < hostEnd; ++cursor) {
        if (*cursor == '.') {
            previousDot = lastDot;
            lastDot = cursor;
        }
    }
    const char* labelStart = previousDot ? previousDot + 1 : host;
    const char* labelEnd = lastDot ? lastDot : hostEnd;

    // Preserve the registrable brand for common country-code suffixes.
    const usize labelLength = static_cast<usize>(labelEnd - labelStart);
    const bool commonSecondLevel =
        (labelLength == 2 &&
         ((labelStart[0] == 'c' && labelStart[1] == 'o') ||
          (labelStart[0] == 'o' && labelStart[1] == 'r') ||
          (labelStart[0] == 'n' && labelStart[1] == 'e'))) ||
        (labelLength == 3 &&
         ((labelStart[0] == 'c' && labelStart[1] == 'o' && labelStart[2] == 'm') ||
          (labelStart[0] == 'n' && labelStart[1] == 'e' && labelStart[2] == 't') ||
          (labelStart[0] == 'o' && labelStart[1] == 'r' && labelStart[2] == 'g')));
    if (commonSecondLevel && previousDot) {
        const char* earlierDot = nullptr;
        for (const char* cursor = host; cursor < previousDot; ++cursor) {
            if (*cursor == '.') {
                earlierDot = cursor;
            }
        }
        labelStart = earlierDot ? earlierDot + 1 : host;
        labelEnd = previousDot;
    }

    text::Builder builder{output, capacity, 0};
    builder.reset();
    if (!builder.append("OPEN / HIDE ")) {
        return false;
    }
    bool wrote = false;
    for (const char* cursor = labelStart; cursor < labelEnd; ++cursor) {
        char value = *cursor;
        if (value >= 'a' && value <= 'z') {
            value = static_cast<char>(value - ('a' - 'A'));
        }
        if ((value >= 'A' && value <= 'Z') ||
            (value >= '0' && value <= '9') || value == '-' || value == '_') {
            if (!builder.appendChar(value)) {
                return false;
            }
            wrote = true;
        }
    }
    return wrote || text::copy(output, capacity, "OPEN / HIDE WEBSITE");
}

} // namespace google_ui::settings
