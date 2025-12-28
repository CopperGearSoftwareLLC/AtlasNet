// RedClear.cpp
// Emscripten + OpenGL ES (WebGL) minimal example: clears the canvas to red.

#ifdef __EMSCRIPTEN__
  #include <emscripten/emscripten.h>
  #include <emscripten/html5.h>
  // Use GLES2 for maximum compatibility (maps to WebGL1).
  #include <GLES2/gl2.h>
#else
  #error "This file is intended to be built with Emscripten for the web."
#endif

static EMSCRIPTEN_WEBGL_CONTEXT_HANDLE g_ctx = 0;

static void frame() {
  // Clear to solid red
  glViewport(0, 0, 0, 0); // will be overwritten below if we query size
  int w = 0, h = 0;
  emscripten_get_canvas_element_size("#canvas", &w, &h);
  glViewport(0, 0, w, h);

  glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
}

int main() {
  // Create a WebGL context on the canvas
  EmscriptenWebGLContextAttributes attr;
  emscripten_webgl_init_context_attributes(&attr);
  attr.alpha = EM_TRUE;
  attr.depth = EM_FALSE;
  attr.stencil = EM_FALSE;
  attr.antialias = EM_TRUE;
  attr.majorVersion = 1; // WebGL1 (GLES2)
  attr.minorVersion = 0;

  g_ctx = emscripten_webgl_create_context("#canvas", &attr);
  if (!g_ctx) return 1;

  if (emscripten_webgl_make_context_current(g_ctx) != EMSCRIPTEN_RESULT_SUCCESS)
    return 2;

  // Render loop
  emscripten_set_main_loop(frame, 0, 1);
  return 0;
}
