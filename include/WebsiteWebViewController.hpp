#pragma once

#include <jni.h>

namespace google_ui::website_webview {

bool show(JNIEnv* env) noexcept;
bool hide(JNIEnv* env) noexcept;
bool toggle(JNIEnv* env) noexcept;
bool destroy(JNIEnv* env) noexcept;
bool updateGeometry(JNIEnv* env) noexcept;
bool reloadConfiguredUrl(JNIEnv* env) noexcept;
void pollResolution(JNIEnv* env) noexcept;
bool isAlive() noexcept;
bool isVisible() noexcept;

} // namespace google_ui::website_webview
