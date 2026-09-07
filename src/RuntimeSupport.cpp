#include "CoreTypes.hpp"

extern "C" __attribute__((visibility("hidden")))
void* memcpy(void* destination, const void* source, google_ui::usize count) noexcept {
    auto* out = static_cast<google_ui::u8*>(destination);
    const auto* in = static_cast<const google_ui::u8*>(source);
    for (google_ui::usize index = 0; index < count; ++index) {
        out[index] = in[index];
    }
    return destination;
}
