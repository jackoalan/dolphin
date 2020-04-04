// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/GL/GLInterface/EGLWayland.h"
#include <cstdio>

GLContextEGLWayland::~GLContextEGLWayland()
{
  DestroyWindowSurface();
  wl_egl_window_destroy(m_egl_window);
  DestroyContext();
}

void GLContextEGLWayland::Update()
{
  m_backbuffer_width = -1;
  m_backbuffer_height = -1;
}

EGLDisplay GLContextEGLWayland::OpenEGLDisplay()
{
  return eglGetDisplay(static_cast<NativeDisplayType>(m_wsi.display_connection));
}

EGLNativeWindowType GLContextEGLWayland::GetEGLNativeWindow(EGLConfig config)
{
  m_egl_window = wl_egl_window_create(static_cast<struct wl_surface*>(m_wsi.render_surface),
                                      m_wsi.width, m_wsi.height);
  return static_cast<NativeWindowType>(m_egl_window);
}
