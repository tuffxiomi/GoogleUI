#include "ModContextResolver.hpp"

#include "CoreTypes.hpp"
#include "RuntimeState.hpp"

namespace google_ui::mod_context {
namespace {

inline constexpr usize kModRootPathOffset = 0xE0;

usize boundedLength(const char* value, usize maximum) noexcept {
    usize length = 0;
    while (value && value[length] && length < maximum) {
        ++length;
    }
    return length;
}

} // namespace

bool captureModRootPath(void* context) noexcept {
    gState.modRootPath[0] = '\0';
    if (!context) {
        return false;
    }

    const u8* object = static_cast<const u8*>(context) + kModRootPathOffset;
    const char* path = nullptr;
    usize length = 0;

    if ((object[0] & 1U) == 0U) {
        length = static_cast<usize>(object[0] >> 1U);
        path = reinterpret_cast<const char*>(object + 1);
    } else {
        length = *reinterpret_cast<const usize*>(object + 8);
        path = *reinterpret_cast<const char* const*>(object + 16);
    }

    if (!path || length == 0 || length >= static_cast<usize>(kModRootPathCapacity) ||
        path[0] != '/' || boundedLength(path, length + 1) < length) {
        return false;
    }

    for (usize index = 0; index < length; ++index) {
        gState.modRootPath[index] = path[index];
    }
    while (length > 1 && gState.modRootPath[length - 1] == '/') {
        --length;
    }
    gState.modRootPath[length] = '\0';
    return true;
}

} // namespace google_ui::mod_context
