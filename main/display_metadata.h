#pragma once
#include <cstdint>
namespace home {
// Only battery context, never calendar contents, framebuffer data or credentials.
struct DisplayMetadata {
    int32_t millivolts = -1;
    char battery_label[24] = {};
};
// A failed completed frame must be resubmittable even when its pixels match.
inline bool ReuseFrame(bool identical, uint32_t requested, uint32_t finished, uint32_t successful) {
    return identical && requested && (finished < requested || successful == requested);
}
}
