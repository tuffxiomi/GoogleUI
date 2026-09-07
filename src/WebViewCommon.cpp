#include "WebViewCommon.hpp"

#include "JniHelpers.hpp"

namespace google_ui::webview_common {
namespace {

bool callBooleanSetting(
    JNIEnv* env,
    jobject settings,
    jclass settingsClass,
    const char* methodName,
    bool value,
    bool required = true) noexcept {
    if (!env || !settings || !settingsClass || !methodName) {
        return false;
    }
    const jmethodID method = env->GetMethodID(settingsClass, methodName, "(Z)V");
    if (!method || jni::clearException(env)) {
        return !required;
    }
    env->CallVoidMethod(settings, method, value ? JNI_TRUE : JNI_FALSE);
    return !jni::clearException(env);
}

bool callIntSetting(
    JNIEnv* env,
    jobject settings,
    jclass settingsClass,
    const char* methodName,
    int value,
    bool required = true) noexcept {
    if (!env || !settings || !settingsClass || !methodName) {
        return false;
    }
    const jmethodID method = env->GetMethodID(settingsClass, methodName, "(I)V");
    if (!method || jni::clearException(env)) {
        return !required;
    }
    env->CallVoidMethod(settings, method, static_cast<jint>(value));
    return !jni::clearException(env);
}

bool setBackground(JNIEnv* env, jobject view, jint color) noexcept {
    if (!env || !view) {
        return false;
    }
    jclass viewClass = env->GetObjectClass(view);
    const jmethodID setBackgroundColor = viewClass
        ? env->GetMethodID(viewClass, "setBackgroundColor", "(I)V")
        : nullptr;
    bool ok = viewClass && setBackgroundColor && !jni::clearException(env);
    if (ok) {
        env->CallVoidMethod(view, setBackgroundColor, color);
        ok = !jni::clearException(env);
    }
    jni::deleteLocal(env, viewClass);
    return ok;
}

bool copyJavaString(JNIEnv* env, jstring value, char* out, int capacity) noexcept {
    if (!env || !value || !out || capacity < 2) {
        return false;
    }
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (!chars || jni::clearException(env)) {
        return false;
    }
    int index = 0;
    while (chars[index] && index + 1 < capacity) {
        out[index] = chars[index];
        ++index;
    }
    out[index] = '\0';
    env->ReleaseStringUTFChars(value, chars);
    return true;
}

} // namespace

bool makeTouchBlocking(JNIEnv* env, jobject view) noexcept {
    if (!env || !view) {
        return false;
    }
    jclass viewClass = env->FindClass("android/view/View");
    const jmethodID clickable = viewClass
        ? env->GetMethodID(viewClass, "setClickable", "(Z)V")
        : nullptr;
    const jmethodID focusable = viewClass
        ? env->GetMethodID(viewClass, "setFocusable", "(Z)V")
        : nullptr;
    const jmethodID focusableTouch = viewClass
        ? env->GetMethodID(viewClass, "setFocusableInTouchMode", "(Z)V")
        : nullptr;
    bool ok = viewClass && clickable && focusable && focusableTouch &&
              !jni::clearException(env);
    if (ok) {
        env->CallVoidMethod(view, clickable, JNI_TRUE);
        env->CallVoidMethod(view, focusable, JNI_TRUE);
        env->CallVoidMethod(view, focusableTouch, JNI_TRUE);
        ok = !jni::clearException(env);
    }
    jni::deleteLocal(env, viewClass);
    return ok;
}

bool configureBrowser(JNIEnv* env, jobject webView) noexcept {
    if (!env || !webView) {
        return false;
    }
    jclass webViewClass = env->GetObjectClass(webView);
    const jmethodID getSettings = webViewClass
        ? env->GetMethodID(
              webViewClass,
              "getSettings",
              "()Landroid/webkit/WebSettings;")
        : nullptr;
    jobject settings = getSettings
        ? env->CallObjectMethod(webView, getSettings)
        : nullptr;
    jclass settingsClass = settings ? env->GetObjectClass(settings) : nullptr;
    bool ok = webViewClass && getSettings && settings && settingsClass &&
              !jni::clearException(env);
    if (ok) {
        ok = callBooleanSetting(env, settings, settingsClass, "setJavaScriptEnabled", true) && ok;
        ok = callBooleanSetting(env, settings, settingsClass, "setDomStorageEnabled", true) && ok;
        ok = callBooleanSetting(env, settings, settingsClass, "setDatabaseEnabled", true, false) && ok;
        ok = callBooleanSetting(env, settings, settingsClass, "setBuiltInZoomControls", true) && ok;
        ok = callBooleanSetting(env, settings, settingsClass, "setDisplayZoomControls", false) && ok;
        ok = callBooleanSetting(env, settings, settingsClass, "setSupportZoom", true) && ok;
        ok = callBooleanSetting(env, settings, settingsClass, "setSupportMultipleWindows", false, false) && ok;
        ok = callBooleanSetting(env, settings, settingsClass, "setJavaScriptCanOpenWindowsAutomatically", false, false) && ok;
        ok = callBooleanSetting(env, settings, settingsClass, "setAllowFileAccess", false) && ok;
        ok = callBooleanSetting(env, settings, settingsClass, "setAllowContentAccess", false) && ok;
        ok = callBooleanSetting(
                 env,
                 settings,
                 settingsClass,
                 "setMediaPlaybackRequiresUserGesture",
                 false,
                 false) && ok;
        ok = callIntSetting(env, settings, settingsClass, "setTextZoom", 100, false) && ok;
        ok = callIntSetting(env, settings, settingsClass, "setDefaultFontSize", 16, false) && ok;
    }

    const jmethodID setMixedContentMode = settingsClass
        ? env->GetMethodID(settingsClass, "setMixedContentMode", "(I)V")
        : nullptr;
    if (setMixedContentMode) {
        env->CallVoidMethod(settings, setMixedContentMode, 1);
        (void)jni::clearException(env);
    } else {
        (void)jni::clearException(env);
    }

    jclass clientClass = env->FindClass("android/webkit/WebViewClient");
    const jmethodID clientConstructor = clientClass
        ? env->GetMethodID(clientClass, "<init>", "()V")
        : nullptr;
    jobject client = clientConstructor
        ? env->NewObject(clientClass, clientConstructor)
        : nullptr;
    const jmethodID setClient = webViewClass
        ? env->GetMethodID(
              webViewClass,
              "setWebViewClient",
              "(Landroid/webkit/WebViewClient;)V")
        : nullptr;
    if (!clientClass || !clientConstructor || !client || !setClient ||
        jni::clearException(env)) {
        ok = false;
    } else {
        env->CallVoidMethod(webView, setClient, client);
        ok = ok && !jni::clearException(env);
    }

    // A real WebChromeClient improves HTML5 media/fullscreen plumbing in Android
    // WebView. Keep it optional so older WebView implementations cannot block
    // the entire browser from opening.
    jclass chromeClass = env->FindClass("android/webkit/WebChromeClient");
    const jmethodID chromeConstructor = chromeClass
        ? env->GetMethodID(chromeClass, "<init>", "()V")
        : nullptr;
    jobject chrome = chromeClass && chromeConstructor
        ? env->NewObject(chromeClass, chromeConstructor)
        : nullptr;
    const jmethodID setChromeClient = webViewClass
        ? env->GetMethodID(
              webViewClass,
              "setWebChromeClient",
              "(Landroid/webkit/WebChromeClient;)V")
        : nullptr;
    if (chrome && setChromeClient && !jni::clearException(env)) {
        env->CallVoidMethod(webView, setChromeClient, chrome);
        (void)jni::clearException(env);
    } else {
        (void)jni::clearException(env);
    }

    // Minecraft itself is a hardware-rendered Surface. Force the embedded
    // browser onto a hardware layer as well so inline HTML5 video is composited
    // instead of degenerating into an audio-only black rectangle on affected
    // Android WebView/GPU combinations. LAYER_TYPE_HARDWARE == 2.
    jclass mediaViewClass = env->FindClass("android/view/View");
    const jmethodID setLayerType = mediaViewClass
        ? env->GetMethodID(
              mediaViewClass,
              "setLayerType",
              "(ILandroid/graphics/Paint;)V")
        : nullptr;
    if (setLayerType && !jni::clearException(env)) {
        env->CallVoidMethod(webView, setLayerType, static_cast<jint>(2), nullptr);
        (void)jni::clearException(env);
    } else {
        (void)jni::clearException(env);
    }

    ok = setBackground(env, webView, static_cast<jint>(0xFFFFFFFFU)) && ok;
    ok = makeTouchBlocking(env, webView) && ok;

    jni::deleteLocal(env, mediaViewClass);
    jni::deleteLocal(env, chrome);
    jni::deleteLocal(env, chromeClass);
    jni::deleteLocal(env, client);
    jni::deleteLocal(env, clientClass);
    jni::deleteLocal(env, settingsClass);
    jni::deleteLocal(env, settings);
    jni::deleteLocal(env, webViewClass);
    return ok;
}

bool reload(JNIEnv* env, jobject webView) noexcept {
    if (!env || !webView) {
        return false;
    }
    jclass webViewClass = env->GetObjectClass(webView);
    const jmethodID method = webViewClass
        ? env->GetMethodID(webViewClass, "reload", "()V")
        : nullptr;
    bool ok = webViewClass && method && !jni::clearException(env);
    if (ok) {
        env->CallVoidMethod(webView, method);
        ok = !jni::clearException(env);
    }
    jni::deleteLocal(env, webViewClass);
    return ok;
}

bool configureLocalUi(JNIEnv* env, jobject webView, jint backgroundColor) noexcept {
    if (!env || !webView) {
        return false;
    }
    jclass webViewClass = env->GetObjectClass(webView);
    const jmethodID getSettings = webViewClass
        ? env->GetMethodID(
              webViewClass,
              "getSettings",
              "()Landroid/webkit/WebSettings;")
        : nullptr;
    jobject settings = getSettings
        ? env->CallObjectMethod(webView, getSettings)
        : nullptr;
    jclass settingsClass = settings ? env->GetObjectClass(settings) : nullptr;
    bool ok = webViewClass && getSettings && settings && settingsClass &&
              !jni::clearException(env);
    if (ok) {
        ok = callBooleanSetting(env, settings, settingsClass, "setJavaScriptEnabled", true) && ok;
        ok = callBooleanSetting(env, settings, settingsClass, "setDomStorageEnabled", false) && ok;
        ok = callBooleanSetting(env, settings, settingsClass, "setSupportZoom", false) && ok;
        ok = callBooleanSetting(env, settings, settingsClass, "setBuiltInZoomControls", false) && ok;
        ok = callBooleanSetting(env, settings, settingsClass, "setDisplayZoomControls", false) && ok;
        ok = callBooleanSetting(env, settings, settingsClass, "setAllowFileAccess", false) && ok;
        ok = callBooleanSetting(env, settings, settingsClass, "setAllowContentAccess", false) && ok;
    }

    jclass viewClass = env->FindClass("android/view/View");
    const jmethodID setVertical = viewClass
        ? env->GetMethodID(viewClass, "setVerticalScrollBarEnabled", "(Z)V")
        : nullptr;
    const jmethodID setHorizontal = viewClass
        ? env->GetMethodID(viewClass, "setHorizontalScrollBarEnabled", "(Z)V")
        : nullptr;
    const jmethodID setOverScrollMode = viewClass
        ? env->GetMethodID(viewClass, "setOverScrollMode", "(I)V")
        : nullptr;
    if (ok && viewClass && setVertical && setHorizontal && setOverScrollMode &&
        !jni::clearException(env)) {
        env->CallVoidMethod(webView, setVertical, JNI_FALSE);
        env->CallVoidMethod(webView, setHorizontal, JNI_FALSE);
        env->CallVoidMethod(webView, setOverScrollMode, 2);
        ok = !jni::clearException(env);
    } else if (viewClass) {
        (void)jni::clearException(env);
    }

    ok = setBackground(env, webView, backgroundColor) && ok;
    ok = makeTouchBlocking(env, webView) && ok;
    jni::deleteLocal(env, viewClass);
    jni::deleteLocal(env, settingsClass);
    jni::deleteLocal(env, settings);
    jni::deleteLocal(env, webViewClass);
    return ok;
}

bool applyGeometry(
    JNIEnv* env,
    jobject view,
    int x,
    int y,
    int width,
    int height,
    float alpha) noexcept {
    if (!env || !view || width <= 0 || height <= 0) {
        return false;
    }
    jclass viewClass = env->FindClass("android/view/View");
    jclass paramsClass = env->FindClass("android/widget/FrameLayout$LayoutParams");
    const jmethodID setX = viewClass
        ? env->GetMethodID(viewClass, "setX", "(F)V")
        : nullptr;
    const jmethodID setY = viewClass
        ? env->GetMethodID(viewClass, "setY", "(F)V")
        : nullptr;
    const jmethodID setAlpha = viewClass
        ? env->GetMethodID(viewClass, "setAlpha", "(F)V")
        : nullptr;
    const jmethodID setLayoutParams = viewClass
        ? env->GetMethodID(
              viewClass,
              "setLayoutParams",
              "(Landroid/view/ViewGroup$LayoutParams;)V")
        : nullptr;
    const jmethodID paramsConstructor = paramsClass
        ? env->GetMethodID(paramsClass, "<init>", "(II)V")
        : nullptr;
    jobject params = paramsConstructor
        ? env->NewObject(paramsClass, paramsConstructor, width, height)
        : nullptr;
    bool ok = viewClass && paramsClass && setX && setY && setAlpha &&
              setLayoutParams && paramsConstructor && params &&
              !jni::clearException(env);
    if (ok) {
        env->CallVoidMethod(view, setAlpha, static_cast<jfloat>(alpha));
        env->CallVoidMethod(view, setX, static_cast<jfloat>(x));
        env->CallVoidMethod(view, setY, static_cast<jfloat>(y));
        env->CallVoidMethod(view, setLayoutParams, params);
        ok = !jni::clearException(env);
    }
    jni::deleteLocal(env, params);
    jni::deleteLocal(env, paramsClass);
    jni::deleteLocal(env, viewClass);
    return ok;
}

bool applyChildGeometry(
    JNIEnv* env,
    jobject view,
    int width,
    int height,
    int left,
    int top) noexcept {
    if (!env || !view || width <= 0 || height <= 0) {
        return false;
    }
    jclass viewClass = env->FindClass("android/view/View");
    jclass paramsClass = env->FindClass("android/widget/FrameLayout$LayoutParams");
    const jmethodID constructor = paramsClass
        ? env->GetMethodID(paramsClass, "<init>", "(II)V")
        : nullptr;
    jobject params = constructor
        ? env->NewObject(paramsClass, constructor, width, height)
        : nullptr;
    const jfieldID leftMargin = paramsClass
        ? env->GetFieldID(paramsClass, "leftMargin", "I")
        : nullptr;
    const jfieldID topMargin = paramsClass
        ? env->GetFieldID(paramsClass, "topMargin", "I")
        : nullptr;
    const jmethodID setLayoutParams = viewClass
        ? env->GetMethodID(
              viewClass,
              "setLayoutParams",
              "(Landroid/view/ViewGroup$LayoutParams;)V")
        : nullptr;
    bool ok = viewClass && paramsClass && constructor && params && leftMargin &&
              topMargin && setLayoutParams && !jni::clearException(env);
    if (ok) {
        env->SetIntField(params, leftMargin, left);
        env->SetIntField(params, topMargin, top);
        env->CallVoidMethod(view, setLayoutParams, params);
        ok = !jni::clearException(env);
    }
    jni::deleteLocal(env, params);
    jni::deleteLocal(env, paramsClass);
    jni::deleteLocal(env, viewClass);
    return ok;
}

bool loadUrl(JNIEnv* env, jobject webView, const char* urlValue) noexcept {
    if (!env || !webView || !urlValue) {
        return false;
    }
    jclass webViewClass = env->GetObjectClass(webView);
    const jmethodID method = webViewClass
        ? env->GetMethodID(webViewClass, "loadUrl", "(Ljava/lang/String;)V")
        : nullptr;
    jstring url = jni::makeString(env, urlValue);
    bool ok = webViewClass && method && url && !jni::clearException(env);
    if (ok) {
        env->CallVoidMethod(webView, method, url);
        ok = !jni::clearException(env);
    }
    jni::deleteLocal(env, url);
    jni::deleteLocal(env, webViewClass);
    return ok;
}

bool loadHtml(
    JNIEnv* env,
    jobject webView,
    const char* baseUrlValue,
    const char* htmlValue) noexcept {
    if (!env || !webView || !baseUrlValue || !htmlValue) {
        return false;
    }
    jclass webViewClass = env->GetObjectClass(webView);
    const jmethodID load = webViewClass
        ? env->GetMethodID(
              webViewClass,
              "loadDataWithBaseURL",
              "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V")
        : nullptr;
    jstring baseUrl = jni::makeString(env, baseUrlValue);
    jstring html = jni::makeString(env, htmlValue);
    jstring mime = jni::makeString(env, "text/html");
    jstring encoding = jni::makeString(env, "UTF-8");
    bool ok = webViewClass && load && baseUrl && html && mime && encoding &&
              !jni::clearException(env);
    if (ok) {
        env->CallVoidMethod(webView, load, baseUrl, html, mime, encoding, baseUrl);
        ok = !jni::clearException(env);
    }
    jni::deleteLocal(env, encoding);
    jni::deleteLocal(env, mime);
    jni::deleteLocal(env, html);
    jni::deleteLocal(env, baseUrl);
    jni::deleteLocal(env, webViewClass);
    return ok;
}

bool getCurrentUrl(JNIEnv* env, jobject webView, char* out, int capacity) noexcept {
    if (!env || !webView || !out || capacity < 2) {
        return false;
    }
    out[0] = '\0';
    jclass webViewClass = env->GetObjectClass(webView);
    const jmethodID getUrl = webViewClass
        ? env->GetMethodID(webViewClass, "getUrl", "()Ljava/lang/String;")
        : nullptr;
    jstring value = getUrl
        ? static_cast<jstring>(env->CallObjectMethod(webView, getUrl))
        : nullptr;
    bool ok = webViewClass && getUrl && value && !jni::clearException(env) &&
              copyJavaString(env, value, out, capacity);
    jni::deleteLocal(env, value);
    jni::deleteLocal(env, webViewClass);
    return ok;
}

bool getCurrentTitle(JNIEnv* env, jobject webView, char* out, int capacity) noexcept {
    if (!env || !webView || !out || capacity < 2) {
        return false;
    }
    out[0] = '\0';
    jclass webViewClass = env->GetObjectClass(webView);
    const jmethodID getTitle = webViewClass
        ? env->GetMethodID(webViewClass, "getTitle", "()Ljava/lang/String;")
        : nullptr;
    jstring value = getTitle
        ? static_cast<jstring>(env->CallObjectMethod(webView, getTitle))
        : nullptr;
    bool ok = webViewClass && getTitle && value && !jni::clearException(env) &&
              copyJavaString(env, value, out, capacity);
    jni::deleteLocal(env, value);
    jni::deleteLocal(env, webViewClass);
    return ok;
}

bool setVisibility(JNIEnv* env, jobject view, bool visible) noexcept {
    if (!env || !view) {
        return false;
    }
    jclass viewClass = env->FindClass("android/view/View");
    const jmethodID method = viewClass
        ? env->GetMethodID(viewClass, "setVisibility", "(I)V")
        : nullptr;
    bool ok = viewClass && method && !jni::clearException(env);
    if (ok) {
        env->CallVoidMethod(view, method, visible ? 0 : 8);
        ok = !jni::clearException(env);
    }
    jni::deleteLocal(env, viewClass);
    return ok;
}

bool setElevation(JNIEnv* env, jobject view, float elevation) noexcept {
    if (!env || !view) {
        return false;
    }
    jclass viewClass = env->FindClass("android/view/View");
    const jmethodID method = viewClass
        ? env->GetMethodID(viewClass, "setElevation", "(F)V")
        : nullptr;
    bool ok = viewClass && method && !jni::clearException(env);
    if (ok) {
        env->CallVoidMethod(view, method, static_cast<jfloat>(elevation));
        ok = !jni::clearException(env);
    }
    jni::deleteLocal(env, viewClass);
    return ok;
}

bool destroy(JNIEnv* env, jobject webView) noexcept {
    if (!env || !webView) {
        return true;
    }
    jclass webViewClass = env->FindClass("android/webkit/WebView");
    const jmethodID stop = webViewClass
        ? env->GetMethodID(webViewClass, "stopLoading", "()V")
        : nullptr;
    const jmethodID destroyMethod = webViewClass
        ? env->GetMethodID(webViewClass, "destroy", "()V")
        : nullptr;
    bool ok = webViewClass && destroyMethod && !jni::clearException(env);
    if (stop) {
        env->CallVoidMethod(webView, stop);
        (void)jni::clearException(env);
    } else {
        (void)jni::clearException(env);
    }
    if (ok) {
        env->CallVoidMethod(webView, destroyMethod);
        ok = !jni::clearException(env);
    }
    jni::deleteLocal(env, webViewClass);
    return ok;
}

} // namespace google_ui::webview_common
