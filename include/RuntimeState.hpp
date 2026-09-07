#pragma once

#include "LeviLoaderAbi.hpp"
#include <jni.h>

namespace google_ui {

inline constexpr const char* kModuleId = "google_ui";
inline constexpr const char* kDisplayName = "GoogleUI";
inline constexpr const char* kFloatingButtonId = "google_ui.google";
inline constexpr const char* kHardcodedUrl = "https://www.google.com";
inline constexpr const char* kDefaultWebsiteUrl = "https://www.google.com";
inline constexpr const char* kTitleBaseUrl = "https://google-ui.local/title";
inline constexpr const char* kButtonBaseUrl = "https://google-ui.local/button";
inline constexpr const char* kWebsiteTitleBaseUrl = "https://google-ui.local/website-title";
inline constexpr int kWebsiteUrlCapacity = 1024;
inline constexpr int kWebsiteActionNameCapacity = 160;
inline constexpr int kModRootPathCapacity = 1024;
inline constexpr int kModuleInfoJsonCapacity = 8192;
inline constexpr int kBulkModuleInfoJsonCapacity = 262144;

using GetExternalModsInfoFn = jstring (*)(JNIEnv*, jclass);
using ToggleExternalModFn = void (*)(JNIEnv*, jclass, jstring, jboolean);
using SetExternalModConfigFn = void (*)(JNIEnv*, jclass, jstring, jstring, jstring);
using GetExternalButtonCountFn = int (*)();
using GetExternalButtonInfoFn = jstring (*)(JNIEnv*, jclass, jint);
using GetExternalButtonIconBytesFn = jbyteArray (*)(JNIEnv*, jclass, jstring, jint, jint, jboolean);
using DispatchExternalButtonEventFn = void (*)(JNIEnv*, jclass, jstring, jint, jfloat);
using GetDrawCommandsRevisionFn = jlong (*)(JNIEnv*, jclass);
using RuntimeGetDrawCommandsRevisionFn = jlong (*)();
using GetDrawCommandsFn = jobjectArray (*)(JNIEnv*, jclass);

struct RuntimeState {
    JavaVM* vm;
    bool moduleEnabled;
    bool hooksInstalled;
    bool uiVisible;
    bool websiteUiVisible;
    bool layoutInitialized;
    bool settingsLoaded;
    bool buttonPaddingFixed;

    int rootWidth;
    int rootHeight;
    float density;

    int x;
    int y;
    int width;
    int height;
    int titleHeight;
    int margin;

    int buttonX;
    int buttonY;
    int buttonSize;
    int buttonOpacityPercent;
    int buttonCommandSequence;
    int adBlockPollCounter;
    int websiteAdBlockPollCounter;
    jlong syntheticDrawRevision;

    char modRootPath[kModRootPathCapacity];
    char websiteUrl[kWebsiteUrlCapacity];

    jobject activity;
    jobject root;
    jobject container;
    jobject titleBarWebView;
    jobject webView;

    jobject buttonActivity;
    jobject buttonRoot;
    jobject buttonWebView;

    jobject websiteActivity;
    jobject websiteRoot;
    jobject websiteContainer;
    jobject websiteTitleBarWebView;
    jobject websiteWebView;

    jobject adBlockClassLoader;
    jclass adBlockClientClass;
    jobject adBlockClient;

    jobject websiteAdBlockClassLoader;
    jclass websiteAdBlockClientClass;
    jobject websiteAdBlockClient;

    GHandle preloaderHandle;
    GHook infosHook;
    GHook toggleHook;
    GHook configHook;
    GHook revisionHook;
    GHook drawHook;
    GHook buttonCountHook;
    GHook buttonInfoHook;
    GHook buttonIconHook;
    GHook buttonEventHook;

    GetExternalModsInfoFn originalInfos;
    ToggleExternalModFn originalToggle;
    SetExternalModConfigFn originalConfig;
    RuntimeGetDrawCommandsRevisionFn originalRevision;
    GetDrawCommandsFn originalDraw;
    GetExternalButtonCountFn originalButtonCount;
    GetExternalButtonInfoFn originalButtonInfo;
    GetExternalButtonIconBytesFn originalButtonIcon;
    DispatchExternalButtonEventFn originalButtonEvent;
};

extern RuntimeState gState;
extern char gModuleInfoJson[kModuleInfoJsonCapacity];
extern char gBulkModuleInfoJson[kBulkModuleInfoJsonCapacity];

} // namespace google_ui
