// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/GL/GLInterface/EGLWayland.h"

#include "VideoCommon/RenderBase.h"

GLContextEGLWayland::~GLContextEGLWayland()
{
  // The context must be destroyed before the window.
  DestroyWindowSurface();
  DestroyContext();
  if (m_render_window)
    wl_egl_window_destroy(m_render_window);
}

void GLContextEGLWayland::Update()
{
  int width, height;
  if (g_renderer)
  {
    width = std::max(g_renderer->GetWaylandWidth(), 1);
    height = std::max(g_renderer->GetWaylandHeight(), 1);
  }
  else
  {
    int bs_width = 0, bs_height = 0;
    Renderer::FetchBootstrapWaylandSize(bs_width, bs_height);
    width = std::max(bs_width, 1);
    height = std::max(bs_height, 1);
  }

  wl_egl_window_resize(m_render_window, width, height, 0, 0);

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
  if (m_render_window)
    wl_egl_window_destroy(m_render_window);

  // If passed handle is null, use the interlock to mutually synchronize host and renderer.
  if (m_wsi.render_surface == nullptr)
    m_wsi.render_surface = g_renderer->WaitForNewSurface();

  int bs_width = 0, bs_height = 0;
  Renderer::FetchBootstrapWaylandSize(bs_width, bs_height);

  m_render_window =
      wl_egl_window_create(static_cast<wl_surface*>(m_wsi.render_surface), bs_width, bs_height);

  m_backbuffer_width = bs_width;
  m_backbuffer_height = bs_height;

  return reinterpret_cast<EGLNativeWindowType>(m_render_window);
}
