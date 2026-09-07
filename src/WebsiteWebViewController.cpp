#include "WebsiteWebViewController.hpp"

#include "ActivityResolver.hpp"
#include "AdBlocker.hpp"
#include "WebsiteChrome.hpp"
#include "JniHelpers.hpp"
#include "ResponsiveLayout.hpp"
#include "SettingsStorage.hpp"
#include "RuntimeState.hpp"
#include "WebViewCommon.hpp"

namespace google_ui::website_webview {
namespace {

inline constexpr jint kAndroidContentViewId = 0x01020002;
inline constexpr jint kMatchParent = -1;
inline constexpr jfloat kBrowserElevation = 96.0F;

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
    if (gState.websiteWebView) {
        env->DeleteGlobalRef(gState.websiteWebView);
    }
    if (gState.websiteTitleBarWebView) {
        env->DeleteGlobalRef(gState.websiteTitleBarWebView);
    }
    if (gState.websiteContainer) {
        env->DeleteGlobalRef(gState.websiteContainer);
    }
    if (gState.websiteRoot) {
        env->DeleteGlobalRef(gState.websiteRoot);
    }
    if (gState.websiteActivity) {
        env->DeleteGlobalRef(gState.websiteActivity);
    }
    gState.websiteWebView = nullptr;
    gState.websiteTitleBarWebView = nullptr;
    gState.websiteContainer = nullptr;
    gState.websiteRoot = nullptr;
    gState.websiteActivity = nullptr;
}

bool applyCurrentGeometry(JNIEnv* env) noexcept {
    if (!env || !gState.websiteContainer || !gState.websiteTitleBarWebView || !gState.websiteWebView) {
        return false;
    }
    layout::clampWindow();
    const int totalHeight = gState.titleHeight + gState.height;
    const bool containerOk = webview_common::applyGeometry(
        env,
        gState.websiteContainer,
        gState.x,
        gState.y,
        gState.width,
        totalHeight,
        1.0F);
    const bool titleOk = webview_common::applyChildGeometry(
        env,
        gState.websiteTitleBarWebView,
        gState.width,
        gState.titleHeight,
        0,
        0);
    const bool browserOk = webview_common::applyChildGeometry(
        env,
        gState.websiteWebView,
        gState.width,
        gState.height,
        0,
        gState.titleHeight);
    return containerOk && titleOk && browserOk;
}

} // namespace

bool isAlive() noexcept {
    return gState.websiteContainer != nullptr && gState.websiteTitleBarWebView != nullptr &&
           gState.websiteWebView != nullptr;
}

bool isVisible() noexcept {
    return isAlive() && gState.websiteUiVisible;
}

bool show(JNIEnv* env) noexcept {
    if (!env || !jni::isMainThread(env)) {
        return false;
    }
    (void)settings::load(env);
    if (isAlive()) {
        gState.websiteUiVisible = true;
        return webview_common::setVisibility(env, gState.websiteContainer, true) &&
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
        gState.websiteActivity = env->NewGlobalRef(activityObject);
        gState.websiteRoot = env->NewGlobalRef(rootObject);
        gState.websiteContainer = env->NewGlobalRef(containerObject);
        gState.websiteTitleBarWebView = env->NewGlobalRef(titleObject);
        gState.websiteWebView = env->NewGlobalRef(browserObject);
        ok = gState.websiteActivity && gState.websiteRoot && gState.websiteContainer &&
             gState.websiteTitleBarWebView && gState.websiteWebView && !jni::clearException(env);
    }
    if (ok) {
        gState.websiteUiVisible = true;
        ok = website_chrome::initialize(env) && applyCurrentGeometry(env) &&
             adblock::installClient(env, gState.websiteActivity, gState.websiteWebView, adblock::Target::Website) &&
             webview_common::loadUrl(env, gState.websiteWebView, gState.websiteUrl);
        if (ok) {
            (void)adblock::initialize(env, adblock::Target::Website);
        }
    }
    if (!ok) {
        adblock::releaseClient(env, adblock::Target::Website);
        (void)removeView(
            env,
            gState.websiteRoot ? gState.websiteRoot : rootObject,
            gState.websiteContainer ? gState.websiteContainer : containerObject);
        if (gState.websiteWebView) {
            (void)webview_common::destroy(env, gState.websiteWebView);
        }
        if (gState.websiteTitleBarWebView) {
            (void)webview_common::destroy(env, gState.websiteTitleBarWebView);
        }
        clearGlobalRefs(env);
        gState.websiteUiVisible = false;
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
        gState.websiteUiVisible = false;
        return true;
    }
    if (!env || !jni::isMainThread(env)) {
        return false;
    }
    gState.websiteUiVisible = false;
    return webview_common::setVisibility(env, gState.websiteContainer, false);
}

bool toggle(JNIEnv* env) noexcept {
    return isVisible() ? hide(env) : show(env);
}

bool reloadConfiguredUrl(JNIEnv* env) noexcept {
    if (!env || !isAlive() || !jni::isMainThread(env) || !gState.websiteUrl[0]) {
        return false;
    }
    const bool loaded = webview_common::loadUrl(env, gState.websiteWebView, gState.websiteUrl);
    if (loaded) {
        (void)adblock::initialize(env, adblock::Target::Website);
    }
    return loaded;
}

bool destroy(JNIEnv* env) noexcept {
    if (!isAlive()) {
        gState.websiteUiVisible = false;
        return true;
    }
    if (!env || !jni::isMainThread(env)) {
        return false;
    }
    const bool removed = removeView(env, gState.websiteRoot, gState.websiteContainer);
    adblock::releaseClient(env, adblock::Target::Website);
    const bool browserDestroyed = webview_common::destroy(env, gState.websiteWebView);
    const bool titleDestroyed = webview_common::destroy(env, gState.websiteTitleBarWebView);
    clearGlobalRefs(env);
    gState.websiteUiVisible = false;
    return removed && browserDestroyed && titleDestroyed;
}

bool updateGeometry(JNIEnv* env) noexcept {
    if (!env || !isAlive() || !jni::isMainThread(env)) {
        return false;
    }
    return applyCurrentGeometry(env);
}

void pollResolution(JNIEnv* env) noexcept {
    if (!env || !isAlive() || !gState.websiteActivity || !gState.websiteRoot) {
        return;
    }
    const int oldWidth = gState.rootWidth;
    const int oldHeight = gState.rootHeight;
    if (layout::refresh(env, gState.websiteActivity, gState.websiteRoot, false) &&
        (oldWidth != gState.rootWidth || oldHeight != gState.rootHeight)) {
        if (gState.websiteUiVisible) {
            (void)updateGeometry(env);
        }
    }
}

} // namespace google_ui::website_webview
