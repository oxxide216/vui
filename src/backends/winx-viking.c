#include "vui/vui-winx-vulkan.h"
#include "../../build/assets-winx-viking.c"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define FONT_RENDER_SCALE 64.0

#define ATLAS_PADDING 5
#define ATLAS_WIDTH   32768
#define ATLAS_HEIGHT  FONT_RENDER_SCALE

#define ONEDGE_VALUE     180
#define PIXEL_DIST_SCALE ((f32) ONEDGE_VALUE / 2.5)

#define ASTR(name) ((Str) { (char *) name, name##_len })
#define SSTR(name) ASTR(build_shaders_winx_viking_##name)

typedef struct {
  f32 screen_width;
  f32 screen_height;
} UBO;

typedef struct {
  f32 x, y;
  f32 r, g, b, a;
} ShapeVertex;

typedef Da(ShapeVertex) ShapeVertices;

typedef struct {
  f32 x, y;
  f32 u, v;
  f32 r, g, b, a;
} CircleVertex;

typedef Da(CircleVertex) CircleVertices;

typedef struct {
  u32 dummy;
} TextVertex;

typedef Da(u32) Indices;

typedef struct {
  u32 _char;
  f32 x_inner_offset;
  f32 x_offset;
  f32 y_offset;
  f32 w, h;
} Glyph;

typedef struct {
  Da(Glyph) data;
  f32       scale;
} Glyphs;

typedef Da(Glyphs) Glyphss;

typedef struct {
  u32 _char;
  f32 tl_u, tl_v;
  f32 br_u, br_v;
} GlyphMapping;

typedef Da(GlyphMapping) GlyphMappings;

typedef struct {
  f32 x, y;
  f32 w, h;
  f32 tl_u, tl_v;
  f32 br_u, br_v;
  f32 r, g, b;
  u8  p[4];
} TextSSBOEntry;

typedef Da(TextSSBOEntry) TextSSBO;

struct VuiBackendData {
  WinxWindow     *window;
  VikInstance    *instance;
  VikExecutor    *executor;
  VikBuffer      *ubo;
  VikBuffer      *text_ssbo;
  VikPipeline    *shape_pipeline;
  VikPipeline    *circle_pipeline;
  VikPipeline    *text_pipeline;
  VikMesh        *shape_mesh;
  VikMesh        *circle_mesh;
  VikMesh        *text_mesh;
  VikImage       *text_image;
  ShapeVertices   shape_vertices;
  Indices         shape_indices;
  CircleVertices  circle_vertices;
  Indices         circle_indices;
  stbtt_fontinfo  font;
  Glyphss         glyphs_cache;
  GlyphMappings   glyph_mappings_cache;
  u8             *text_data;
  u32             text_cursor_x;
  u64             image_generation;
  u64             last_image_generation;
  TextSSBO        text_ssbo_data;
  u32             max_text_ssbo_data_len;
  bool            is_glyphs_cache_dirty;
};

static VikAttr shape_attrs[] = {
  VikAttrVec2,
  VikAttrVec4,
};

static VikAttr circle_attrs[] = {
  VikAttrVec2,
  VikAttrVec2,
  VikAttrVec4,
};

static VikAttr text_attrs[] = {
  VikAttrUInt,
};

static TextVertex text_vertices[4] = {0};

static u32 text_indices[] = { 0, 1, 2, 2, 1, 3 };

static void get_window_size(VuiBackendData *data, f32 *w, f32 *h) {
  *w = data->window->width;
  *h = data->window->height;
}

static void begin_frame(VuiBackendData *data) {
  data->shape_vertices.len = 0;
  data->shape_indices.len = 0;
  data->circle_vertices.len = 0;
  data->circle_indices.len = 0;
  data->text_ssbo_data.len = 0;
  data->is_glyphs_cache_dirty = false;
}

static void ensure_atlas_is_actual(VuiBackendData *data) {
  if (data->is_glyphs_cache_dirty) {
    vik_delete_image(data->text_image);
    data->text_image = vik_make_image_ex(data->instance, data->text_data,
                                         ATLAS_WIDTH, ATLAS_HEIGHT,
                                         VikImageFormatR8,
                                         VikImageFilterLinear);

    ++data->image_generation;
    data->is_glyphs_cache_dirty = false;
  }
}

