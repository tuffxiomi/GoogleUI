#include "DrawCommandAppender.hpp"

#include "JniHelpers.hpp"
#include "RuntimeState.hpp"

namespace google_ui::draw {
namespace {

constexpr jsize kRectValuesPerCommand = 6;
constexpr jsize kChunk = 256;

bool copyInts(JNIEnv* env, jintArray source, jintArray destination, jsize count) noexcept {
    jint buffer[kChunk];
    for (jsize offset = 0; offset < count; offset += kChunk) {
        const jsize amount = (count - offset) < kChunk ? (count - offset) : kChunk;
        env->GetIntArrayRegion(source, offset, amount, buffer);
        if (jni::clearException(env)) {
            return false;
        }
        env->SetIntArrayRegion(destination, offset, amount, buffer);
        if (jni::clearException(env)) {
            return false;
        }
    }
    return true;
}

bool copyFloats(JNIEnv* env, jfloatArray source, jfloatArray destination, jsize count) noexcept {
    jfloat buffer[kChunk];
    for (jsize offset = 0; offset < count; offset += kChunk) {
        const jsize amount = (count - offset) < kChunk ? (count - offset) : kChunk;
        env->GetFloatArrayRegion(source, offset, amount, buffer);
        if (jni::clearException(env)) {
            return false;
        }
        env->SetFloatArrayRegion(destination, offset, amount, buffer);
        if (jni::clearException(env)) {
            return false;
        }
    }
    return true;
}

bool copyObjects(
    JNIEnv* env,
    jobjectArray source,
    jobjectArray destination,
    jsize count) noexcept {
    for (jsize index = 0; index < count; ++index) {
        jobject value = env->GetObjectArrayElement(source, index);
        if (jni::clearException(env)) {
            jni::deleteLocal(env, value);
            return false;
        }
        env->SetObjectArrayElement(destination, index, value);
        jni::deleteLocal(env, value);
        if (jni::clearException(env)) {
            return false;
        }
    }
    return true;
}

void deleteArrays(
    JNIEnv* env,
    jobject a0,
    jobject a1,
    jobject a2,
    jobject a3,
    jobject a4,
    jobject a5,
    jobject a6,
    jobject a7) noexcept {
    jni::deleteLocal(env, a7);
    jni::deleteLocal(env, a6);
    jni::deleteLocal(env, a5);
    jni::deleteLocal(env, a4);
    jni::deleteLocal(env, a3);
    jni::deleteLocal(env, a2);
    jni::deleteLocal(env, a1);
    jni::deleteLocal(env, a0);
}

} // namespace

jobjectArray appendFrame(JNIEnv* env, jobjectArray original) noexcept {
    if (!env || !gState.uiVisible) {
        return original;
    }

    jintArray oldTypes = nullptr;
    jfloatArray oldRects = nullptr;
    jintArray oldColors = nullptr;
    jfloatArray oldSizes = nullptr;
    jobjectArray oldTexts = nullptr;
    jobjectArray oldModules = nullptr;
    jobjectArray oldFonts = nullptr;
    jobjectArray oldImages = nullptr;
    jsize originalCount = 0;

    if (original) {
        const jsize outerLength = env->GetArrayLength(original);
        if (jni::clearException(env) || outerLength < 8) {
            return original;
        }
        oldTypes = static_cast<jintArray>(env->GetObjectArrayElement(original, 0));
        oldRects = static_cast<jfloatArray>(env->GetObjectArrayElement(original, 1));
        oldColors = static_cast<jintArray>(env->GetObjectArrayElement(original, 2));
        oldSizes = static_cast<jfloatArray>(env->GetObjectArrayElement(original, 3));
        oldTexts = static_cast<jobjectArray>(env->GetObjectArrayElement(original, 4));
        oldModules = static_cast<jobjectArray>(env->GetObjectArrayElement(original, 5));
        oldFonts = static_cast<jobjectArray>(env->GetObjectArrayElement(original, 6));
        oldImages = static_cast<jobjectArray>(env->GetObjectArrayElement(original, 7));
        if (jni::clearException(env) || !oldTypes || !oldRects || !oldColors ||
            !oldSizes || !oldTexts || !oldModules || !oldFonts || !oldImages) {
            deleteArrays(env, oldTypes, oldRects, oldColors, oldSizes, oldTexts,
                         oldModules, oldFonts, oldImages);
            return original;
        }
        originalCount = env->GetArrayLength(oldTypes);
        if (jni::clearException(env) ||
            env->GetArrayLength(oldRects) < originalCount * kRectValuesPerCommand ||
            env->GetArrayLength(oldColors) < originalCount ||
            env->GetArrayLength(oldSizes) < originalCount ||
            env->GetArrayLength(oldTexts) < originalCount ||
            env->GetArrayLength(oldModules) < originalCount ||
            env->GetArrayLength(oldFonts) < originalCount ||
            env->GetArrayLength(oldImages) < originalCount ||
            jni::clearException(env)) {
            deleteArrays(env, oldTypes, oldRects, oldColors, oldSizes, oldTexts,
                         oldModules, oldFonts, oldImages);
            return original;
        }
    }

    constexpr jsize extraCount = 2;
    const jsize newCount = originalCount + extraCount;
    jclass stringClass = env->FindClass("java/lang/String");
    jclass objectClass = env->FindClass("java/lang/Object");
    jintArray types = env->NewIntArray(newCount);
    jfloatArray rects = env->NewFloatArray(newCount * kRectValuesPerCommand);
    jintArray colors = env->NewIntArray(newCount);
    jfloatArray sizes = env->NewFloatArray(newCount);
    jobjectArray texts = stringClass ? env->NewObjectArray(newCount, stringClass, nullptr) : nullptr;
    jobjectArray modules = stringClass ? env->NewObjectArray(newCount, stringClass, nullptr) : nullptr;
    jobjectArray fonts = stringClass ? env->NewObjectArray(newCount, stringClass, nullptr) : nullptr;
    jobjectArray images = stringClass ? env->NewObjectArray(newCount, stringClass, nullptr) : nullptr;
    jobjectArray result = objectClass ? env->NewObjectArray(8, objectClass, nullptr) : nullptr;

    bool ok = stringClass && objectClass && types && rects && colors && sizes &&
              texts && modules && fonts && images && result && !jni::clearException(env);
    if (ok && originalCount > 0) {
        ok = copyInts(env, oldTypes, types, originalCount) &&
             copyFloats(env, oldRects, rects, originalCount * kRectValuesPerCommand) &&
             copyInts(env, oldColors, colors, originalCount) &&
             copyFloats(env, oldSizes, sizes, originalCount) &&
             copyObjects(env, oldTexts, texts, originalCount) &&
             copyObjects(env, oldModules, modules, originalCount) &&
             copyObjects(env, oldFonts, fonts, originalCount) &&
             copyObjects(env, oldImages, images, originalCount);
    }

    if (ok) {
        const jint extraTypes[extraCount] = {3, 1};
        const jfloat extraRects[extraCount * kRectValuesPerCommand] = {
            static_cast<jfloat>(gState.x),
            static_cast<jfloat>(gState.y),
            static_cast<jfloat>(gState.width),
            static_cast<jfloat>(gState.titleHeight),
            0.0F, 0.0F,
            static_cast<jfloat>(gState.x),
            static_cast<jfloat>(gState.y),
            static_cast<jfloat>(gState.width),
            static_cast<jfloat>(gState.height + gState.titleHeight),
            0.0F, 0.0F,
        };
        const jint extraColors[extraCount] = {
            static_cast<jint>(0xE6252525U),
            static_cast<jint>(0xFFE0B351U),
        };
        const jfloat extraSizes[extraCount] = {0.0F, 0.0F};
        env->SetIntArrayRegion(types, originalCount, extraCount, extraTypes);
        env->SetFloatArrayRegion(rects, originalCount * kRectValuesPerCommand,
                                 extraCount * kRectValuesPerCommand, extraRects);
        env->SetIntArrayRegion(colors, originalCount, extraCount, extraColors);
        env->SetFloatArrayRegion(sizes, originalCount, extraCount, extraSizes);
        ok = !jni::clearException(env);
        jstring moduleId = jni::makeString(env, kModuleId);
        if (!moduleId || jni::clearException(env)) {
            ok = false;
        } else {
            env->SetObjectArrayElement(modules, originalCount, moduleId);
            env->SetObjectArrayElement(modules, originalCount + 1, moduleId);
            ok = !jni::clearException(env);
        }
        jni::deleteLocal(env, moduleId);
    }

    if (ok) {
        env->SetObjectArrayElement(result, 0, types);
        env->SetObjectArrayElement(result, 1, rects);
        env->SetObjectArrayElement(result, 2, colors);
        env->SetObjectArrayElement(result, 3, sizes);
        env->SetObjectArrayElement(result, 4, texts);
        env->SetObjectArrayElement(result, 5, modules);
        env->SetObjectArrayElement(result, 6, fonts);
        env->SetObjectArrayElement(result, 7, images);
        ok = !jni::clearException(env);
    }

    deleteArrays(env, types, rects, colors, sizes, texts, modules, fonts, images);
    jni::deleteLocal(env, objectClass);
    jni::deleteLocal(env, stringClass);
    deleteArrays(env, oldTypes, oldRects, oldColors, oldSizes, oldTexts,
                 oldModules, oldFonts, oldImages);
    if (!ok) {
        jni::deleteLocal(env, result);
        return original;
    }
    return result;
}

} // namespace google_ui::draw
