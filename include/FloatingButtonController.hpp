#pragma once

#include <jni.h>

namespace google_ui::floating_button {

bool ensure(JNIEnv* env) noexcept;
void poll(JNIEnv* env) noexcept;
bool updateGeometry(JNIEnv* env) noexcept;
bool destroy(JNIEnv* env) noexcept;

} // namespace google_ui::floating_button
