#include "ActivityResolver.hpp"
#include "JniHelpers.hpp"

namespace google_ui::activity {

jobject resolve(JNIEnv* env) noexcept {
    if (!env) {
        return nullptr;
    }
    jclass managerClass = env->FindClass(
        "org/levimc/launcher/core/mods/inbuilt/overlay/InbuiltOverlayManager");
    if (!managerClass || jni::clearException(env)) {
        jni::deleteLocal(env, managerClass);
        return nullptr;
    }
    const jmethodID getInstance = env->GetStaticMethodID(
        managerClass,
        "getInstance",
        "()Lorg/levimc/launcher/core/mods/inbuilt/overlay/InbuiltOverlayManager;");
    if (!getInstance || jni::clearException(env)) {
        jni::deleteLocal(env, managerClass);
        return nullptr;
    }
    jobject manager = env->CallStaticObjectMethod(managerClass, getInstance);
    if (!manager || jni::clearException(env)) {
        jni::deleteLocal(env, manager);
        jni::deleteLocal(env, managerClass);
        return nullptr;
    }
    const jfieldID activityField = env->GetFieldID(
        managerClass,
        "activity",
        "Landroid/app/Activity;");
    if (!activityField || jni::clearException(env)) {
        jni::deleteLocal(env, manager);
        jni::deleteLocal(env, managerClass);
        return nullptr;
    }
    jobject activity = env->GetObjectField(manager, activityField);
    if (jni::clearException(env)) {
        jni::deleteLocal(env, activity);
        activity = nullptr;
    }
    jni::deleteLocal(env, manager);
    jni::deleteLocal(env, managerClass);
    return activity;
}

} // namespace google_ui::activity
