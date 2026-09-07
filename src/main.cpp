#include "AndroidWebViewController.hpp"
#include "FloatingButtonController.hpp"
#include "JniHelpers.hpp"
#include "LeviLoaderAbi.hpp"
#include "ModContextResolver.hpp"
#include "ModMenuBridgeHooks.hpp"
#include "RuntimeState.hpp"
#include "RuntimeLog.hpp"
#include "SettingsStorage.hpp"
#include "WebsiteWebViewController.hpp"

namespace google_ui {
namespace {

bool load(void*, void* context) noexcept {
    if (!context) {
        return false;
    }
    gState.vm = *reinterpret_cast<JavaVM**>(context);
    if (!mod_context::captureModRootPath(context)) {
        return false;
    }
    gState.moduleEnabled = true;
    gState.uiVisible = false;
    gState.websiteUiVisible = false;
    gState.layoutInitialized = false;
    settings::resetLoadedState();
    return gState.vm != nullptr;
}

bool enable(void*, void*) noexcept {
    gState.moduleEnabled = true;
    const bool installed = hooks::install();
    if (!installed) {
        runtime_log::error("enable: bridge installation failed; see preceding GoogleUI hook diagnostics");
    }
    return installed;
}

bool disable(void*, void*) noexcept {
    gState.moduleEnabled = false;
    gState.uiVisible = false;
    gState.websiteUiVisible = false;
    bool attached = false;
    JNIEnv* env = jni::getEnv(gState.vm, attached);
    bool cleaned = true;
    if (env && jni::isMainThread(env)) {
        cleaned = website_webview::destroy(env) && cleaned;
        cleaned = webview::destroy(env) && cleaned;
        cleaned = floating_button::destroy(env) && cleaned;
    }
    jni::detachIfNeeded(gState.vm, attached);
    hooks::uninstall();
    return cleaned;
}

bool unload(void* instance, void* context) noexcept {
    const bool result = disable(instance, context);
    gState.vm = nullptr;
    return result;
}

levi::ModRegistration gRegistration = {
    &gState,
    &load,
    &enable,
    &disable,
    &unload,
};

} // namespace
} // namespace google_ui

extern "C" __attribute__((visibility("default")))
google_ui::levi::ModRegistration* PLGetModRegistration() {
    return &google_ui::gRegistration;
}
