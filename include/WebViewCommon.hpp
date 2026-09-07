#pragma once

#include <jni.h>

namespace google_ui::webview_common {

bool configureBrowser(JNIEnv* env, jobject webView) noexcept;
bool reload(JNIEnv* env, jobject webView) noexcept;
bool configureLocalUi(JNIEnv* env, jobject webView, jint backgroundColor) noexcept;
bool makeTouchBlocking(JNIEnv* env, jobject view) noexcept;
bool applyGeometry(JNIEnv* env, jobject view, int x, int y, int width, int height, float alpha) noexcept;
bool applyChildGeometry(JNIEnv* env, jobject view, int width, int height, int left, int top) noexcept;
bool loadUrl(JNIEnv* env, jobject webView, const char* url) noexcept;
bool loadHtml(JNIEnv* env, jobject webView, const char* baseUrl, const char* html) noexcept;
bool destroy(JNIEnv* env, jobject webView) noexcept;
bool getCurrentUrl(JNIEnv* env, jobject webView, char* out, int capacity) noexcept;
bool getCurrentTitle(JNIEnv* env, jobject webView, char* out, int capacity) noexcept;
bool setVisibility(JNIEnv* env, jobject view, bool visible) noexcept;
bool setElevation(JNIEnv* env, jobject view, float elevation) noexcept;

} // namespace google_ui::webview_common
