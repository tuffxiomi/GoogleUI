#pragma once

#include <jni.h>

namespace google_ui::adblock {

enum class Target {
    Google,
    Website,
};

bool installClient(JNIEnv* env, jobject activity, jobject webView, Target target) noexcept;
void releaseClient(JNIEnv* env, Target target) noexcept;
bool initialize(JNIEnv* env, Target target) noexcept;
void poll(JNIEnv* env) noexcept;

} // namespace google_ui::adblock
