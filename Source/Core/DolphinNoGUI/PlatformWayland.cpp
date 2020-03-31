// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <unistd.h>

#include "DolphinNoGUI/Platform.h"
#include "DolphinNoGUI/xdg-shell-client-protocol.h"

#include "Common/MsgHandler.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/State.h"

#include <climits>
#include <cstdio>
#include <cstring>

#include <wayland-client.h>

#include "VideoCommon/RenderBase.h"

namespace
{
class PlatformWayland : public Platform
{
public:
  ~PlatformWayland() override;

  bool Init() override;
  void SetTitle(const std::string& string) override;
  void MainLoop() override;

  WindowSystemInfo GetWindowSystemInfo() const override;

private:
  static void registry_listener_global_add(void* self, struct wl_registry* registry, uint32_t id,
                                           const char* interface, uint32_t version);
  static void registry_listener_global_remove(void* self, struct wl_registry* registry,
                                              uint32_t id);
  static void xdg_wm_base_listener_ping(void* data, struct xdg_wm_base* xdg_wm_base,
                                        uint32_t serial);

  static constexpr struct wl_registry_listener m_registry_listener = {
      .global = PlatformWayland::registry_listener_global_add,
      .global_remove = PlatformWayland::registry_listener_global_remove};

  static constexpr struct xdg_wm_base_listener m_xdg_wm_base_listener = {
      .ping = PlatformWayland::xdg_wm_base_listener_ping};

  struct wl_display* m_display;
  struct wl_registry* m_registry;
  struct wl_compositor* m_compositor;
  struct wl_surface* m_surface;
  struct wl_shm* m_shm;

  struct xdg_surface* m_xdg_surface;
  struct xdg_wm_base* m_wm_base;
  struct xdg_toplevel* m_toplevel;

  struct wl_keyboard* m_keyboard;
  struct wl_seat* m_seat;
  struct wl_pointer* m_pointer;
};

PlatformWayland::~PlatformWayland()
{
  wl_keyboard_destroy(m_keyboard);
  wl_pointer_destroy(m_pointer);
  wl_seat_destroy(m_seat);
  wl_shm_destroy(m_shm);
  wl_surface_destroy(m_surface);
  wl_compositor_destroy(m_compositor);
  wl_registry_destroy(m_registry);
  wl_display_disconnect(m_display);
}

bool PlatformWayland::Init()
{
  m_display = wl_display_connect(NULL);
  if (!m_display)
  {
    PanicAlert("Could not connect to Wayland display");
    return false;
  }

  m_registry = wl_display_get_registry(m_display);
  if (!m_registry)
  {
    PanicAlert("Could not get Wayland registry");
    return false;
  }

  wl_registry_add_listener(m_registry, &m_registry_listener, this);
  wl_display_roundtrip(m_display);  // Wait for roundtrip so registry listener is registered

  if (!m_compositor)
  {
    PanicAlert("Could not get Wayland compositor");
    return false;
  }
  if (!m_surface)
  {
    PanicAlert("Could not get Wayland surface");
    return false;
  }
  if (!m_shm)
  {
    PanicAlert("Could not get Wayland shm object");
    return false;
  }

  return true;
}

void PlatformWayland::SetTitle(const std::string& string)
{
}

void PlatformWayland::MainLoop()
{
  Core::HostDispatchJobs();
  if (wl_display_dispatch(m_display) < 0)
  {
    PanicAlert("Could not process Wayland events");
    return;
  }
}

WindowSystemInfo PlatformWayland::GetWindowSystemInfo() const
{
  WindowSystemInfo wsi;
  wsi.type = WindowSystemType::Wayland;
  wsi.display_connection = static_cast<void*>(m_display);
  wsi.render_window = static_cast<void*>(m_surface);
  wsi.render_surface = static_cast<void*>(m_surface);
  wsi.width = -1;   // TODO
  wsi.height = -1;  // TODO
  return wsi;
}

void PlatformWayland::registry_listener_global_add(void* data, struct wl_registry* registry,
                                                   uint32_t id, const char* interface,
                                                   uint32_t version)
{
  printf("Got registry item with name %s id %u version %u\n", interface, id, version);
  PlatformWayland* platform = static_cast<PlatformWayland*>(data);
  if (strcmp(interface, wl_compositor_interface.name) == 0)
  {
    platform->m_compositor = static_cast<struct wl_compositor*>(
        wl_registry_bind(registry, id, &wl_compositor_interface, 4));
    platform->m_surface = wl_compositor_create_surface(platform->m_compositor);
  }
  else if (strcmp(interface, wl_shm_interface.name) == 0)
  {
    platform->m_shm =
        static_cast<struct wl_shm*>(wl_registry_bind(registry, id, &wl_shm_interface, 1));
  }
  else if (strcmp(interface, xdg_wm_base_interface.name) == 0)
  {
    platform->m_wm_base =
        static_cast<struct xdg_wm_base*>(wl_registry_bind(registry, id, &xdg_wm_base_interface, 1));
    xdg_wm_base_add_listener(platform->m_wm_base, &PlatformWayland::m_xdg_wm_base_listener, data);
  }
}

void PlatformWayland::registry_listener_global_remove(void* data, struct wl_registry* registry,
                                                      uint32_t id)
{
  printf("Deleted registry item with id %d\n", id);
}

void PlatformWayland::xdg_wm_base_listener_ping(void* data, struct xdg_wm_base* xdg_wm_base,
                                                uint32_t serial)
{
  xdg_wm_base_pong(xdg_wm_base, serial);
}
}  // namespace

std::unique_ptr<Platform> Platform::CreateWaylandPlatform()
{
  return std::make_unique<PlatformWayland>();
}
