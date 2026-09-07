#include "AndroidWebViewController.hpp"

#include "ActivityResolver.hpp"
#include "AdBlocker.hpp"
#include "GoogleChrome.hpp"
#include "JniHelpers.hpp"
#include "ResponsiveLayout.hpp"
#include "SettingsStorage.hpp"
#include "RuntimeState.hpp"
#include "WebViewCommon.hpp"

namespace google_ui::webview {
namespace {

inline constexpr jint kAndroidContentViewId = 0x01020002;
inline constexpr jint kMatchParent = -1;
inline constexpr jfloat kBrowserElevation = 90.0F;

bool removeView(JNIEnv* env, jobject root, jobject view) noexcept {
    if (!env || !root || !view) {
        return true;
    }
    jclass viewGroupClass = env->FindClass("android/view/ViewGroup");
    const jmethodID remove = viewGroupClass
        ? env->GetMethodID(viewGroupClass, "removeView", "(Landroid/view/View;)V")
        : nullptr;
    bool ok = viewGroupClass && remove && !jni::clearException(env);
    if (ok) {
        env->CallVoidMethod(root, remove, view);
        ok = !jni::clearException(env);
    }
    jni::deleteLocal(env, viewGroupClass);
    return ok;
}

void clearGlobalRefs(JNIEnv* env) noexcept {
    if (!env) {
        return;
    }
    if (gState.webView) {
        env->DeleteGlobalRef(gState.webView);
    }
    if (gState.titleBarWebView) {
        env->DeleteGlobalRef(gState.titleBarWebView);
    }
    if (gState.container) {
        env->DeleteGlobalRef(gState.container);
    }
    if (gState.root) {
        env->DeleteGlobalRef(gState.root);
    }
    if (gState.activity) {
        env->DeleteGlobalRef(gState.activity);
    }
    gState.webView = nullptr;
    gState.titleBarWebView = nullptr;
    gState.container = nullptr;
    gState.root = nullptr;
    gState.activity = nullptr;
}

bool applyCurrentGeometry(JNIEnv* env) noexcept {
    if (!env || !gState.container || !gState.titleBarWebView || !gState.webView) {
        return false;
    }
    layout::clampWindow();
    const int totalHeight = gState.titleHeight + gState.height;
    const bool containerOk = webview_common::applyGeometry(
        env,
        gState.container,
        gState.x,
        gState.y,
        gState.width,
        totalHeight,
        1.0F);
    const bool titleOk = webview_common::applyChildGeometry(
        env,
        gState.titleBarWebView,
        gState.width,
        gState.titleHeight,
        0,
        0);
    const bool browserOk = webview_common::applyChildGeometry(
        env,
        gState.webView,
        gState.width,
        gState.height,
        0,
        gState.titleHeight);
    return containerOk && titleOk && browserOk;
}

} // namespace

bool isAlive() noexcept {
    return gState.container != nullptr && gState.titleBarWebView != nullptr &&
           gState.webView != nullptr;
}

bool isVisible() noexcept {
    return isAlive() && gState.uiVisible;
}

bool show(JNIEnv* env) noexcept {
    if (!env || !jni::isMainThread(env)) {
        return false;
    }
    (void)settings::load(env);
    if (isAlive()) {
        gState.uiVisible = true;
        return webview_common::setVisibility(env, gState.container, true) &&
               updateGeometry(env);
    }
    jobject activityObject = gState.buttonActivity
        ? env->NewLocalRef(gState.buttonActivity)
        : activity::resolve(env);
    if (!activityObject) {
        return false;
    }
    jclass activityClass = env->GetObjectClass(activityObject);
    const jmethodID findViewById = activityClass
        ? env->GetMethodID(activityClass, "findViewById", "(I)Landroid/view/View;")
        : nullptr;
    jobject rootObject = gState.buttonRoot
        ? env->NewLocalRef(gState.buttonRoot)
        : (findViewById
               ? env->CallObjectMethod(activityObject, findViewById, kAndroidContentViewId)
               : nullptr);
    if (!activityClass || !rootObject || jni::clearException(env) ||
        !layout::refresh(env, activityObject, rootObject, true)) {
        jni::deleteLocal(env, rootObject);
        jni::deleteLocal(env, activityClass);
        jni::deleteLocal(env, activityObject);
        return false;
    }

    jclass frameClass = env->FindClass("android/widget/FrameLayout");
    const jmethodID frameConstructor = frameClass
        ? env->GetMethodID(frameClass, "<init>", "(Landroid/content/Context;)V")
        : nullptr;
    jobject containerObject = frameConstructor
        ? env->NewObject(frameClass, frameConstructor, activityObject)
        : nullptr;

    jclass webViewClass = env->FindClass("android/webkit/WebView");
    const jmethodID webViewConstructor = webViewClass
        ? env->GetMethodID(webViewClass, "<init>", "(Landroid/content/Context;)V")
        : nullptr;
    jobject titleObject = webViewConstructor
        ? env->NewObject(webViewClass, webViewConstructor, activityObject)
        : nullptr;
    jobject browserObject = webViewConstructor
        ? env->NewObject(webViewClass, webViewConstructor, activityObject)
        : nullptr;

    jclass paramsClass = env->FindClass("android/widget/FrameLayout$LayoutParams");
    const jmethodID paramsConstructor = paramsClass
        ? env->GetMethodID(paramsClass, "<init>", "(II)V")
        : nullptr;
    jobject fillParams = paramsConstructor
        ? env->NewObject(paramsClass, paramsConstructor, kMatchParent, kMatchParent)
        : nullptr;
    jclass viewGroupClass = env->FindClass("android/view/ViewGroup");
    const jmethodID addView = viewGroupClass
        ? env->GetMethodID(
              viewGroupClass,
              "addView",
              "(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V")
        : nullptr;

    bool ok = frameClass && frameConstructor && containerObject && webViewClass &&
              webViewConstructor && titleObject && browserObject && paramsClass &&
              paramsConstructor && fillParams && viewGroupClass && addView &&
              !jni::clearException(env);
    if (ok) {
        ok = webview_common::configureLocalUi(
                 env,
                 titleObject,
                 static_cast<jint>(0xFF252525U)) &&
             webview_common::configureBrowser(env, browserObject) &&
             webview_common::makeTouchBlocking(env, containerObject);
    }
    if (ok) {
        env->CallVoidMethod(containerObject, addView, titleObject, fillParams);
        env->CallVoidMethod(containerObject, addView, browserObject, fillParams);
        env->CallVoidMethod(rootObject, addView, containerObject, fillParams);
        ok = !jni::clearException(env) &&
             webview_common::setElevation(env, containerObject, kBrowserElevation);
    }
    if (ok) {
        gState.activity = env->NewGlobalRef(activityObject);
        gState.root = env->NewGlobalRef(rootObject);
        gState.container = env->NewGlobalRef(containerObject);
        gState.titleBarWebView = env->NewGlobalRef(titleObject);
        gState.webView = env->NewGlobalRef(browserObject);
        ok = gState.activity && gState.root && gState.container &&
             gState.titleBarWebView && gState.webView && !jni::clearException(env);
    }
    if (ok) {
        gState.uiVisible = true;
        ok = chrome::initialize(env) && applyCurrentGeometry(env) &&
             adblock::installClient(env, gState.activity, gState.webView, adblock::Target::Google) &&
             webview_common::loadUrl(env, gState.webView, kHardcodedUrl);
        if (ok) {
            (void)adblock::initialize(env, adblock::Target::Google);
        }
    }
    if (!ok) {
        adblock::releaseClient(env, adblock::Target::Google);
        (void)removeView(
            env,
            gState.root ? gState.root : rootObject,
            gState.container ? gState.container : containerObject);
        if (gState.webView) {
            (void)webview_common::destroy(env, gState.webView);
        }
        if (gState.titleBarWebView) {
            (void)webview_common::destroy(env, gState.titleBarWebView);
        }
        clearGlobalRefs(env);
        gState.uiVisible = false;
    }

    jni::deleteLocal(env, viewGroupClass);
    jni::deleteLocal(env, fillParams);
    jni::deleteLocal(env, paramsClass);
    jni::deleteLocal(env, browserObject);
    jni::deleteLocal(env, titleObject);
    jni::deleteLocal(env, webViewClass);
    jni::deleteLocal(env, containerObject);
    jni::deleteLocal(env, frameClass);
    jni::deleteLocal(env, rootObject);
    jni::deleteLocal(env, activityClass);
    jni::deleteLocal(env, activityObject);
    return ok;
}

bool hide(JNIEnv* env) noexcept {
    if (!isAlive()) {
        gState.uiVisible = false;
        return true;
    }
    if (!env || !jni::isMainThread(env)) {
        return false;
    }
    gState.uiVisible = false;
    return webview_common::setVisibility(env, gState.container, false);
}

bool destroy(JNIEnv* env) noexcept {
    if (!isAlive()) {
        gState.uiVisible = false;
        return true;
    }
    if (!env || !jni::isMainThread(env)) {
        return false;
    }
    const bool removed = removeView(env, gState.root, gState.container);
    adblock::releaseClient(env, adblock::Target::Google);
    const bool browserDestroyed = webview_common::destroy(env, gState.webView);
    const bool titleDestroyed = webview_common::destroy(env, gState.titleBarWebView);
    clearGlobalRefs(env);
    gState.uiVisible = false;
    return removed && browserDestroyed && titleDestroyed;
}

bool updateGeometry(JNIEnv* env) noexcept {
    if (!env || !isAlive() || !jni::isMainThread(env)) {
        return false;
    }
    return applyCurrentGeometry(env);
}

void pollResolution(JNIEnv* env) noexcept {
    if (!env || !isAlive() || !gState.activity || !gState.root) {
        return;
    }
    const int oldWidth = gState.rootWidth;
    const int oldHeight = gState.rootHeight;
    if (layout::refresh(env, gState.activity, gState.root, false) &&
        (oldWidth != gState.rootWidth || oldHeight != gState.rootHeight)) {
        if (gState.uiVisible) {
            (void)updateGeometry(env);
        }
    }
}

} // namespace google_ui::webview
