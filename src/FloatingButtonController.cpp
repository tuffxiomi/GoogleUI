#include "FloatingButtonController.hpp"

#include "JniHelpers.hpp"
#include "RuntimeState.hpp"

namespace google_ui::floating_button {
namespace {

bool clear(JNIEnv* env) noexcept {
    return env && jni::clearException(env);
}

} // namespace

bool ensure(JNIEnv* env) noexcept {
    if (!env || !gState.moduleEnabled) {
        return false;
    }
    if (gState.buttonPaddingFixed) {
        return true;
    }

    jclass managerClass = env->FindClass(
        "org/levimc/launcher/core/mods/inbuilt/overlay/InbuiltOverlayManager");
    const jmethodID getInstance = managerClass
        ? env->GetStaticMethodID(
              managerClass,
              "getInstance",
              "()Lorg/levimc/launcher/core/mods/inbuilt/overlay/InbuiltOverlayManager;")
        : nullptr;
    jobject manager = getInstance
        ? env->CallStaticObjectMethod(managerClass, getInstance)
        : nullptr;
    const jfieldID mapField = managerClass
        ? env->GetFieldID(
              managerClass,
              "externalButtonOverlayMap",
              "Ljava/util/Map;")
        : nullptr;
    jobject map = manager && mapField
        ? env->GetObjectField(manager, mapField)
        : nullptr;

    jclass mapClass = env->FindClass("java/util/Map");
    const jmethodID get = mapClass
        ? env->GetMethodID(
              mapClass,
              "get",
              "(Ljava/lang/Object;)Ljava/lang/Object;")
        : nullptr;
    jstring buttonId = jni::makeString(env, kFloatingButtonId);
    jobject overlay = map && get && buttonId
        ? env->CallObjectMethod(map, get, buttonId)
        : nullptr;

    jclass overlayClass = overlay ? env->GetObjectClass(overlay) : nullptr;
    jclass baseClass = overlayClass ? env->GetSuperclass(overlayClass) : nullptr;
    const jfieldID viewField = baseClass
        ? env->GetFieldID(baseClass, "overlayView", "Landroid/view/View;")
        : nullptr;
    jobject view = overlay && viewField
        ? env->GetObjectField(overlay, viewField)
        : nullptr;
    jclass viewClass = env->FindClass("android/view/View");
    const jmethodID setPadding = viewClass
        ? env->GetMethodID(viewClass, "setPadding", "(IIII)V")
        : nullptr;

    bool ok = managerClass && getInstance && manager && mapField && map &&
              mapClass && get && buttonId && overlay && overlayClass &&
              baseClass && viewField && view && viewClass && setPadding &&
              !clear(env);
    if (ok) {
        env->CallVoidMethod(
            view,
            setPadding,
            static_cast<jint>(0),
            static_cast<jint>(0),
            static_cast<jint>(0),
            static_cast<jint>(0));
        ok = !clear(env);
    }
    if (ok) {
        gState.buttonPaddingFixed = true;
    }

    jni::deleteLocal(env, viewClass);
    jni::deleteLocal(env, view);
    jni::deleteLocal(env, baseClass);
    jni::deleteLocal(env, overlayClass);
    jni::deleteLocal(env, overlay);
    jni::deleteLocal(env, buttonId);
    jni::deleteLocal(env, mapClass);
    jni::deleteLocal(env, map);
    jni::deleteLocal(env, manager);
    jni::deleteLocal(env, managerClass);
    return ok;
}

void poll(JNIEnv* env) noexcept {
    if (!gState.buttonPaddingFixed) {
        (void)ensure(env);
    }
}

bool updateGeometry(JNIEnv* env) noexcept {
    return ensure(env);
}

bool destroy(JNIEnv* env) noexcept {
    (void)env;
    gState.buttonPaddingFixed = false;
    return true;
}

} // namespace google_ui::floating_button