static void end_frame(VuiBackendData *data) {
  ensure_atlas_is_actual(data);

  UBO ubo_data = { data->window->width, data->window->height };
  vik_set_buffer_data(data->ubo, &ubo_data);

  if (data->shape_mesh) {
    vik_delete_mesh(data->shape_mesh);
    data->shape_mesh = NULL;
  }

  if (data->circle_mesh) {
    vik_delete_mesh(data->circle_mesh);
    data->circle_mesh = NULL;
  }

  if (data->shape_vertices.len > 0) {
    data->shape_mesh = vik_make_mesh(data->instance,
                                     data->shape_vertices.items,
                                     data->shape_vertices.len,
                                     data->shape_indices.items,
                                     data->shape_indices.len);

    vik_cmd_use_pipeline(data->executor, data->shape_pipeline);
    vik_cmd_draw(data->executor, data->shape_mesh, 1);
  }

  if (data->circle_vertices.len > 0) {
    data->circle_mesh = vik_make_mesh(data->instance,
                                      data->circle_vertices.items,
                                      data->circle_vertices.len,
                                      data->circle_indices.items,
                                      data->circle_indices.len);

    vik_cmd_use_pipeline(data->executor, data->circle_pipeline);
    vik_cmd_draw(data->executor, data->circle_mesh, 1);
  }

  bool should_recreate_text_ssbo = data->text_ssbo_data.len > data->max_text_ssbo_data_len;
  if (should_recreate_text_ssbo) {
    vik_delete_buffer(data->text_ssbo);
    data->text_ssbo = vik_make_buffer(data->instance,
                                      data->text_ssbo_data.len *
                                      sizeof(*data->text_ssbo_data.items),
                                      VikBufferKindSSBO,
                                      VikBufferAccessCPU);
    data->max_text_ssbo_data_len = data->text_ssbo_data.len;
  }

  if (data->text_ssbo_data.len > 0)
    vik_set_buffer_data(data->text_ssbo, data->text_ssbo_data.items);

  if (should_recreate_text_ssbo ||
      data->last_image_generation < data->image_generation) {
    VikBuffer *buffers[] = { data->ubo, data->text_ssbo, };
    vik_use_resources(data->text_pipeline, buffers, ARRAY_LEN(buffers), &data->text_image, 1);

    data->last_image_generation = data->image_generation;
  }

  vik_cmd_use_pipeline(data->executor, data->text_pipeline);
  vik_cmd_draw(data->executor, data->text_mesh, data->text_ssbo_data.len);
}

static void draw_rect(VuiBackendData *data,
                      f32 x, f32 y, f32 w, f32 h,
                      f32 r, f32 g, f32 b, f32 a) {
  u32 indices[] = { 0, 1, 2, 2, 1, 3 };
  for (u32 i = 0; i < ARRAY_LEN(indices); ++i) {
    indices[i] += data->shape_vertices.len;
    DA_APPEND(data->shape_indices, indices[i]);
  }

  ShapeVertex vertices[] = {
    { x,     y,     r, g, b, a },
    { x + w, y,     r, g, b, a },
    { x,     y + h, r, g, b, a },
    { x + w, y + h, r, g, b, a },
  };
  for (u32 i = 0; i < ARRAY_LEN(vertices); ++i)
    DA_APPEND(data->shape_vertices, vertices[i]);
}

