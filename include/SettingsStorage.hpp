#pragma once

#include "CoreTypes.hpp"
#include <jni.h>

namespace google_ui::settings {

bool load(JNIEnv* env) noexcept;
bool save(JNIEnv* env) noexcept;
void resetLoadedState() noexcept;
bool setWebsiteUrl(const char* value) noexcept;
bool deriveWebsiteActionName(
    const char* url,
    char* output,
    usize capacity) noexcept;

} // namespace google_ui::settings
