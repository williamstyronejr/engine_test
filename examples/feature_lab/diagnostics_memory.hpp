#pragma once
#include "engine/renderer.hpp"
#include "hud_text.hpp"

namespace feature_lab {
// The sink consumes each view immediately, before the next row reuses text.
// Used CPU bytes are a subset of reserved bytes; GPU values are logical payloads.
template <class Row>
void memory_rows(const engine::RenderMemory& memory, std::string& text, Row&& row) {
    const auto kib = [](std::size_t bytes) { return Fixed2{static_cast<double>(bytes) / 1024}; };
    row("RENDERER MEMORY / KIB");
    row("KIB: 1024 BYTES / PRE F2 DRAW");
    row("CPU BUFFER / RESERVED / USED");
    row(format_text(text, "VERTICES / ", kib(memory.cpu_vertex_reserved_bytes), " / ",
                    kib(memory.cpu_vertex_used_bytes)));
    row(format_text(text, "FONT ATLAS / ", kib(memory.cpu_font_atlas_reserved_bytes), " / ",
                    kib(memory.cpu_font_atlas_used_bytes)));
    row(format_text(text, "FONT GLYPHS / ", kib(memory.cpu_font_glyph_reserved_bytes), " / ",
                    kib(memory.cpu_font_glyph_used_bytes)));
    row("USED IS PART OF RESERVED");
    row("GPU / LOGICAL PAYLOADS");
    row(format_text(text, "VERTEX STORE / ", kib(memory.gpu_vertex_store_bytes)));
    row(format_text(text, "BUILTIN TEXELS / ", kib(memory.gpu_builtin_texture_bytes)));
    row(format_text(text, "UPLOADED TEXELS / ", kib(memory.gpu_uploaded_texture_bytes)));
    row(format_text(text, "FONT TEXELS / ", kib(memory.gpu_font_texture_bytes)));
    row(format_text(text, "TARGET TEXELS / ", kib(memory.gpu_target_texture_bytes)));
    row("EXCLUDES DRIVER AND RESIDENCY");
    row("RSS AND STAGING PEAKS ARE SEPARATE");
    row("EXCLUDES OTHER HEAP AND GL OBJECTS");
}
} // namespace feature_lab
