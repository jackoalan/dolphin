// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

// wayland-egl must be before egl to set correct defines
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

private:
  struct wl_egl_window* m_egl_window;
};
