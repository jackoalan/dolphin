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
  bool Connect();

  static void wl_registry_handle_global_add(void* data, struct wl_registry* registry, uint32_t id,
                                            const char* interface, uint32_t version);
  static void wl_registry_handle_global_remove(void* data, struct wl_registry* registry,
                                               uint32_t id);

  static void wl_surface_handle_enter(void* data, struct wl_surface* surface,
                                      struct wl_output* output);
  static void wl_surface_handle_leave(void* data, struct wl_surface* surface,
                                      struct wl_output* output);

  static void wl_output_handle_geometry(void* data, struct wl_output* output, int32_t x, int32_t y,
                                        int32_t physical_width, int32_t physical_height,
                                        int32_t subpixel, const char* make, const char* model,
                                        int32_t transform);
  static void wl_output_handle_mode(void* data, struct wl_output* output, uint32_t flags,
                                    int32_t width, int32_t height, int32_t refresh);
  static void wl_output_handle_done(void* data, struct wl_output* output);
  static void wl_output_handle_scale(void* data, struct wl_output* output, int32_t factor);

  static void xdg_wm_base_handle_ping(void* data, struct xdg_wm_base* xdg_wm_base, uint32_t serial);

  static void xdg_toplevel_handle_configure(void* data, struct xdg_toplevel* xdg_toplevel,
                                            int32_t width, int32_t height, struct wl_array* states);
  static void xdg_toplevel_handle_close(void* data, struct xdg_toplevel* xdg_toplevel);

  static void xdg_surface_handle_configure(void* data, struct xdg_surface* xdg_surface,
                                           uint32_t serial);

  static constexpr struct wl_registry_listener m_wl_registry_listener = {
      .global = wl_registry_handle_global_add, .global_remove = wl_registry_handle_global_remove};
  static constexpr struct wl_surface_listener m_wl_surface_listener = {
      .enter = wl_surface_handle_enter, .leave = wl_surface_handle_leave};
  static constexpr struct wl_output_listener m_wl_output_listener = {
      .geometry = wl_output_handle_geometry,
      .mode = wl_output_handle_mode,
      .done = wl_output_handle_done,
      .scale = wl_output_handle_scale};
  static constexpr struct xdg_wm_base_listener m_xdg_wm_base_listener = {
      .ping = xdg_wm_base_handle_ping};
  static constexpr struct xdg_toplevel_listener m_xdg_toplevel_listener = {
      .configure = xdg_toplevel_handle_configure, .close = xdg_toplevel_handle_close};
  static constexpr struct xdg_surface_listener m_xdg_surface_listener = {
      .configure = xdg_surface_handle_configure};

  struct wl_display* m_display;
  struct wl_registry* m_registry;
  struct wl_compositor* m_compositor;
  struct wl_surface* m_surface;
  struct wl_output* m_output;

  struct xdg_surface* m_xdg_surface;
  struct xdg_wm_base* m_xdg_wm_base;
  struct xdg_toplevel* m_xdg_toplevel;

  struct wl_keyboard* m_keyboard;
  struct wl_seat* m_seat;
  struct wl_pointer* m_pointer;

  int32_t m_window_x = Config::Get(Config::MAIN_RENDER_WINDOW_XPOS);
  int32_t m_window_y = Config::Get(Config::MAIN_RENDER_WINDOW_YPOS);
  int32_t m_window_width = Config::Get(Config::MAIN_RENDER_WINDOW_WIDTH);
  int32_t m_window_height = Config::Get(Config::MAIN_RENDER_WINDOW_HEIGHT);
  int32_t m_scaling_factor = 1;
};

PlatformWayland::~PlatformWayland()
{
  if (m_xdg_toplevel)
    xdg_toplevel_destroy(m_xdg_toplevel);
  if (m_xdg_surface)
    xdg_surface_destroy(m_xdg_surface);
  if (m_keyboard)
    wl_keyboard_destroy(m_keyboard);
  if (m_pointer)
    wl_pointer_destroy(m_pointer);
  if (m_seat)
    wl_seat_destroy(m_seat);
  if (m_surface)
    wl_surface_destroy(m_surface);
  if (m_xdg_wm_base)
    xdg_wm_base_destroy(m_xdg_wm_base);
  if (m_compositor)
    wl_compositor_destroy(m_compositor);
  if (m_display)
    wl_display_disconnect(m_display);
}

bool PlatformWayland::Init()
{
  if (!Connect())
  {
    PanicAlert("Could not connect to Wayland session");
    return false;
  }

  xdg_toplevel_set_title(m_xdg_toplevel, "Dolphin Emulator");
  wl_surface_commit(m_surface);
  wl_display_roundtrip(m_display);

  return true;
}

bool PlatformWayland::Connect()
{
  m_display = wl_display_connect(NULL);
  if (!m_display)
    return false;

  m_registry = wl_display_get_registry(m_display);
  if (!m_registry)
    return false;

  wl_registry_add_listener(m_registry, &m_wl_registry_listener, this);
  wl_display_roundtrip(m_display);  // Wait for roundtrip so registry listener is registered

  if (!m_compositor || !m_surface || !m_xdg_wm_base)
    return false;

  m_xdg_surface = xdg_wm_base_get_xdg_surface(m_xdg_wm_base, m_surface);
  if (!m_xdg_surface)
    return false;
  xdg_surface_add_listener(m_xdg_surface, &m_xdg_surface_listener, this);

  m_xdg_toplevel = xdg_surface_get_toplevel(m_xdg_surface);
  if (!m_xdg_toplevel)
    return false;
  xdg_toplevel_add_listener(m_xdg_toplevel, &m_xdg_toplevel_listener, this);
  return true;
}

