#include "AdBlocker.hpp"

#include "AdBlockClientDex.hpp"
#include "AdBlockData.hpp"
#include "JniHelpers.hpp"
#include "RuntimeState.hpp"
#include "WebViewCommon.hpp"

namespace google_ui::adblock {
namespace {

inline constexpr int kReinjectInterval = 8;
inline constexpr const char* kClientClassName =
    "org.levimc.googleui.AdBlockWebViewClient";

char lowerAscii(char value) noexcept {
    return value >= 'A' && value <= 'Z'
        ? static_cast<char>(value + ('a' - 'A'))
        : value;
}

usize stringLength(const char* value) noexcept {
    usize length = 0;
    while (value && value[length]) {
        ++length;
    }
    return length;
}

bool equalsInsensitive(const char* left, usize leftLength, const char* right) noexcept {
    if (!left || !right || stringLength(right) != leftLength) {
        return false;
    }
    for (usize index = 0; index < leftLength; ++index) {
        if (lowerAscii(left[index]) != lowerAscii(right[index])) {
            return false;
        }
    }
    return true;
}

bool containsInsensitive(const char* value, const char* token) noexcept {
    if (!value || !token || !*token) {
        return false;
    }
    for (usize offset = 0; value[offset]; ++offset) {
        usize index = 0;
        while (token[index] && value[offset + index] &&
               lowerAscii(value[offset + index]) == lowerAscii(token[index])) {
            ++index;
        }
        if (!token[index]) {
            return true;
        }
    }
    return false;
}

bool endsWithHost(const char* host, usize hostLength, const char* domain) noexcept {
    const usize domainLength = stringLength(domain);
    if (!host || domainLength == 0 || hostLength < domainLength) {
        return false;
    }
    const usize offset = hostLength - domainLength;
    if (!equalsInsensitive(host + offset, domainLength, domain)) {
        return false;
    }
    return offset == 0 || host[offset - 1] == '.';
}

bool isTrackingLabel(const char* value, usize length) noexcept {
    constexpr const char* kLabels[] = {
        "ad", "ads", "adserver", "adservice", "adservices", "advertising",
        "analytics", "beacon", "banners", "clicktrack", "metrics", "pixel",
        "promo", "sponsor", "sponsored", "telemetry", "tracker", "tracking",
    };
    for (const char* label : kLabels) {
        if (equalsInsensitive(value, length, label)) {
            return true;
        }
    }
    return false;
}

bool hostHasTrackingLabel(const char* host, usize hostLength) noexcept {
    usize labelStart = 0;
    for (usize index = 0; index <= hostLength; ++index) {
        if (index == hostLength || host[index] == '.') {
            if (index > labelStart && isTrackingLabel(host + labelStart, index - labelStart)) {
                return true;
            }
            labelStart = index + 1;
        }
    }
    return false;
}

bool shouldBlockUrl(const char* url) noexcept {
    if (!url || !*url) {
        return false;
    }
    const char* scheme = url;
    while (*scheme && *scheme != ':') {
        ++scheme;
    }
    if (*scheme != ':' || scheme[1] != '/' || scheme[2] != '/') {
        return false;
    }
    const usize schemeLength = static_cast<usize>(scheme - url);
    if (!equalsInsensitive(url, schemeLength, "http") &&
        !equalsInsensitive(url, schemeLength, "https")) {
        return false;
    }

    const char* host = scheme + 3;
    const char* hostEnd = host;
    while (*hostEnd && *hostEnd != '/' && *hostEnd != ':' &&
           *hostEnd != '?' && *hostEnd != '#') {
        ++hostEnd;
    }
    const usize hostLength = static_cast<usize>(hostEnd - host);
    if (hostLength == 0) {
        return false;
    }

    for (usize index = 0; index < adblock_data::kBlockedRuleCount; ++index) {
        const char* rule = adblock_data::kBlockedRules[index];
        if (!rule || !*rule) {
            continue;
        }
        if (containsInsensitive(rule, "/")) {
            if (containsInsensitive(url, rule)) {
                return true;
            }
        } else if (endsWithHost(host, hostLength, rule)) {
            return true;
        }
    }
    for (usize index = 0; index < adblock_data::kBlockedUrlTokenCount; ++index) {
        if (containsInsensitive(url, adblock_data::kBlockedUrlTokens[index])) {
            return true;
        }
    }
    return hostHasTrackingLabel(host, hostLength);
}

jboolean JNICALL nativeShouldBlock(JNIEnv* env, jclass, jstring urlValue) noexcept {
    if (!env || !urlValue) {
        return JNI_FALSE;
    }
    const char* url = env->GetStringUTFChars(urlValue, nullptr);
    if (!url || jni::clearException(env)) {
        return JNI_FALSE;
    }
    const bool blocked = shouldBlockUrl(url);
    env->ReleaseStringUTFChars(urlValue, url);
    return blocked ? JNI_TRUE : JNI_FALSE;
}

bool setWebViewClient(JNIEnv* env, jobject webView, jobject client) noexcept {
    if (!env || !webView || !client) {
        return false;
    }
    jclass webViewClass = env->GetObjectClass(webView);
    const jmethodID setClient = webViewClass
        ? env->GetMethodID(
              webViewClass,
              "setWebViewClient",
              "(Landroid/webkit/WebViewClient;)V")
        : nullptr;
    bool ok = webViewClass && setClient && !jni::clearException(env);
    if (ok) {
        env->CallVoidMethod(webView, setClient, client);
        ok = !jni::clearException(env);
    }
    jni::deleteLocal(env, webViewClass);
    return ok;
}

jobject& loaderRef(Target target) noexcept {
    return target == Target::Website
        ? gState.websiteAdBlockClassLoader
        : gState.adBlockClassLoader;
}

jclass& classRef(Target target) noexcept {
    return target == Target::Website
        ? gState.websiteAdBlockClientClass
        : gState.adBlockClientClass;
}

jobject& clientRef(Target target) noexcept {
    return target == Target::Website
        ? gState.websiteAdBlockClient
        : gState.adBlockClient;
}

jobject targetWebView(Target target) noexcept {
    return target == Target::Website ? gState.websiteWebView : gState.webView;
}

bool targetVisible(Target target) noexcept {
    return target == Target::Website ? gState.websiteUiVisible : gState.uiVisible;
}

int& targetPollCounter(Target target) noexcept {
    return target == Target::Website
        ? gState.websiteAdBlockPollCounter
        : gState.adBlockPollCounter;
}

bool inject(JNIEnv* env, Target target) noexcept {
    jobject webView = targetWebView(target);
    return env && webView && targetVisible(target) &&
           webview_common::loadUrl(
               env,
               webView,
               reinterpret_cast<const char*>(adblock_data::kUniversalScript));
}

void pollTarget(JNIEnv* env, Target target) noexcept {
    if (!env || !targetWebView(target) || !targetVisible(target)) {
        return;
    }
    int& counter = targetPollCounter(target);
    ++counter;
    if (counter >= kReinjectInterval) {
        counter = 0;
        (void)inject(env, target);
    }
}

} // namespace

void releaseClient(JNIEnv* env, Target target) noexcept {
    if (!env) {
        return;
    }
    if (clientRef(target)) {
        env->DeleteGlobalRef(clientRef(target));
    }
    if (classRef(target)) {
        env->DeleteGlobalRef(classRef(target));
    }
    if (loaderRef(target)) {
        env->DeleteGlobalRef(loaderRef(target));
    }
    clientRef(target) = nullptr;
    classRef(target) = nullptr;
    loaderRef(target) = nullptr;
}

bool installClient(
    JNIEnv* env,
    jobject activity,
    jobject webView,
    Target target) noexcept {
    if (!env || !activity || !webView || adblock_dex::kDexSize == 0) {
        return false;
    }
    releaseClient(env, target);

    jbyteArray dexArray = env->NewByteArray(static_cast<jsize>(adblock_dex::kDexSize));
    if (dexArray) {
        env->SetByteArrayRegion(
            dexArray,
            0,
            static_cast<jsize>(adblock_dex::kDexSize),
            reinterpret_cast<const jbyte*>(adblock_dex::kDexBytes));
    }

    jclass byteBufferClass = env->FindClass("java/nio/ByteBuffer");
    const jmethodID wrap = byteBufferClass
        ? env->GetStaticMethodID(
              byteBufferClass,
              "wrap",
              "([B)Ljava/nio/ByteBuffer;")
        : nullptr;
    jobject byteBuffer = byteBufferClass && wrap && dexArray
        ? env->CallStaticObjectMethod(byteBufferClass, wrap, dexArray)
        : nullptr;

    jclass activityClass = env->GetObjectClass(activity);
    const jmethodID getClassLoader = activityClass
        ? env->GetMethodID(
              activityClass,
              "getClassLoader",
              "()Ljava/lang/ClassLoader;")
        : nullptr;
    jobject parentLoader = activityClass && getClassLoader
        ? env->CallObjectMethod(activity, getClassLoader)
        : nullptr;

    jclass loaderClass = env->FindClass("dalvik/system/InMemoryDexClassLoader");
    const jmethodID loaderConstructor = loaderClass
        ? env->GetMethodID(
              loaderClass,
              "<init>",
              "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V")
        : nullptr;
    jobject loader = loaderClass && loaderConstructor && byteBuffer && parentLoader
        ? env->NewObject(loaderClass, loaderConstructor, byteBuffer, parentLoader)
        : nullptr;

    jclass classLoaderClass = env->FindClass("java/lang/ClassLoader");
    const jmethodID loadClass = classLoaderClass
        ? env->GetMethodID(
              classLoaderClass,
              "loadClass",
              "(Ljava/lang/String;)Ljava/lang/Class;")
        : nullptr;
    jstring className = jni::makeString(env, kClientClassName);
    jobject loadedClassObject = classLoaderClass && loadClass && loader && className
        ? env->CallObjectMethod(loader, loadClass, className)
        : nullptr;
    jclass loadedClass = static_cast<jclass>(loadedClassObject);

    JNINativeMethod method = {
        .name = const_cast<char*>("nativeShouldBlock"),
        .signature = const_cast<char*>("(Ljava/lang/String;)Z"),
        .fnPtr = reinterpret_cast<void*>(&nativeShouldBlock),
    };
    bool ok = dexArray && byteBufferClass && wrap && byteBuffer && activityClass &&
              getClassLoader && parentLoader && loaderClass && loaderConstructor &&
              loader && classLoaderClass && loadClass && className && loadedClass &&
              !jni::clearException(env);
    if (ok) {
        ok = env->RegisterNatives(loadedClass, &method, 1) == JNI_OK &&
             !jni::clearException(env);
    }

    const jmethodID clientConstructor = ok
        ? env->GetMethodID(loadedClass, "<init>", "()V")
        : nullptr;
    jobject client = clientConstructor
        ? env->NewObject(loadedClass, clientConstructor)
        : nullptr;
    if (!clientConstructor || !client || jni::clearException(env)) {
        ok = false;
    }
    if (ok) {
        ok = setWebViewClient(env, webView, client);
    }
    if (ok) {
        loaderRef(target) = env->NewGlobalRef(loader);
        classRef(target) = static_cast<jclass>(env->NewGlobalRef(loadedClass));
        clientRef(target) = env->NewGlobalRef(client);
        ok = loaderRef(target) && classRef(target) && clientRef(target) &&
             !jni::clearException(env);
    }
    if (!ok) {
        releaseClient(env, target);
    }

    jni::deleteLocal(env, client);
    jni::deleteLocal(env, loadedClassObject);
    jni::deleteLocal(env, className);
    jni::deleteLocal(env, classLoaderClass);
    jni::deleteLocal(env, loader);
    jni::deleteLocal(env, loaderClass);
    jni::deleteLocal(env, parentLoader);
    jni::deleteLocal(env, activityClass);
    jni::deleteLocal(env, byteBuffer);
    jni::deleteLocal(env, byteBufferClass);
    jni::deleteLocal(env, dexArray);
    return ok;
}

bool initialize(JNIEnv* env, Target target) noexcept {
    targetPollCounter(target) = 0;
    return inject(env, target);
}

void poll(JNIEnv* env) noexcept {
    pollTarget(env, Target::Google);
    pollTarget(env, Target::Website);
}

} // namespace google_ui::adblock