static void draw_rounded_rect(VuiBackendData *data, f32 radius,
                              f32 x, f32 y, f32 w, f32 h,
                              f32 r, f32 g, f32 b, f32 a) {
  f32 ix = x + radius;
  f32 iy = y + radius;
  f32 iw = w - radius * 2.0;
  f32 ih = h - radius * 2.0;

  u32 indices0[] = { 0, 1, 2, 2, 1, 3 };
  for (u32 i = 0; i < ARRAY_LEN(indices0); ++i) {
    indices0[i] += data->shape_vertices.len;
    DA_APPEND(data->shape_indices, indices0[i]);
  }

  ShapeVertex vertices0[] = {
    { x,     iy,      r, g, b, a },
    { x + w, iy,      r, g, b, a },
    { x,     iy + ih, r, g, b, a },
    { x + w, iy + ih, r, g, b, a },
  };
  for (u32 i = 0; i < ARRAY_LEN(vertices0); ++i)
    DA_APPEND(data->shape_vertices, vertices0[i]);

  u32 indices1[] = { 0, 1, 2, 2, 1, 3 };
  for (u32 i = 0; i < ARRAY_LEN(indices1); ++i) {
    indices1[i] += data->shape_vertices.len;
    DA_APPEND(data->shape_indices, indices1[i]);
  }

  ShapeVertex vertices1[] = {
    { ix,      y,     r, g, b, a },
    { ix + iw, y,     r, g, b, a },
    { ix,      y + h, r, g, b, a },
    { ix + iw, y + h, r, g, b, a },
  };
  for (u32 i = 0; i < ARRAY_LEN(vertices1); ++i)
    DA_APPEND(data->shape_vertices, vertices1[i]);

  f32 x_offsets[2] = { x, ix + iw - radius };
  f32 y_offsets[2] = { y, iy + ih - radius };
  static f32 us[4][4] = {
    { 0.5, -0.5,  0.5, -0.5 },
    { 0.5, -0.5,  0.5, -0.5 },
    { -0.5, 0.5, -0.5,  0.5 },
    { -0.5, 0.5, -0.5,  0.5 },
  };
  static f32 vs[4][4] = {
    {  0.5,  0.5, -0.5, -0.5 },
    { -0.5, -0.5,  0.5,  0.5 },
    {  0.5,  0.5, -0.5, -0.5 },
    { -0.5, -0.5,  0.5,  0.5 },
  };

  f32 d = radius * 2.0;

  for (u32 i = 0; i < 4; ++i) {
    u32 indices2[] = { 0, 1, 2, 2, 1, 3 };
    for (u32 j = 0; j < ARRAY_LEN(indices2); ++j) {
      indices2[j] += data->circle_vertices.len;
      DA_APPEND(data->circle_indices, indices2[j]);
    }

    f32 ox = x_offsets[i / 2];
    f32 oy = y_offsets[i % 2];
    f32 *u = us[i];
    f32 *v = vs[i];

    CircleVertex vertices2[] = {
      { ox,     oy,     u[0], v[0], r, g, b, a },
      { ox + d, oy,     u[1], v[1], r, g, b, a },
      { ox,     oy + d, u[2], v[2], r, g, b, a },
      { ox + d, oy + d, u[3], v[3], r, g, b, a },
    };
    for (u32 j = 0; j < ARRAY_LEN(vertices2); ++j)
      DA_APPEND(data->circle_vertices, vertices2[j]);
  }
}

static void get_char_data(VuiBackendData *data, u32 _char, f32 scale,
                          f32 *out_inner_x, f32 *out_x, f32 *out_y,
                          f32 *out_width, f32 *out_height) {
  Glyphs *cache = NULL;

  for (u32 i = 0; i < data->glyphs_cache.len; ++i) {
    if (fabsf(scale - data->glyphs_cache.items[i].scale) < 0.001) {
      cache = data->glyphs_cache.items + i;
      break;
    }
  }

  if (!cache) {
    DA_APPEND(data->glyphs_cache, (Glyphs) {0});
    cache = data->glyphs_cache.items + data->glyphs_cache.len - 1;
  }

  for (u32 i = 0; i < cache->data.len; ++i) {
    Glyph *glyph = cache->data.items + i;
    if (glyph->_char == _char) {
      if (out_inner_x)
        *out_inner_x += glyph->x_inner_offset;
      if (out_x)
        *out_x += glyph->x_offset;
      if (out_y)
        *out_y += glyph->y_offset;
      if (out_width)
        *out_width = glyph->w;
      if (out_height)
        *out_height = glyph->h;
      return;
    }
  }

  f32 scale_y = stbtt_ScaleForPixelHeight(&data->font, scale);

  i32 advance, lsb;
  stbtt_GetCodepointHMetrics(&data->font, _char, &advance, &lsb);

  i32 ascent;
  stbtt_GetFontVMetrics(&data->font, &ascent, NULL, NULL);

  i32 x0, y0, x1, y1;
  stbtt_GetCodepointBitmapBox(&data->font, _char, scale_y, scale_y, &x0, &y0, &x1, &y1);

  f32 x_inner_offset = lsb * scale_y;
  f32 x_offset = advance * scale_y;
  f32 y_offset = ascent * scale_y + y0;
  f32 o_width = (f32) x1 - (f32) x0;
  f32 o_height = (f32) y1 - (f32) y0;

  Glyph glyph = {
    _char,
    x_inner_offset,
    x_offset, y_offset,
    o_width, o_height,
  };
  DA_APPEND(cache->data, glyph);

  if (out_inner_x)
    *out_inner_x += x_inner_offset;
  if (out_x)
    *out_x += x_offset;
  if (out_y)
    *out_y += y_offset;
  if (out_width)
    *out_width = o_width;
  if (out_height)
    *out_height = o_height;
}