void PlatformWayland::SetTitle(const std::string& string)
{
  xdg_toplevel_set_title(m_xdg_toplevel, string.c_str());
}

void PlatformWayland::MainLoop()
{
  printf("Starting main loop\n");
  while (IsRunning())
  {
    Core::HostDispatchJobs();
    if (wl_display_dispatch(m_display) < 0)
    {
      PanicAlert("Could not process Wayland events");
      return;
    }
  }
}

WindowSystemInfo PlatformWayland::GetWindowSystemInfo() const
{
  WindowSystemInfo wsi;
  wsi.type = WindowSystemType::Wayland;
  wsi.display_connection = static_cast<void*>(m_display);
  wsi.render_window = static_cast<void*>(m_surface);
  wsi.render_surface = static_cast<void*>(m_surface);
  wsi.width = m_window_width;
  wsi.height = m_window_height;
  return wsi;
}

void PlatformWayland::wl_registry_handle_global_add(void* data, struct wl_registry* registry,
                                                    uint32_t id, const char* interface,
                                                    uint32_t version)
{
  PlatformWayland* platform = static_cast<PlatformWayland*>(data);
  if (strcmp(interface, wl_compositor_interface.name) == 0)
  {
    platform->m_compositor = static_cast<struct wl_compositor*>(
        wl_registry_bind(registry, id, &wl_compositor_interface, 4));
    platform->m_surface = wl_compositor_create_surface(platform->m_compositor);
    wl_surface_add_listener(platform->m_surface, &PlatformWayland::m_wl_surface_listener, data);
    wl_display_roundtrip(platform->m_display);
  }
  else if (strcmp(interface, xdg_wm_base_interface.name) == 0)
  {
    platform->m_xdg_wm_base =
        static_cast<struct xdg_wm_base*>(wl_registry_bind(registry, id, &xdg_wm_base_interface, 1));
    xdg_wm_base_add_listener(platform->m_xdg_wm_base, &PlatformWayland::m_xdg_wm_base_listener,
                             data);
  }
}

void PlatformWayland::wl_registry_handle_global_remove(void* data, struct wl_registry* registry,
                                                       uint32_t id)
{
  printf("Deleted registry item with id %d\n", id);
}

void PlatformWayland::wl_surface_handle_enter(void* data, struct wl_surface* surface,
                                              struct wl_output* output)
{
  PlatformWayland* platform = static_cast<PlatformWayland*>(data);
  if(platform->m_output) wl_output_release(platform->m_output);
  platform->m_output = output;
  wl_output_add_listener(output, &PlatformWayland::m_wl_output_listener, data);
  printf("Moved into output");
}

void PlatformWayland::wl_surface_handle_leave(void* data, struct wl_surface* surface,
                                              struct wl_output* output)

{
}

void PlatformWayland::wl_output_handle_geometry(void* data, struct wl_output* output, int32_t x,
                                                int32_t y, int32_t physical_width,
                                                int32_t physical_height, int32_t subpixel,
                                                const char* make, const char* model,
                                                int32_t transform)
{
}

void PlatformWayland::wl_output_handle_mode(void* data, struct wl_output* output, uint32_t flags,
                                            int32_t width, int32_t height, int32_t refresh)
{
}

void PlatformWayland::wl_output_handle_done(void* data, struct wl_output* output)
{
  PlatformWayland* platform = static_cast<PlatformWayland*>(data);
  printf("New display has scale %d\n", platform->m_scaling_factor);
  wl_surface_set_buffer_scale(platform->m_surface, platform->m_scaling_factor);
  wl_surface_commit(platform->m_surface);
}

void PlatformWayland::wl_output_handle_scale(void* data, struct wl_output* output, int32_t factor)
{
  PlatformWayland* platform = static_cast<PlatformWayland*>(data);
  platform->m_scaling_factor = factor;
}

void PlatformWayland::xdg_wm_base_handle_ping(void* data, struct xdg_wm_base* xdg_wm_base,
                                              uint32_t serial)
{
  xdg_wm_base_pong(xdg_wm_base, serial);
}

void PlatformWayland::xdg_toplevel_handle_configure(void* data, struct xdg_toplevel* xdg_toplevel,
                                                    int32_t width, int32_t height,
                                                    struct wl_array* states)
{
  printf("Got toplevel configure event, width=%d, height=%d\n", width, height);
  PlatformWayland* platform = static_cast<PlatformWayland*>(data);
  if (width != 0 && height != 0)
  {
    platform->m_window_width = width * platform->m_scaling_factor;
    platform->m_window_height = height * platform->m_scaling_factor;
    if (g_renderer)
      g_renderer->ResizeSurface(platform->m_window_width, platform->m_window_height);
  }
  else
  {
    xdg_surface_set_window_geometry(platform->m_xdg_surface, platform->m_window_x,
                                    platform->m_window_y, platform->m_window_width,
                                    platform->m_window_height);
    wl_surface_commit(platform->m_surface);
  }
}

void PlatformWayland::xdg_toplevel_handle_close(void* data, struct xdg_toplevel* xdg_toplevel)
{
  PlatformWayland* platform = static_cast<PlatformWayland*>(data);
  platform->Stop();
}

void PlatformWayland::xdg_surface_handle_configure(void* data, struct xdg_surface* xdg_surface,
                                                   uint32_t serial)
{
  xdg_surface_ack_configure(xdg_surface, serial);
}

}  // namespace

std::unique_ptr<Platform> Platform::CreateWaylandPlatform()
{
  return std::make_unique<PlatformWayland>();
}
