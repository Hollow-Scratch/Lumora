#pragma once
#include <cstdint>

namespace Lumora::Graphics::Font {
// Seven rows of five high-order-to-low-order pixel bits. Unsupported glyphs
// render as a question mark; lowercase ASCII is folded to uppercase.
void GlyphRows(char character, uint8_t rows[7]) noexcept;
}