static void get_char_mapping(VuiBackendData *data, u32 _char,
                             f32 *out_tl_u, f32 *out_tl_v,
                             f32 *out_br_u, f32 *out_br_v) {
  for (u32 i = 0; i < data->glyph_mappings_cache.len; ++i) {
    GlyphMapping *mapping = data->glyph_mappings_cache.items + i;
    if (mapping->_char == _char) {
      if (out_tl_u)
        *out_tl_u = mapping->tl_u;
      if (out_tl_v)
        *out_tl_v = mapping->tl_v;
      if (out_br_u)
        *out_br_u = mapping->br_u;
      if (out_br_v)
        *out_br_v = mapping->br_v;
      return;
    }
  }

  f32 scale_y = stbtt_ScaleForPixelHeight(&data->font, FONT_RENDER_SCALE);

  i32 width = 0, height = 0;
  u8 *bitmap = stbtt_GetCodepointSDF(&data->font, scale_y, _char, 0, ONEDGE_VALUE,
                                     PIXEL_DIST_SCALE, &width, &height, NULL, NULL);

  if (data->text_cursor_x + width + ATLAS_PADDING > ATLAS_WIDTH) {
    data->text_cursor_x = 0;

    memset(data->text_data, 0, ATLAS_WIDTH * ATLAS_HEIGHT * sizeof(*data->text_data));

    data->glyph_mappings_cache.len = 0;
  }

  // If this fails, increase atlas size
  assert(data->text_cursor_x + width + ATLAS_PADDING <= ATLAS_WIDTH);
  assert(height <= ATLAS_HEIGHT);

  data->text_cursor_x += ATLAS_PADDING;

  if (_char != U' ')
    for (u32 y = 0; y < (u32) height; ++y)
      for (u32 x = 0; x < (u32) width; ++x)
        data->text_data[y * ATLAS_WIDTH + x + data->text_cursor_x] = bitmap[y * width + x];

  f32 o_tl_u = (f32) data->text_cursor_x / ATLAS_WIDTH;
  f32 o_tl_v = 0.0;

  data->text_cursor_x += width;

  f32 o_br_u = (f32) data->text_cursor_x / ATLAS_WIDTH;
  f32 o_br_v = (f32) height / ATLAS_HEIGHT;

  stbtt_FreeSDF(bitmap, NULL);

  GlyphMapping mapping = {
    _char,
    o_tl_u, o_tl_v,
    o_br_u, o_br_v,
  };
  DA_APPEND(data->glyph_mappings_cache, mapping);
  data->is_glyphs_cache_dirty = true;

  if (out_tl_u)
    *out_tl_u = o_tl_u;
  if (out_tl_v)
    *out_tl_v = o_tl_v;
  if (out_br_u)
    *out_br_u = o_br_u;
  if (out_br_v)
    *out_br_v = o_br_v;
}

static f32 measure_text(VuiBackendData *data, const char *text, f32 font_scale) {
  f32 width = 0.0;

  u32 len = strlen(text);
  for (u32 i = 0; i < len; ++i)
    get_char_data(data, text[i], font_scale,
                  NULL, &width, NULL, NULL, NULL);

  return width;
}

static void draw_text(VuiBackendData *data, const char *text,
                      f32 font_scale, f32 x, f32 y,
                      f32 r, f32 g, f32 b, f32 a) {
  (void) a;

  u32 len = strlen(text);
  for (u32 i = 0; i < len; ++i) {
    TextSSBOEntry text_entry = {
      x, y,       // Modified by get_char_data
      10.0, 10.0, // Overwritten by get_char_data
      0.0, 0.0,   // Overwritten by get_char_data
      1.0, 1.0,   // Overwritten by get_char_data
      r, g, b,
      {},
    };
    get_char_data(data, text[i], font_scale,
                  &text_entry.x, &x, &text_entry.y,
                  &text_entry.w, &text_entry.h);
    get_char_mapping(data, text[i],
                     &text_entry.tl_u, &text_entry.tl_v,
                     &text_entry.br_u, &text_entry.br_v);
    DA_APPEND(data->text_ssbo_data, text_entry);
  }
}

