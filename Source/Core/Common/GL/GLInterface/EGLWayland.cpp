// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/GL/GLInterface/EGLWayland.h"
#include "VideoCommon/RenderBase.h"

GLContextEGLWayland::~GLContextEGLWayland()
{
  DestroyWindowSurface();
  wl_egl_window_destroy(m_egl_window);
  DestroyContext();
}

void GLContextEGLWayland::Update()
{
  int width = g_renderer->GetNewWidth();
  int height = g_renderer->GetNewHeight();

  wl_egl_window_resize(m_egl_window, width, height, 0, 0);

  m_backbuffer_width = width;
  m_backbuffer_height = height;
}

EGLDisplay GLContextEGLWayland::OpenEGLDisplay()
{
  return eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR,
                               static_cast<wl_display*>(m_wsi.display_connection), nullptr);
}

EGLNativeWindowType GLContextEGLWayland::GetEGLNativeWindow(EGLConfig config)
{
  if (m_egl_window)
    wl_egl_window_destroy(m_egl_window);

  // If passed handle is null, use the interlock to mutually synchronize host and renderer.
  if (m_wsi.render_surface == nullptr)
    m_wsi.render_surface = g_renderer->WaitForNewSurface();

  m_egl_window = wl_egl_window_create(static_cast<struct wl_surface*>(m_wsi.render_surface),
                                      m_wsi.width, m_wsi.height);

  return static_cast<NativeWindowType>(m_egl_window);
}
