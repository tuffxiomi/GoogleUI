#include "GoogleChrome.hpp"

#include "RuntimeState.hpp"
#include "WebViewCommon.hpp"

namespace google_ui::chrome {
namespace {

inline constexpr const char* kTitleHtml = R"HTML(
<!doctype html><html><head><meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no"><style>
*{box-sizing:border-box;-webkit-user-select:none;user-select:none;-webkit-tap-highlight-color:transparent}html,body{margin:0;width:100%;height:100%;overflow:hidden;background:#252525;color:#fff;font-family:sans-serif}body{display:flex;align-items:center;padding:0 clamp(8px,2vw,16px);border:1px solid #686868;border-bottom-color:#494949}.name{font-size:clamp(12px,2.2vw,17px);font-weight:700;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}.url{margin-left:10px;font-size:clamp(9px,1.6vw,12px);color:#d2d2d2;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}.hint{margin-left:auto;padding-left:8px;font-size:clamp(9px,1.5vw,11px);color:#e0b351;letter-spacing:.08em;font-weight:700}
</style></head><body><div class="name">GoogleUI</div><div class="url">Google</div><div class="hint">AUTO RESOLUTION · AD BLOCK</div><script>document.addEventListener('touchmove',e=>e.preventDefault(),{passive:false});document.addEventListener('contextmenu',e=>e.preventDefault());</script></body></html>
)HTML";

} // namespace

bool initialize(JNIEnv* env) noexcept {
    if (!env || !gState.titleBarWebView) {
        return false;
    }
    return webview_common::loadHtml(
        env,
        gState.titleBarWebView,
        kTitleBaseUrl,
        kTitleHtml);
}

void poll(JNIEnv* env) noexcept {
    (void)env;
}

} // namespace google_ui::chrome