static void destroy(VuiBackendData *data) {
  if (data->shape_mesh)
    vik_delete_mesh(data->shape_mesh);
  if (data->circle_mesh)
    vik_delete_mesh(data->circle_mesh);
  vik_delete_mesh(data->text_mesh);
  vik_delete_pipeline(data->shape_pipeline);
  vik_delete_pipeline(data->circle_pipeline);
  vik_delete_pipeline(data->text_pipeline);
  vik_delete_buffer(data->ubo);
  vik_delete_buffer(data->text_ssbo);
  vik_delete_image(data->text_image);

  if (data->shape_vertices.items)
    free(data->shape_vertices.items);
  if (data->shape_indices.items)
    free(data->shape_indices.items);
  if (data->circle_vertices.items)
    free(data->circle_vertices.items);
  if (data->circle_indices.items)
    free(data->circle_indices.items);
  if (data->text_ssbo_data.items)
    free(data->text_ssbo_data.items);
  free(data->text_data);
  free(data);
}

bool vui_winx_vulkan_backend_init(VuiBackend *backend,
                                  WinxWindow *window,
                                  VikInstance *instance,
                                  VikExecutor *executor) {
  backend->data = malloc(sizeof(*backend->data));
  memset(backend->data, 0, sizeof(*backend->data));
  backend->data->window = window;
  backend->data->instance = instance;
  backend->data->executor = executor;

  backend->data->ubo = vik_make_buffer(instance, sizeof(UBO),
                                       VikBufferKindUBO,
                                       VikBufferAccessCPU);

  VikShader *shape_shader =
    vik_make_shader_vf(instance, SSTR(shape_vert_spv), SSTR(shape_frag_spv));
  backend->data->shape_pipeline =
    vik_make_pipeline(instance, shape_shader,
                      shape_attrs, ARRAY_LEN(shape_attrs),
                      &backend->data->ubo, 1,
                      NULL, 0);
  vik_delete_shader(shape_shader);

  VikShader *circle_shader =
    vik_make_shader_vf(instance, SSTR(circle_vert_spv), SSTR(circle_frag_spv));
  backend->data->circle_pipeline =
    vik_make_pipeline(instance, circle_shader,
                      circle_attrs, ARRAY_LEN(circle_attrs),
                      &backend->data->ubo, 1,
                      NULL, 0);
  vik_delete_shader(circle_shader);

  i32 offset = stbtt_GetFontOffsetForIndex(fonts_OpenSans_Regular_ttf, 0);
  stbtt_InitFont(&backend->data->font, fonts_OpenSans_Regular_ttf, offset);

  backend->data->text_data =
    malloc(ATLAS_WIDTH * ATLAS_HEIGHT * sizeof(*backend->data->text_data));
  backend->data->text_image = vik_make_image_ex(instance,
                                                backend->data->text_data,
                                                ATLAS_WIDTH, ATLAS_HEIGHT,
                                                VikImageFormatR8,
                                                VikImageFilterLinear);

  VikShader *text_shader =
    vik_make_shader_vf(instance, SSTR(text_vert_spv), SSTR(text_frag_spv));
  backend->data->text_ssbo =
    vik_make_buffer(instance, 1, VikBufferKindSSBO, VikBufferAccessCPU);
  VikBuffer *text_buffers[] = {
    backend->data->ubo,
    backend->data->text_ssbo,
  };
  backend->data->text_pipeline =
    vik_make_pipeline(instance, text_shader,
                      text_attrs, ARRAY_LEN(text_attrs),
                      text_buffers, ARRAY_LEN(text_buffers),
                      &backend->data->text_image, 1);
  vik_delete_shader(text_shader);

  backend->data->text_mesh = vik_make_mesh(instance,
                                           text_vertices,
                                           ARRAY_LEN(text_vertices),
                                           text_indices,
                                           ARRAY_LEN(text_indices));

  backend->get_window_size = get_window_size;
  backend->begin_frame = begin_frame;
  backend->draw_rect = draw_rect;
  backend->draw_rounded_rect = draw_rounded_rect;
  backend->measure_text = measure_text;
  backend->draw_text = draw_text;
  backend->end_frame = end_frame;
  backend->destroy = destroy;

  return true;
}

bool vui_winx_vulkan_process_event(Vui *vui, WinxEvent *event) {
  switch (event->kind) {
  case WinxEventKindMouseMove: {
    vui_on_mouse_move(vui, event->as.mouse_move.x, event->as.mouse_move.y);
  } break;

  case WinxEventKindButtonPress: {
    if (event->as.button.button == WinxMouseButtonLeft) {
      vui_on_mouse_down(vui);
      return true;
    }
  } break;

  case WinxEventKindButtonRelease: {
    if (event->as.button.button == WinxMouseButtonLeft) {
      vui_on_mouse_up(vui);
      return true;
    }
  } break;

  default: break;
  }

  return false;
}
