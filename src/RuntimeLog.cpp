#include "RuntimeLog.hpp"

extern "C" int __android_log_print(int priority, const char* tag, const char* format, ...);

namespace google_ui::runtime_log {
namespace {

constexpr int kInfo = 4;
constexpr int kWarn = 5;
constexpr int kError = 6;
constexpr const char* kTag = "GoogleUI";

void write(int priority, const char* message) noexcept {
    __android_log_print(priority, kTag, "%s", message ? message : "(null)");
}

} // namespace

void info(const char* message) noexcept {
    write(kInfo, message);
}

void warn(const char* message) noexcept {
    write(kWarn, message);
}

void error(const char* message) noexcept {
    write(kError, message);
}

void hookStatus(
    const char* symbol,
    bool resolved,
    bool wildcardMatched,
    bool installed,
    usize actualSize,
    usize expectedSize) noexcept {
    __android_log_print(
        installed ? kInfo : kError,
        kTag,
        "bridge hook %s resolved=%d wildcard=%d installed=%d size=%llu expected=%llu",
        symbol ? symbol : "(null)",
        resolved ? 1 : 0,
        wildcardMatched ? 1 : 0,
        installed ? 1 : 0,
        static_cast<unsigned long long>(actualSize),
        static_cast<unsigned long long>(expectedSize));
}

} // namespace google_ui::runtime_log
