// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

// Use Wayland for EGL native types
#define WL_EGL_PLATFORM

#include "Common/GL/GLInterface/EGL.h"

class GLContextEGLWayland final : public GLContextEGL
{
public:
  ~GLContextEGLWayland() override;
  void Update() override;

protected:
  EGLDisplay OpenEGLDisplay() override;
  EGLNativeWindowType GetEGLNativeWindow(EGLConfig config) override;
};
