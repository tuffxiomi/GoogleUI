#include "ResponsiveLayout.hpp"

#include "JniHelpers.hpp"
#include "RuntimeState.hpp"
#include "TextUtil.hpp"

namespace google_ui::layout {
namespace {

int getViewDimension(JNIEnv* env, jobject view, const char* methodName) noexcept {
    if (!env || !view || !methodName) {
        return 0;
    }
    jclass viewClass = env->GetObjectClass(view);
    const jmethodID method = viewClass
        ? env->GetMethodID(viewClass, methodName, "()I")
        : nullptr;
    const jint value = method ? env->CallIntMethod(view, method) : 0;
    const bool ok = viewClass && method && !jni::clearException(env);
    jni::deleteLocal(env, viewClass);
    return ok ? static_cast<int>(value) : 0;
}

float getDensity(JNIEnv* env, jobject activity) noexcept {
    if (!env || !activity) {
        return 1.0F;
    }
    jclass contextClass = env->FindClass("android/content/Context");
    const jmethodID getResources = contextClass
        ? env->GetMethodID(
              contextClass,
              "getResources",
              "()Landroid/content/res/Resources;")
        : nullptr;
    jobject resources = getResources
        ? env->CallObjectMethod(activity, getResources)
        : nullptr;
    jclass resourcesClass = resources ? env->GetObjectClass(resources) : nullptr;
    const jmethodID getDisplayMetrics = resourcesClass
        ? env->GetMethodID(
              resourcesClass,
              "getDisplayMetrics",
              "()Landroid/util/DisplayMetrics;")
        : nullptr;
    jobject metrics = getDisplayMetrics
        ? env->CallObjectMethod(resources, getDisplayMetrics)
        : nullptr;
    jclass metricsClass = metrics ? env->GetObjectClass(metrics) : nullptr;
    const jfieldID densityField = metricsClass
        ? env->GetFieldID(metricsClass, "density", "F")
        : nullptr;
    const jfloat density = densityField
        ? env->GetFloatField(metrics, densityField)
        : 1.0F;
    const bool ok = contextClass && getResources && resources && resourcesClass &&
                    getDisplayMetrics && metrics && metricsClass && densityField &&
                    !jni::clearException(env);
    jni::deleteLocal(env, metricsClass);
    jni::deleteLocal(env, metrics);
    jni::deleteLocal(env, resourcesClass);
    jni::deleteLocal(env, resources);
    jni::deleteLocal(env, contextClass);
    if (!ok || density < 0.5F || density > 8.0F) {
        return 1.0F;
    }
    return density;
}

int minimum(int a, int b) noexcept {
    return a < b ? a : b;
}

int maximum(int a, int b) noexcept {
    return a > b ? a : b;
}

} // namespace

void clampWindow() noexcept {
    if (gState.rootWidth <= 0 || gState.rootHeight <= 0) {
        return;
    }
    const int totalHeight = gState.titleHeight + gState.height;
    const int maximumX = maximum(0, gState.rootWidth - gState.width);
    const int maximumY = maximum(0, gState.rootHeight - totalHeight);
    gState.x = text::clampInt(gState.x, 0, maximumX);
    gState.y = text::clampInt(gState.y, 0, maximumY);
}

void clampButton() noexcept {
    if (gState.rootWidth <= 0 || gState.rootHeight <= 0 || gState.buttonSize <= 0) {
        return;
    }
    const int maximumX = maximum(0, gState.rootWidth - gState.buttonSize);
    const int maximumY = maximum(0, gState.rootHeight - gState.buttonSize);
    gState.buttonX = text::clampInt(gState.buttonX, 0, maximumX);
    gState.buttonY = text::clampInt(gState.buttonY, 0, maximumY);
}

bool refresh(JNIEnv* env, jobject activity, jobject root, bool force) noexcept {
    if (!env || !activity || !root) {
        return false;
    }
    const int newWidth = getViewDimension(env, root, "getWidth");
    const int newHeight = getViewDimension(env, root, "getHeight");
    if (newWidth <= 0 || newHeight <= 0) {
        return false;
    }
    if (!force && gState.layoutInitialized && newWidth == gState.rootWidth &&
        newHeight == gState.rootHeight) {
        return true;
    }

    const int previousWidth = gState.rootWidth;
    const int previousHeight = gState.rootHeight;
    const bool hadLayout = gState.layoutInitialized;
    gState.rootWidth = newWidth;
    gState.rootHeight = newHeight;
    gState.density = getDensity(env, activity);

    const int shortSide = minimum(newWidth, newHeight);
    float layoutScale = static_cast<float>(newWidth) / 1280.0F;
    const float heightScale = static_cast<float>(newHeight) / 720.0F;
    if (heightScale < layoutScale) {
        layoutScale = heightScale;
    }
    if (layoutScale < 0.65F) {
        layoutScale = 0.65F;
    } else if (layoutScale > 1.60F) {
        layoutScale = 1.60F;
    }

    gState.margin = text::clampInt(shortSide / 45, 8, 36);
    gState.titleHeight = text::clampInt(
        static_cast<int>(40.0F * layoutScale),
        28,
        64);
    gState.buttonSize = text::clampInt(shortSide / 9, 52, 96);

    const int maximumWidth = maximum(1, newWidth - gState.margin * 2);
    const int maximumTotalHeight = maximum(1, newHeight - gState.margin * 2);
    const float aspect = static_cast<float>(newWidth) / static_cast<float>(newHeight);
    const float widthRatio = aspect >= 1.60F ? 0.72F : (aspect >= 1.0F ? 0.82F : 0.92F);
    const float heightRatio = aspect >= 1.60F ? 0.78F : (aspect >= 1.0F ? 0.76F : 0.72F);
    gState.width = text::clampInt(
        static_cast<int>(static_cast<float>(newWidth) * widthRatio),
        minimum(300, maximumWidth),
        maximumWidth);
    int totalHeight = text::clampInt(
        static_cast<int>(static_cast<float>(newHeight) * heightRatio),
        minimum(230, maximumTotalHeight),
        maximumTotalHeight);
    if (totalHeight <= gState.titleHeight + 80) {
        totalHeight = minimum(maximumTotalHeight, gState.titleHeight + 80);
    }
    gState.height = maximum(80, totalHeight - gState.titleHeight);

    // The browser UI is fixed and centered. The floating button is positioned
    // exclusively by Mod Menu X/Y sliders; no drag or automatic coordinate
    // scaling is applied. Values are only clamped to the current display.
    gState.x = (newWidth - gState.width) / 2;
    gState.y = (newHeight - (gState.titleHeight + gState.height)) / 2;

    (void)hadLayout;
    (void)previousWidth;
    (void)previousHeight;
    clampWindow();
    clampButton();
    gState.layoutInitialized = true;
    return true;
}

} // namespace google_ui::layout
