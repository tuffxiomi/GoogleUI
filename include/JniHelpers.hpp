#pragma once

#include <jni.h>

namespace google_ui::jni {

JNIEnv* getEnv(JavaVM* vm, bool& attached) noexcept;
void detachIfNeeded(JavaVM* vm, bool attached) noexcept;
bool clearException(JNIEnv* env) noexcept;
bool isMainThread(JNIEnv* env) noexcept;
void deleteLocal(JNIEnv* env, jobject object) noexcept;
jstring makeString(JNIEnv* env, const char* value) noexcept;

} // namespace google_ui::jni
