#include <stddef.h>

extern "C" void* memset(void* destination, int value, size_t count) {
    auto* out = static_cast<unsigned char*>(destination);
    for (size_t i = 0; i < count; ++i) out[i] = static_cast<unsigned char>(value);
    return destination;
}

extern "C" void* memcpy(void* destination, const void* source, size_t count) {
    auto* out = static_cast<unsigned char*>(destination);
    const auto* in = static_cast<const unsigned char*>(source);
    for (size_t i = 0; i < count; ++i) out[i] = in[i];
    return destination;
}
