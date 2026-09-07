#pragma once

#include "CoreTypes.hpp"

namespace google_ui::runtime_log {

void info(const char* message) noexcept;
void warn(const char* message) noexcept;
void error(const char* message) noexcept;
void hookStatus(
    const char* symbol,
    bool resolved,
    bool wildcardMatched,
    bool installed,
    usize actualSize,
    usize expectedSize) noexcept;

} // namespace google_ui::runtime_log
