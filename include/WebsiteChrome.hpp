#pragma once

#include <jni.h>

namespace google_ui::website_chrome {

bool initialize(JNIEnv* env) noexcept;
void poll(JNIEnv* env) noexcept;

} // namespace google_ui::website_chrome
