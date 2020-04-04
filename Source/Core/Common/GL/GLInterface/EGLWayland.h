// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wayland-egl.h>

#include "Common/GL/GLInterface/EGL.h"

class GLContextEGLWayland final : public GLContextEGL
{
public:
  ~GLContextEGLWayland() override;

  void Update() override;

protected:
  EGLDisplay OpenEGLDisplay() override;
  EGLNativeWindowType GetEGLNativeWindow(EGLConfig config) override;

  wl_egl_window* m_render_window = nullptr;
};
