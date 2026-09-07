#include "JniHelpers.hpp"

namespace google_ui::jni {

JNIEnv* getEnv(JavaVM* vm, bool& attached) noexcept {
    attached = false;
    if (!vm) {
        return nullptr;
    }
    JNIEnv* env = nullptr;
    const jint status = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (status == JNI_OK) {
        return env;
    }
    if (status != JNI_EDETACHED) {
        return nullptr;
    }
    if (vm->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr) != JNI_OK) {
        return nullptr;
    }
    attached = true;
    return env;
}

void detachIfNeeded(JavaVM* vm, bool attached) noexcept {
    if (vm && attached) {
        vm->DetachCurrentThread();
    }
}

bool clearException(JNIEnv* env) noexcept {
    if (!env || env->ExceptionCheck() != JNI_TRUE) {
        return false;
    }
    env->ExceptionClear();
    return true;
}

void deleteLocal(JNIEnv* env, jobject object) noexcept {
    if (env && object) {
        env->DeleteLocalRef(object);
    }
}

jstring makeString(JNIEnv* env, const char* value) noexcept {
    if (!env || !value) {
        return nullptr;
    }
    return env->NewStringUTF(value);
}

bool isMainThread(JNIEnv* env) noexcept {
    if (!env) {
        return false;
    }
    jclass looperClass = env->FindClass("android/os/Looper");
    if (!looperClass || clearException(env)) {
        deleteLocal(env, looperClass);
        return false;
    }
    const jmethodID getMainLooper = env->GetStaticMethodID(
        looperClass, "getMainLooper", "()Landroid/os/Looper;");
    const jmethodID myLooper = env->GetStaticMethodID(
        looperClass, "myLooper", "()Landroid/os/Looper;");
    if (!getMainLooper || !myLooper || clearException(env)) {
        deleteLocal(env, looperClass);
        return false;
    }
    jobject mainLooper = env->CallStaticObjectMethod(looperClass, getMainLooper);
    jobject currentLooper = env->CallStaticObjectMethod(looperClass, myLooper);
    const bool ok = !clearException(env) && mainLooper && currentLooper &&
                    env->IsSameObject(mainLooper, currentLooper) == JNI_TRUE;
    deleteLocal(env, currentLooper);
    deleteLocal(env, mainLooper);
    deleteLocal(env, looperClass);
    return ok;
}

} // namespace google_ui::jni
