#pragma once

#include "CoreTypes.hpp"

namespace google_ui::levi {
using LifecycleFunction = bool (*)(void* instance, void* context);

struct ModRegistration {
    void* instance;
    LifecycleFunction load;
    LifecycleFunction enable;
    LifecycleFunction disable;
    LifecycleFunction unload;
};
}

extern "C" {
using GHandle = void*;
using GHook = void*;

__attribute__((visibility("default"))) GHandle GlossOpen(const char* libraryName);
__attribute__((visibility("default"))) int GlossClose(GHandle handle, bool callDlclose);
__attribute__((visibility("default"))) google_ui::uptr GlossSymbol(
    GHandle handle,
    const char* symbol,
    google_ui::usize* symbolSize);
__attribute__((visibility("default"))) google_ui::uptr GlossSymbolEx(
    const void* addressInLibrary,
    const char* symbol,
    google_ui::usize* symbolSize);
__attribute__((visibility("default"))) void GlossInit(bool hookLinker);
__attribute__((visibility("default"))) GHook GlossHook(
    void* symbolAddress,
    void* replacement,
    void** original);
__attribute__((visibility("default"))) void GlossHookDelete(GHook hook);
}
