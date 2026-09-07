#pragma once

#include <jni.h>

namespace google_ui::layout {

bool refresh(JNIEnv* env, jobject activity, jobject root, bool force) noexcept;
void clampWindow() noexcept;
void clampButton() noexcept;

} // namespace google_ui::layout
