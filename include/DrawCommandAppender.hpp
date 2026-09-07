#pragma once

#include <jni.h>

namespace google_ui::draw {

jobjectArray appendFrame(JNIEnv* env, jobjectArray original) noexcept;

} // namespace google_ui::draw
