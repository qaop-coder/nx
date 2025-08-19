//------------------------------------------------------------------------------
// OpenGL-based layer system
//------------------------------------------------------------------------------

#pragma once

#include "core.h"

// Minimal immediate-mode style interface for layered RGBA buffers rendered via
// OpenGL. Call gfx_init after creating an OpenGL context. Create one or more
// layers (each owns an OpenGL texture). On every frame call gfx_render with an
// array of layer pointers in the order you want them blended. Each layer is
// auto letter/pillar boxed to best fit the window while preserving aspect
// ratio; unused regions are cleared to black.
//
// All pixel data is 32-bit RGBA (8 bits per channel) in memory order compatible
// with GL_RGBA / GL_UNSIGNED_BYTE.

typedef struct GfxLayer GfxLayer;

// Initialize after a valid OpenGL context exists. Returns true on success.
bool gfx_init(void);
// Free all internal GL objects created by gfx_init (does not destroy layers).
void gfx_shutdown(void);

// Create a layer. Optional initial pixel data (can be NULL).
GfxLayer* gfx_layer_create(int width, int height, const uint32_t* rgba_pixels);
// Destroy a layer (safe to pass NULL).
void      gfx_layer_destroy(GfxLayer* layer);

// Enable/disable layer (default enabled). Disabled layers are skipped.
void gfx_layer_set_enabled(GfxLayer* layer, bool enabled);
bool gfx_layer_is_enabled(const GfxLayer* layer);

// Replace all pixels (must supply width*height uint32_t values). Size must
// match layer.
void gfx_layer_update_pixels(GfxLayer* layer, const uint32_t* rgba_pixels);

// (Optional) Resize layer and replace pixels. Pass new pixel data sized
// new_w*new_h. Returns true on success.
bool gfx_layer_resize(GfxLayer*  layer,
                      int        new_w,
                      int        new_h,
                      const u32* rgba_pixels);

// Accessors
int gfx_layer_get_width(const GfxLayer* layer);
int gfx_layer_get_height(const GfxLayer* layer);

// Render ordered list of layers (front-most last) to current framebuffer of
// given window size.
void gfx_render(GfxLayer** layers,
                int        layer_count,
                int        window_width,
                int        window_height);
