// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QEvent>
#include <QStackedWidget>
#include <QWidget>

#include "Common/WindowSystemInfo.h"

class QMouseEvent;
class QTimer;

class RenderWidget final : public QWidget
{
  Q_OBJECT

public:
  explicit RenderWidget(WindowSystemType wsi_type, QWidget* parent = nullptr);

  bool event(QEvent* event) override;
  void showFullScreen();
  QPaintEngine* paintEngine() const override;

signals:
  void EscapePressed();
  void Closed();
  void HandleChanged(void* handle);
  void SurfaceAboutToBeDestroyed();
  void SurfaceCreated(void* handle);
  void StateChanged(bool fullscreen);
  void SizeChanged(int new_width, int new_height);
  void FocusChanged(bool focus);

private:
  void HandleCursorTimer();
  void OnHideCursorChanged();
  void OnKeepOnTopChanged(bool top);
  void OnFreeLookMouseMove(QMouseEvent* event);
  void PassEventToImGui(const QEvent* event);
  void SetImGuiKeyMap();
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent* event) override;

  static constexpr int MOUSE_HIDE_DELAY = 3000;
  QTimer* m_mouse_timer;
  QPoint m_last_mouse{};
  WindowSystemType m_wsi_type;
};

// Most Wayland compositors rely on client-side decorations. Qt will only be able to draw
// decorations into a surface that isn't directly used by the renderer. A parent QStackedWidget
// is an easy way to accomplish this.
class RenderParent final : public QStackedWidget
{
  Q_OBJECT
  RenderWidget* m_render_widget;

public:
  explicit RenderParent(RenderWidget* render_widget, QWidget* parent = nullptr);

  bool event(QEvent* event) override;
};
