#include "ModMenuBridgeHooks.hpp"

#include "AdBlocker.hpp"
#include "AndroidWebViewController.hpp"
#include "GoogleChrome.hpp"
#include "DrawCommandAppender.hpp"
#include "FloatingButtonController.hpp"
#include "JniHelpers.hpp"
#include "ResponsiveLayout.hpp"
#include "RuntimeState.hpp"
#include "RuntimeLog.hpp"
#include "SettingsStorage.hpp"
#include "TextUtil.hpp"
#include "WebsiteChrome.hpp"
#include "WebsiteWebViewController.hpp"

namespace google_ui::hooks {
namespace {

constexpr const char* kInfosSymbol =
    "Java_org_levimc_launcher_core_mods_inbuilt_ExternalModBridge_nativeGetExternalModsInfo";
constexpr const char* kToggleSymbol =
    "Java_org_levimc_launcher_core_mods_inbuilt_ExternalModBridge_nativeToggleExternalMod";
constexpr const char* kConfigSymbol =
    "Java_org_levimc_launcher_core_mods_inbuilt_ExternalModBridge_nativeSetExternalModConfig";
constexpr const char* kRevisionSymbol =
    "Java_org_levimc_launcher_core_mods_inbuilt_ExternalModBridge_nativeGetDrawCommandsRevision";
constexpr const char* kDrawSymbol =
    "Java_org_levimc_launcher_core_mods_inbuilt_ExternalModBridge_nativeGetDrawCommands";
constexpr const char* kButtonCountSymbol =
    "Java_org_levimc_launcher_core_mods_inbuilt_ExternalModBridge_nativeGetExternalButtonCount";
constexpr const char* kButtonInfoSymbol =
    "Java_org_levimc_launcher_core_mods_inbuilt_ExternalModBridge_nativeGetExternalButtonInfo";
constexpr const char* kButtonIconSymbol =
    "Java_org_levimc_launcher_core_mods_inbuilt_ExternalModBridge_nativeGetExternalButtonIconBytes";
constexpr const char* kButtonEventSymbol =
    "Java_org_levimc_launcher_core_mods_inbuilt_ExternalModBridge_nativeDispatchExternalButtonEvent";

bool appendTextConfig(
    text::Builder& builder,
    const char* key,
    const char* displayName,
    const char* defaultValue,
    const char* currentValue,
    bool comma) noexcept {
    bool ok = !comma || builder.append(",");
    ok = ok && builder.append("{\"key\":");
    ok = ok && builder.appendJsonEscaped(key);
    ok = ok && builder.append(",\"display_name\":");
    ok = ok && builder.appendJsonEscaped(displayName);
    ok = ok && builder.append(",\"type\":6,\"default_value\":");
    ok = ok && builder.appendJsonEscaped(defaultValue);
    ok = ok && builder.append(",\"min_value\":\"\",\"max_value\":\"\",\"current_value\":");
    ok = ok && builder.appendJsonEscaped(currentValue);
    ok = ok && builder.append(",\"depends_on\":\"\"}");
    return ok;
}

bool appendActionConfig(
    text::Builder& builder,
    const char* key,
    const char* displayName,
    bool comma) noexcept {
    bool ok = !comma || builder.append(",");
    ok = ok && builder.append("{\"key\":");
    ok = ok && builder.appendJsonEscaped(key);
    ok = ok && builder.append(",\"display_name\":");
    ok = ok && builder.appendJsonEscaped(displayName);
    ok = ok && builder.append(",\"type\":7,\"default_value\":\"\",\"min_value\":\"\",\"max_value\":\"\",\"current_value\":\"\",\"depends_on\":\"\"}");
    return ok;
}

bool buildModuleInfoJson(JNIEnv* env, char* output, usize capacity) noexcept {
    if (!env || !output || capacity == 0) {
        return false;
    }
    (void)settings::load(env);
    text::Builder builder{output, capacity, 0};
    builder.reset();
    bool ok = builder.append(
        "{\"module_id\":\"google_ui\","
        "\"display_name\":\"GoogleUI\","
        "\"description\":\"Google plus a separate configurable universal WebView with all-site ad filtering.\","
        "\"mod_id\":\"google_ui\",\"enabled\":");
    ok = ok && builder.appendBool(gState.moduleEnabled);
    ok = ok && builder.append(",\"hide_in_hud_editor\":true,\"configs\":[");
    ok = ok && appendTextConfig(
        builder, "button_url", "Website URL",
        kDefaultWebsiteUrl, gState.websiteUrl, false);
    char actionName[kWebsiteActionNameCapacity];
    if (!settings::deriveWebsiteActionName(
            gState.websiteUrl, actionName, sizeof(actionName))) {
        (void)text::copy(actionName, sizeof(actionName), "OPEN / HIDE WEBSITE");
    }
    ok = ok && appendActionConfig(
        builder, "open_hide_website", actionName, true);
    ok = ok && builder.append("]}");
    return ok;
}

jstring buildSingleModuleArray(JNIEnv* env) noexcept {
    if (!buildModuleInfoJson(
            env, gModuleInfoJson, sizeof(gModuleInfoJson))) {
        return env->NewStringUTF("[]");
    }
    text::Builder builder{
        gBulkModuleInfoJson, sizeof(gBulkModuleInfoJson), 0};
    builder.reset();
    const bool ok = builder.append("[") &&
        builder.append(gModuleInfoJson) && builder.append("]");
    return env->NewStringUTF(ok ? gBulkModuleInfoJson : "[]");
}

jstring hookGetInfos(JNIEnv* env, jclass clazz) noexcept {
    jstring original = gState.originalInfos
        ? gState.originalInfos(env, clazz)
        : nullptr;
    if (jni::clearException(env)) {
        if (original) {
            env->DeleteLocalRef(original);
        }
        original = nullptr;
    }
    if (!original) {
        return buildSingleModuleArray(env);
    }

    const jsize byteLength = env->GetStringUTFLength(original);
    if (jni::clearException(env) || byteLength < 2 ||
        static_cast<usize>(byteLength) + kModuleInfoJsonCapacity + 3U >=
            sizeof(gBulkModuleInfoJson)) {
        return original;
    }
    const char* chars = env->GetStringUTFChars(original, nullptr);
    if (!chars) {
        (void)jni::clearException(env);
        return original;
    }

    const usize length = static_cast<usize>(byteLength);
    bool validArray = chars[0] == '[' && chars[length - 1] == ']';
    bool hasItems = false;
    for (usize index = 1; validArray && index + 1 < length; ++index) {
        const char value = chars[index];
        if (value != ' ' && value != '\t' && value != '\r' && value != '\n') {
            hasItems = true;
            break;
        }
    }
    if (!validArray || !buildModuleInfoJson(
            env, gModuleInfoJson, sizeof(gModuleInfoJson))) {
        env->ReleaseStringUTFChars(original, chars);
        return original;
    }

    text::Builder builder{
        gBulkModuleInfoJson, sizeof(gBulkModuleInfoJson), 0};
    builder.reset();
    bool ok = builder.appendChar('[');
    for (usize index = 1; ok && index + 1 < length; ++index) {
        ok = builder.appendChar(chars[index]);
    }
    if (ok && hasItems) {
        ok = builder.appendChar(',');
    }
    ok = ok && builder.append(gModuleInfoJson) && builder.appendChar(']');
    env->ReleaseStringUTFChars(original, chars);
    if (!ok) {
        return original;
    }

    jstring merged = env->NewStringUTF(gBulkModuleInfoJson);
    if (!merged || jni::clearException(env)) {
        return original;
    }
    env->DeleteLocalRef(original);
    return merged;
}

void hookToggle(
    JNIEnv* env,
    jclass clazz,
    jstring moduleId,
    jboolean enabled) noexcept {
    if (!text::jstringEquals(env, moduleId, kModuleId)) {
        if (gState.originalToggle) {
            gState.originalToggle(env, clazz, moduleId, enabled);
        }
        return;
    }
    gState.moduleEnabled = enabled == JNI_TRUE;
    if (!gState.moduleEnabled) {
        gState.uiVisible = false;
        gState.websiteUiVisible = false;
        if (jni::isMainThread(env)) {
            (void)website_webview::destroy(env);
            (void)webview::destroy(env);
            (void)floating_button::destroy(env);
        }
    } else if (jni::isMainThread(env)) {
        (void)settings::load(env);
    }
}

void hookConfig(
    JNIEnv* env,
    jclass clazz,
    jstring moduleId,
    jstring key,
    jstring value) noexcept {
    if (!text::jstringEquals(env, moduleId, kModuleId)) {
        if (gState.originalConfig) {
            gState.originalConfig(env, clazz, moduleId, key, value);
        }
        return;
    }
    (void)settings::load(env);

    if (text::jstringEquals(env, key, "open_hide_website")) {
        if (jni::isMainThread(env)) {
            (void)website_webview::toggle(env);
        }
        return;
    }

    bool changed = false;
    bool websiteUrlChanged = false;
    if (text::jstringEquals(env, key, "button_url")) {
        char textValue[kWebsiteUrlCapacity];
        if (text::jstringToUtf8(env, value, textValue, sizeof(textValue))) {
            changed = settings::setWebsiteUrl(textValue);
            websiteUrlChanged = changed;
        }
    }
    if (!changed) {
        return;
    }

    (void)settings::save(env);
    if (jni::isMainThread(env)) {
        if (websiteUrlChanged && website_webview::isAlive()) {
            (void)website_webview::reloadConfiguredUrl(env);
        }
    }
}

void closeRandomAccessFile(JNIEnv* env, jobject file, jclass fileClass) noexcept {
    if (!env || !file || !fileClass) {
        return;
    }
    const jmethodID close = env->GetMethodID(fileClass, "close", "()V");
    if (close && !jni::clearException(env)) {
        env->CallVoidMethod(file, close);
    }
    (void)jni::clearException(env);
}

jbyteArray readButtonAsset(JNIEnv* env, const char* relativePath) noexcept {
    if (!env || !relativePath || !gState.modRootPath[0]) {
        return nullptr;
    }
    char path[1400];
    text::Builder builder{path, sizeof(path), 0};
    builder.reset();
    if (!builder.append(gState.modRootPath) || !builder.append(relativePath)) {
        return nullptr;
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
    ok = ok && lengthMethod && readFully && length > 0 && length <= 1048576 &&
         !jni::clearException(env);
    jbyteArray bytes = ok
        ? env->NewByteArray(static_cast<jsize>(length))
        : nullptr;
    if (ok && bytes) {
        env->CallVoidMethod(file, readFully, bytes);
        ok = !jni::clearException(env);
    } else {
        ok = false;
    }

    closeRandomAccessFile(env, file, fileClass);
    jni::deleteLocal(env, file);
    jni::deleteLocal(env, modeValue);
    jni::deleteLocal(env, pathValue);
    jni::deleteLocal(env, fileClass);
    if (!ok) {
        jni::deleteLocal(env, bytes);
        return nullptr;
    }
    return bytes;
}

int hookGetExternalButtonCount() noexcept {
    const int count = gState.originalButtonCount
        ? gState.originalButtonCount()
        : 0;
    return count < 0 ? 1 : count + 1;
}

jstring hookGetExternalButtonInfo(
    JNIEnv* env,
    jclass clazz,
    jint index) noexcept {
    const int count = gState.originalButtonCount
        ? gState.originalButtonCount()
        : 0;
    if (index < count || index > count) {
        return gState.originalButtonInfo
            ? gState.originalButtonInfo(env, clazz, index)
            : env->NewStringUTF("{}");
    }

    char json[2048];
    text::Builder builder{json, sizeof(json), 0};
    builder.reset();
    bool ok = builder.append(
        "{\"button_id\":\"google_ui.google\","
        "\"module_id\":\"google_ui\","
        "\"display_name\":\"Google Google\","
        "\"mod_id\":\"google_ui\","
        "\"label\":\"\",\"android_key_code\":0,"
        "\"behavior\":1,\"default_visible\":true,\"module_enabled\":") &&
        builder.appendBool(gState.moduleEnabled) &&
        builder.append(
            ",\"has_icon\":true,\"icon_format\":1,"
            "\"hide_label_when_icon_present\":true,"
            "\"style\":{\"preset\":0,"
            "\"normal_bg_color\":1,\"active_bg_color\":1,"
            "\"border_color\":1,\"text_color\":1,"
            "\"active_text_color\":1,\"width_scale\":1.0,"
            "\"height_scale\":1.0}}");
    return env->NewStringUTF(ok ? json : "{}");
}

jbyteArray hookGetExternalButtonIconBytes(
    JNIEnv* env,
    jclass clazz,
    jstring buttonId,
    jint width,
    jint height,
    jboolean active) noexcept {
    if (!text::jstringEquals(env, buttonId, kFloatingButtonId)) {
        return gState.originalButtonIcon
            ? gState.originalButtonIcon(
                  env, clazz, buttonId, width, height, active)
            : nullptr;
    }
    return readButtonAsset(
        env,
        active == JNI_TRUE
            ? "/resources/google_button.png"
            : "/resources/google_button.png");
}

void hookDispatchExternalButtonEvent(
    JNIEnv* env,
    jclass clazz,
    jstring buttonId,
    jint event,
    jfloat value) noexcept {
    if (!text::jstringEquals(env, buttonId, kFloatingButtonId)) {
        if (gState.originalButtonEvent) {
            gState.originalButtonEvent(
                env, clazz, buttonId, event, value);
        }
        return;
    }
    // ButtonBehavior::Hold sends Down=1 and Up=2. Toggle the browser once on
    // Down, while Levi itself owns the pressed Before/After visuals.
    if (event == 1 && gState.moduleEnabled && jni::isMainThread(env)) {
        if (webview::isVisible()) {
            (void)webview::hide(env);
        } else {
            (void)webview::show(env);
        }
    }
}

jobjectArray hookGetDrawCommands(JNIEnv* env, jclass clazz) noexcept {
    jobjectArray original = gState.originalDraw
        ? gState.originalDraw(env, clazz)
        : nullptr;
    if (jni::clearException(env)) {
        return original;
    }
    if (gState.moduleEnabled) {
        floating_button::poll(env);
        webview::pollResolution(env);
        website_webview::pollResolution(env);
        chrome::poll(env);
        website_chrome::poll(env);
        adblock::poll(env);
    }
    if (!gState.uiVisible && !gState.websiteUiVisible) {
        return original;
    }
    jobjectArray result = draw::appendFrame(env, original);
    if (result != original && original) {
        env->DeleteLocalRef(original);
    }
    return result;
}

jlong hookGetDrawCommandsRevision() noexcept {
    jlong baseRevision = 0;
    if (gState.originalRevision) {
        baseRevision = gState.originalRevision();
    }
    if (!gState.moduleEnabled) {
        return baseRevision;
    }
    if (gState.syntheticDrawRevision >= 0x3FFFFFFFLL) {
        gState.syntheticDrawRevision = 1;
    } else {
        ++gState.syntheticDrawRevision;
    }
    constexpr jlong kMaxJlong = 0x7FFFFFFFFFFFFFFFLL;
    if (baseRevision < 0 ||
        baseRevision > kMaxJlong - gState.syntheticDrawRevision) {
        return gState.syntheticDrawRevision;
    }
    return baseRevision + gState.syntheticDrawRevision;
}

bool bytesMatchMasked(
    uptr address,
    const u8* expectedBytes,
    const u8* expectedMask,
    usize expectedByteCount) noexcept {
    if (!address || !expectedBytes || !expectedMask || expectedByteCount == 0) {
        return false;
    }
    const u8* code = reinterpret_cast<const u8*>(address);
    for (usize index = 0; index < expectedByteCount; ++index) {
        const u8 mask = expectedMask[index];
        if ((code[index] & mask) != (expectedBytes[index] & mask)) {
            return false;
        }
    }
    return true;
}

uptr resolveSymbol(const char* symbol, usize& symbolSize) noexcept {
    symbolSize = 0;
    uptr address = 0;
    if (gState.preloaderHandle) {
        address = GlossSymbol(gState.preloaderHandle, symbol, &symbolSize);
    }
    if (!address) {
        symbolSize = 0;
        address = GlossSymbolEx(
            reinterpret_cast<const void*>(&GlossOpen), symbol, &symbolSize);
    }
    return address;
}

bool resolveAndHook(
    const char* symbol,
    usize expectedSize,
    const u8* signatureBytes,
    const u8* signatureMask,
    usize signatureByteCount,
    void* replacement,
    void** original,
    GHook& hook) noexcept {
    if (!symbol || !replacement || !original) {
        return false;
    }
    usize symbolSize = 0;
    const uptr address = resolveSymbol(symbol, symbolSize);
    const bool resolved = address != 0;
    const bool signatureMatched = resolved &&
        (symbolSize == 0 || symbolSize == expectedSize) &&
        bytesMatchMasked(
            address, signatureBytes, signatureMask, signatureByteCount);
    if (!resolved) {
        runtime_log::hookStatus(
            symbol, false, false, false, symbolSize, expectedSize);
        return false;
    }
    if (!signatureMatched) {
        runtime_log::warn(
            "bridge symbol resolved but wildcard signature/size differed; attempting named-symbol hook");
    }
    hook = GlossHook(reinterpret_cast<void*>(address), replacement, original);
    const bool installed = hook != nullptr && *original != nullptr;
    runtime_log::hookStatus(
        symbol, true, signatureMatched, installed, symbolSize, expectedSize);
    return installed;
}

uptr decodeDirectBranchTarget(uptr stubAddress) noexcept {
    if (!stubAddress) {
        return 0;
    }
    const u32 instruction = *reinterpret_cast<const u32*>(stubAddress);
    if ((instruction & 0x7C000000U) != 0x14000000U) {
        return 0;
    }
    long long displacement = static_cast<long long>(instruction & 0x03FFFFFFU);
    if ((displacement & 0x02000000LL) != 0) {
        displacement |= ~0x03FFFFFFLL;
    }
    displacement <<= 2;
    return static_cast<uptr>(
        static_cast<long long>(stubAddress) + displacement);
}

bool resolveBranchTargetAndHook(
    const char* symbol,
    usize expectedStubSize,
    const u8* targetSignatureBytes,
    const u8* targetSignatureMask,
    usize targetSignatureByteCount,
    void* replacement,
    void** original,
    GHook& hook) noexcept {
    usize symbolSize = 0;
    const uptr stubAddress = resolveSymbol(symbol, symbolSize);
    const bool stubShapeMatched = stubAddress &&
        (symbolSize == 0 || symbolSize == expectedStubSize) &&
        ((*reinterpret_cast<const u32*>(stubAddress) & 0x7C000000U) ==
            0x14000000U);
    const uptr targetAddress = decodeDirectBranchTarget(stubAddress);
    const bool targetSignatureMatched = targetAddress &&
        bytesMatchMasked(
            targetAddress, targetSignatureBytes, targetSignatureMask,
            targetSignatureByteCount);
    if (!targetAddress) {
        runtime_log::hookStatus(
            symbol, stubAddress != 0, stubShapeMatched, false,
            symbolSize, expectedStubSize);
        return false;
    }
    if (!stubShapeMatched || !targetSignatureMatched) {
        runtime_log::warn(
            "draw-revision branch/wildcard signature differed; attempting resolved branch-target hook");
    }
    hook = GlossHook(
        reinterpret_cast<void*>(targetAddress), replacement, original);
    const bool installed = hook != nullptr && *original != nullptr;
    runtime_log::hookStatus(
        symbol, true, stubShapeMatched && targetSignatureMatched,
        installed, symbolSize, expectedStubSize);
    return installed;
}

// Wildcard signatures. A 0x00 mask byte means "ignore this byte".
// Every pattern below is unique in the supplied LeviLauncher 1.5.18
// libpreloader.so and was intentionally chosen to avoid patch-sensitive
// branch/ADR or otherwise volatile instruction bytes.
constexpr u8 kInfosSignature[] = {
    0xFF,0x43,0x02,0xD1, 0xFD,0x7B,0x05,0xA9,
    0xF7,0x33,0x00,0xF9, 0xF6,0x57,0x07,0xA9,
    0xF4,0x4F,0x08,0xA9, 0xFD,0x43,0x01,0x91,
    0x00,0x00,0x00,0x00, 0xF3,0x03,0x00,0xAA,
};
constexpr u8 kInfosMask[] = {
    0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,
    0x00,0x00,0x00,0x00, 0xFF,0xFF,0xFF,0xFF,
};

constexpr u8 kToggleSignature[] = {
    0xFD,0x7B,0xBD,0xA9, 0xF5,0x0B,0x00,0xF9,
    0xF4,0x4F,0x02,0xA9, 0xFD,0x03,0x00,0x91,
    0x00,0x00,0x00,0x00, 0x08,0x00,0x40,0xF9,
    0xF3,0x03,0x02,0xAA, 0xE1,0x03,0x02,0xAA,
};
constexpr u8 kToggleMask[] = {
    0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,
    0x00,0x00,0x00,0x00, 0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,
};

constexpr u8 kConfigSignature[] = {
    0xFD,0x7B,0xBB,0xA9, 0xF9,0x0B,0x00,0xF9,
    0xF8,0x5F,0x02,0xA9, 0xF6,0x57,0x03,0xA9,
    0xF4,0x4F,0x04,0xA9, 0xFD,0x03,0x00,0x91,
    0x00,0x00,0x00,0x00, 0xF5,0x03,0x03,0xAA,
    0x00,0x00,0x00,0x00,
};
constexpr u8 kConfigMask[] = {
    0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,
    0x00,0x00,0x00,0x00, 0xFF,0xFF,0xFF,0xFF,
    0x00,0x00,0x00,0x00,
};

constexpr u8 kDrawSignature[] = {
    0xFF,0x43,0x05,0xD1, 0xFD,0x7B,0x0F,0xA9,
    0x00,0x00,0x00,0x00, 0xFA,0x67,0x11,0xA9,
    0xF8,0x5F,0x12,0xA9, 0xF6,0x57,0x13,0xA9,
};
constexpr u8 kDrawMask[] = {
    0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,
    0x00,0x00,0x00,0x00, 0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,
};

constexpr u8 kRevisionTargetSignature[] = {
    0x1F,0x20,0x03,0xD5, 0x00,0x00,0x00,0x00,
    0x00,0xFD,0xDF,0xC8, 0xC0,0x03,0x5F,0xD6,
};
constexpr u8 kRevisionTargetMask[] = {
    0xFF,0xFF,0xFF,0xFF, 0x00,0x00,0x00,0x00,
    0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,
};

constexpr u8 kButtonCountTargetSignature[] = {
    0xFD,0x7B,0xBE,0xA9, 0xF4,0x4F,0x01,0xA9,
    0xFD,0x03,0x00,0x91, 0x1F,0x20,0x03,0xD5,
    0x00,0x00,0x00,0x00, 0xE0,0x03,0x13,0xAA,
};
constexpr u8 kButtonCountTargetMask[] = {
    0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,
    0x00,0x00,0x00,0x00, 0xFF,0xFF,0xFF,0xFF,
};

constexpr u8 kButtonInfoSignature[] = {
    0xFD,0x7B,0xBB,0xA9, 0xFC,0x67,0x01,0xA9,
    0xF8,0x5F,0x02,0xA9, 0xF6,0x57,0x03,0xA9,
    0xF4,0x4F,0x04,0xA9, 0xFD,0x03,0x00,0x91,
};
constexpr u8 kButtonInfoMask[] = {
    0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,
};

constexpr u8 kButtonIconSignature[] = {
    0xFF,0x03,0x02,0xD1, 0xFD,0x7B,0x03,0xA9,
    0xF9,0x23,0x00,0xF9, 0xF8,0x5F,0x05,0xA9,
    0xF6,0x57,0x06,0xA9, 0xF4,0x4F,0x07,0xA9,
};
constexpr u8 kButtonIconMask[] = {
    0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,
};

constexpr u8 kButtonEventSignature[] = {
    0xE8,0x0F,0x1C,0xFC, 0xFD,0x7B,0x01,0xA9,
    0xF6,0x57,0x02,0xA9, 0xF4,0x4F,0x03,0xA9,
    0xFD,0x43,0x00,0x91, 0x00,0x00,0x00,0x00,
};
constexpr u8 kButtonEventMask[] = {
    0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF, 0x00,0x00,0x00,0x00,
};

} // namespace

bool install() noexcept {
    if (gState.hooksInstalled) {
        return true;
    }

    runtime_log::info("enable: initializing LeviLauncher 1.5.18 Mod Menu bridge");
    GlossInit(false);
    gState.preloaderHandle = GlossOpen("libpreloader.so");
    if (!gState.preloaderHandle) {
        runtime_log::warn(
            "GlossOpen(libpreloader.so) failed; using address-based GlossSymbolEx fallback");
    }

    void* original = nullptr;
    if (!resolveAndHook(kInfosSymbol, 464, kInfosSignature, kInfosMask, sizeof(kInfosSignature),
            reinterpret_cast<void*>(&hookGetInfos), &original, gState.infosHook)) {
        runtime_log::error("enable: nativeGetExternalModsInfo hook failed");
        uninstall();
        return false;
    }
    gState.originalInfos = reinterpret_cast<GetExternalModsInfoFn>(original);

    original = nullptr;
    if (!resolveAndHook(kConfigSymbol, 316, kConfigSignature, kConfigMask, sizeof(kConfigSignature),
            reinterpret_cast<void*>(&hookConfig), &original, gState.configHook)) {
        runtime_log::error("enable: nativeSetExternalModConfig hook failed");
        uninstall();
        return false;
    }
    gState.originalConfig = reinterpret_cast<SetExternalModConfigFn>(original);

    original = nullptr;
    if (!resolveAndHook(kDrawSymbol, 2416, kDrawSignature, kDrawMask, sizeof(kDrawSignature),
            reinterpret_cast<void*>(&hookGetDrawCommands), &original, gState.drawHook)) {
        runtime_log::error("enable: nativeGetDrawCommands hook failed");
        uninstall();
        return false;
    }
    gState.originalDraw = reinterpret_cast<GetDrawCommandsFn>(original);

    original = nullptr;
    if (!resolveBranchTargetAndHook(
            kButtonCountSymbol, 4,
            kButtonCountTargetSignature, kButtonCountTargetMask,
            sizeof(kButtonCountTargetSignature),
            reinterpret_cast<void*>(&hookGetExternalButtonCount), &original,
            gState.buttonCountHook)) {
        runtime_log::error("enable: external button count hook failed");
        uninstall();
        return false;
    }
    gState.originalButtonCount =
        reinterpret_cast<GetExternalButtonCountFn>(original);

    original = nullptr;
    if (!resolveAndHook(
            kButtonInfoSymbol, 5264,
            kButtonInfoSignature, kButtonInfoMask,
            sizeof(kButtonInfoSignature),
            reinterpret_cast<void*>(&hookGetExternalButtonInfo), &original,
            gState.buttonInfoHook)) {
        runtime_log::error("enable: external button info hook failed");
        uninstall();
        return false;
    }
    gState.originalButtonInfo =
        reinterpret_cast<GetExternalButtonInfoFn>(original);

    original = nullptr;
    if (!resolveAndHook(
            kButtonIconSymbol, 460,
            kButtonIconSignature, kButtonIconMask,
            sizeof(kButtonIconSignature),
            reinterpret_cast<void*>(&hookGetExternalButtonIconBytes), &original,
            gState.buttonIconHook)) {
        runtime_log::error("enable: external button icon hook failed");
        uninstall();
        return false;
    }
    gState.originalButtonIcon =
        reinterpret_cast<GetExternalButtonIconBytesFn>(original);

    original = nullptr;
    if (!resolveAndHook(
            kButtonEventSymbol, 140,
            kButtonEventSignature, kButtonEventMask,
            sizeof(kButtonEventSignature),
            reinterpret_cast<void*>(&hookDispatchExternalButtonEvent), &original,
            gState.buttonEventHook)) {
        runtime_log::error("enable: external button event hook failed");
        uninstall();
        return false;
    }
    gState.originalButtonEvent =
        reinterpret_cast<DispatchExternalButtonEventFn>(original);

    original = nullptr;
    if (!resolveBranchTargetAndHook(
            kRevisionSymbol, 4,
            kRevisionTargetSignature, kRevisionTargetMask,
            sizeof(kRevisionTargetSignature),
            reinterpret_cast<void*>(&hookGetDrawCommandsRevision), &original,
            gState.revisionHook)) {
        runtime_log::error("enable: draw revision target hook failed");
        uninstall();
        return false;
    }
    gState.originalRevision =
        reinterpret_cast<RuntimeGetDrawCommandsRevisionFn>(original);

    original = nullptr;
    if (resolveAndHook(kToggleSymbol, 128, kToggleSignature, kToggleMask, sizeof(kToggleSignature),
            reinterpret_cast<void*>(&hookToggle), &original, gState.toggleHook)) {
        gState.originalToggle = reinterpret_cast<ToggleExternalModFn>(original);
    } else {
        runtime_log::warn(
            "enable: toggle hook unavailable; module remains enabled and settings/actions still work");
    }

    gState.hooksInstalled = true;
    runtime_log::info("enable: GoogleUI bridge installed successfully");
    return true;
}

void uninstall() noexcept {
    if (gState.buttonEventHook) GlossHookDelete(gState.buttonEventHook);
    if (gState.buttonIconHook) GlossHookDelete(gState.buttonIconHook);
    if (gState.buttonInfoHook) GlossHookDelete(gState.buttonInfoHook);
    if (gState.buttonCountHook) GlossHookDelete(gState.buttonCountHook);
    if (gState.drawHook) GlossHookDelete(gState.drawHook);
    if (gState.revisionHook) GlossHookDelete(gState.revisionHook);
    if (gState.configHook) GlossHookDelete(gState.configHook);
    if (gState.toggleHook) GlossHookDelete(gState.toggleHook);
    if (gState.infosHook) GlossHookDelete(gState.infosHook);
    gState.drawHook = nullptr;
    gState.buttonEventHook = nullptr;
    gState.buttonIconHook = nullptr;
    gState.buttonInfoHook = nullptr;
    gState.buttonCountHook = nullptr;
    gState.revisionHook = nullptr;
    gState.configHook = nullptr;
    gState.toggleHook = nullptr;
    gState.infosHook = nullptr;
    gState.originalDraw = nullptr;
    gState.originalButtonEvent = nullptr;
    gState.originalButtonIcon = nullptr;
    gState.originalButtonInfo = nullptr;
    gState.originalButtonCount = nullptr;
    gState.originalRevision = nullptr;
    gState.originalConfig = nullptr;
    gState.originalToggle = nullptr;
    gState.originalInfos = nullptr;
    gState.syntheticDrawRevision = 0;
    gState.buttonPaddingFixed = false;
    gState.hooksInstalled = false;
    if (gState.preloaderHandle) {
        (void)GlossClose(gState.preloaderHandle, false);
        gState.preloaderHandle = nullptr;
    }
}

} // namespace google_ui::hooks
