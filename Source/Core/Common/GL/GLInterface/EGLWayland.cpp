// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/GL/GLInterface/EGLWayland.h"
#include <wayland-client.h>
#include <wayland-egl.h>

GLContextEGLWayland::~GLContextEGLWayland()
{
  // The context must be destroyed before the window.
  DestroyWindowSurface();
  DestroyContext();
}

void GLContextEGLWayland::Update()
{
}

EGLDisplay GLContextEGLWayland::OpenEGLDisplay()
{
  return eglGetDisplay(static_cast<NativeDisplayType>(m_wsi.display_connection));
}

EGLNativeWindowType GLContextEGLWayland::GetEGLNativeWindow(EGLConfig config)
{
  struct wl_egl_window* egl_window = wl_egl_window_create(
      static_cast<struct wl_surface*>(m_wsi.render_surface), m_wsi.width, m_wsi.height);
  return static_cast<NativeWindowType>(egl_window);
}
