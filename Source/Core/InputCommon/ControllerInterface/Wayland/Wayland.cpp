// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Logging/Log.h"

#include <fmt/format.h>

#include "InputCommon/ControllerInterface/Wayland/Wayland.h"

#include <cstring>
#include <sys/mman.h>

// Mouse axis control tuning. Unlike absolute mouse position, relative mouse
// motion data needs to be tweaked and smoothed out a bit to be usable.

// Mouse axis control output is simply divided by this number. In practice,
// that just means you can use a smaller "dead zone" if you bind axis controls
// to a joystick. No real need to make this customizable.
#define MOUSE_AXIS_SENSITIVITY 8.0f

// The mouse axis controls use a weighted running average. Each frame, the new
// value is the average of the old value and the amount of relative mouse
// motion during that frame. The old value is weighted by a ratio of
// MOUSE_AXIS_SMOOTHING:1 compared to the new value. Increasing
// MOUSE_AXIS_SMOOTHING makes the controls smoother, decreasing it makes them
// more responsive. This might be useful as a user-customizable option.
#define MOUSE_AXIS_SMOOTHING 1.5f

namespace ciface::Wayland
{
// Wayland proxy display, queue, and registry for handling events independently from whatever
// application framework or threading model is in use.
static class WaylandProxy
{
  wl_display* m_display = nullptr;
  wl_event_queue* m_queue = nullptr;
  wl_display* m_display_wrapper = nullptr;
  wl_registry* m_registry = nullptr;
  std::unordered_map<uint32_t, uint32_t> m_seat_ids;

  static void SyncCallback(void* data, struct wl_callback* callback, uint32_t serial)
  {
    *reinterpret_cast<int*>(data) = 1;
    wl_callback_destroy(callback);
  }

  static const wl_callback_listener s_sync_listener;

  // Modified implementation of wl_display_roundtrip_queue to use existing display wrapper.
  int RoundtripSync()
  {
    int ret = 0;
    int done = 0;

    wl_callback* callback = wl_display_sync(m_display_wrapper);

    if (callback == nullptr)
      return -1;

    wl_callback_add_listener(callback, &s_sync_listener, &done);
    while (!done && ret >= 0)
      ret = wl_display_dispatch_queue(m_display, m_queue);

    if (ret == -1 && !done)
      wl_callback_destroy(callback);

    return ret;
  }

public:
  void Roundtrip()
  {
    if (m_queue)
    {
      if (RoundtripSync() < 0)
      {
        const int err = wl_display_get_error(m_display);
        if ((err == EPIPE || err == ECONNRESET))
          ERROR_LOG(SERIALINTERFACE, "Lost connection to the Wayland compositor.");
        else
          ERROR_LOG(SERIALINTERFACE, "Wayland fatal error.");
        Finish();
      }
    }
  }

  static const wl_registry_listener s_registry_listener;

  void wl_registry_listener_global(struct wl_registry* registry, uint32_t id,
                                   const char* interface_in, uint32_t version)
  {
    // Gnome crashes if this is 5
    constexpr uint32_t max_wl_seat_version = 4;
    if (strcmp("wl_seat", interface_in) == 0)
      m_seat_ids[id] = std::min(version, max_wl_seat_version);
  }
  static void wl_registry_listener_global(void* data, struct wl_registry* registry, uint32_t id,
                                          const char* interface, uint32_t version)
  {
    reinterpret_cast<WaylandProxy*>(data)->wl_registry_listener_global(registry, id, interface,
                                                                       version);
  }

  void wl_registry_listener_global_remove(struct wl_registry* registry, uint32_t id)
  {
    m_seat_ids.erase(id);
  }
  static void wl_registry_listener_global_remove(void* data, struct wl_registry* registry,
                                                 uint32_t id)
  {
    reinterpret_cast<WaylandProxy*>(data)->wl_registry_listener_global_remove(registry, id);
  }

  const std::unordered_map<uint32_t, uint32_t>& SeatIds() const { return m_seat_ids; }

  bool HasSeatId(uint32_t seat_id) const { return m_seat_ids.find(seat_id) != m_seat_ids.end(); }

  wl_seat* BindSeat(uint32_t seat_id, uint32_t seat_version)
  {
    if (m_registry)
    {
      return reinterpret_cast<wl_seat*>(
          wl_registry_bind(m_registry, seat_id, &wl_seat_interface, seat_version));
    }
    return nullptr;
  }

  void Setup(void* const display)
  {
    m_display = reinterpret_cast<wl_display*>(display);

    m_queue = wl_display_create_queue(m_display);
    if (!m_queue)
    {
      ERROR_LOG(SERIALINTERFACE, "Failed to wl_display_create_queue");
      Finish();
      return;
    }

    m_display_wrapper = reinterpret_cast<wl_display*>(wl_proxy_create_wrapper(m_display));
    if (!m_display_wrapper)
    {
      ERROR_LOG(SERIALINTERFACE, "Failed to wl_proxy_create_wrapper");
      Finish();
      return;
    }

    wl_proxy_set_queue((struct wl_proxy*)m_display_wrapper, m_queue);

    m_registry = wl_display_get_registry(m_display_wrapper);
    if (!m_registry)
    {
      ERROR_LOG(SERIALINTERFACE, "Failed to wl_display_get_registry");
      Finish();
      return;
    }

    wl_registry_add_listener(m_registry, &s_registry_listener, this);
  }

  void Finish()
  {
    m_seat_ids.clear();
    if (m_registry)
    {
      wl_registry_destroy(m_registry);
      m_registry = nullptr;
    }
    if (m_display_wrapper)
    {
      wl_proxy_wrapper_destroy(m_display_wrapper);
      m_display_wrapper = nullptr;
    }
    if (m_queue)
    {
      wl_event_queue_destroy(m_queue);
      m_queue = nullptr;
    }
    m_display = nullptr;
  }
} g_proxy;

const wl_callback_listener WaylandProxy::s_sync_listener = {SyncCallback};

const wl_registry_listener WaylandProxy::s_registry_listener = {wl_registry_listener_global,
                                                                wl_registry_listener_global_remove};

void Seat::wl_pointer_listener_enter(struct wl_pointer* wl_pointer, uint32_t serial,
                                     struct wl_surface* surface, wl_fixed_t surface_x,
                                     wl_fixed_t surface_y)
{
  if (surface == m_surface)
    m_in_surface = true;
}
void Seat::wl_pointer_listener_enter(void* data, struct wl_pointer* wl_pointer, uint32_t serial,
                                     struct wl_surface* surface, wl_fixed_t surface_x,
                                     wl_fixed_t surface_y)
{
  reinterpret_cast<Seat*>(data)->wl_pointer_listener_enter(wl_pointer, serial, surface, surface_x,
                                                           surface_y);
}

void Seat::wl_pointer_listener_leave(struct wl_pointer* wl_pointer, uint32_t serial,
                                     struct wl_surface* surface)
{
  if (surface == m_surface)
    m_in_surface = false;
}
void Seat::wl_pointer_listener_leave(void* data, struct wl_pointer* wl_pointer, uint32_t serial,
                                     struct wl_surface* surface)
{
  reinterpret_cast<Seat*>(data)->wl_pointer_listener_leave(wl_pointer, serial, surface);
}

void Seat::wl_pointer_listener_motion(struct wl_pointer* wl_pointer, uint32_t time,
                                      wl_fixed_t surface_x, wl_fixed_t surface_y)
{
  if (!m_in_surface)
    return;

  int win_width, win_height;
  g_controller_interface.FetchWindowSize(win_width, win_height);

  if (win_width > 0 && win_height > 0)
  {
    const auto window_scale = g_controller_interface.GetWindowInputScale();

    // the mouse position as a range from -1 to 1
    m_state.cursor.x = (wl_fixed_to_double(surface_x) / win_width * 2 - 1) * window_scale.x;
    m_state.cursor.y = (wl_fixed_to_double(surface_y) / win_height * 2 - 1) * window_scale.y;
  }
  else
  {
    m_state.cursor.x = 0.f;
    m_state.cursor.y = 0.f;
  }
}
void Seat::wl_pointer_listener_motion(void* data, struct wl_pointer* wl_pointer, uint32_t time,
                                      wl_fixed_t surface_x, wl_fixed_t surface_y)
{
  reinterpret_cast<Seat*>(data)->wl_pointer_listener_motion(wl_pointer, time, surface_x, surface_y);
}

void Seat::wl_pointer_listener_button(struct wl_pointer* wl_pointer, uint32_t serial, uint32_t time,
                                      uint32_t button, uint32_t state)
{
  if (state == WL_POINTER_BUTTON_STATE_PRESSED)
    m_state.buttons |= 1 << (button - 0x110);
  else
    m_state.buttons &= ~(1 << (button - 0x110));
}
void Seat::wl_pointer_listener_button(void* data, struct wl_pointer* wl_pointer, uint32_t serial,
                                      uint32_t time, uint32_t button, uint32_t state)
{
  reinterpret_cast<Seat*>(data)->wl_pointer_listener_button(wl_pointer, serial, time, button,
                                                            state);
}

void Seat::wl_pointer_listener_axis(struct wl_pointer* wl_pointer, uint32_t time, uint32_t axis,
                                    wl_fixed_t value)
{
  if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL)
    m_state.accum_axis.x += wl_fixed_to_double(value);
  else
    m_state.accum_axis.y += wl_fixed_to_double(value);
}
void Seat::wl_pointer_listener_axis(void* data, struct wl_pointer* wl_pointer, uint32_t time,
                                    uint32_t axis, wl_fixed_t value)
{
  reinterpret_cast<Seat*>(data)->wl_pointer_listener_axis(wl_pointer, time, axis, value);
}

void Seat::wl_pointer_listener_frame(struct wl_pointer* wl_pointer)
{
}
void Seat::wl_pointer_listener_frame(void* data, struct wl_pointer* wl_pointer)
{
  reinterpret_cast<Seat*>(data)->wl_pointer_listener_frame(wl_pointer);
}

void Seat::wl_pointer_listener_axis_source(struct wl_pointer* wl_pointer, uint32_t axis_source)
{
}
void Seat::wl_pointer_listener_axis_source(void* data, struct wl_pointer* wl_pointer,
                                           uint32_t axis_source)
{
  reinterpret_cast<Seat*>(data)->wl_pointer_listener_axis_source(wl_pointer, axis_source);
}

void Seat::wl_pointer_listener_axis_stop(struct wl_pointer* wl_pointer, uint32_t time,
                                         uint32_t axis)
{
}
void Seat::wl_pointer_listener_axis_stop(void* data, struct wl_pointer* wl_pointer, uint32_t time,
                                         uint32_t axis)
{
  reinterpret_cast<Seat*>(data)->wl_pointer_listener_axis_stop(wl_pointer, time, axis);
}

void Seat::wl_pointer_listener_axis_discrete(struct wl_pointer* wl_pointer, uint32_t axis,
                                             int32_t discrete)
{
}
void Seat::wl_pointer_listener_axis_discrete(void* data, struct wl_pointer* wl_pointer,
                                             uint32_t axis, int32_t discrete)
{
  reinterpret_cast<Seat*>(data)->wl_pointer_listener_axis_discrete(wl_pointer, axis, discrete);
}

const wl_pointer_listener Seat::s_pointer_listener = {
    wl_pointer_listener_enter,        wl_pointer_listener_leave,
    wl_pointer_listener_motion,       wl_pointer_listener_button,
    wl_pointer_listener_axis,         wl_pointer_listener_frame,
    wl_pointer_listener_axis_source,  wl_pointer_listener_axis_stop,
    wl_pointer_listener_axis_discrete};

void Seat::wl_keyboard_listener_keymap(struct wl_keyboard* wl_keyboard, uint32_t format, int32_t fd,
                                       uint32_t size)
{
  // Changes to the keymap affect dolphin's view of the device, therefore the device is
  // invalidated.
  if (m_constructed)
  {
    m_valid = false;
    return;
  }

  ClearKeymap();

  if (format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1)
  {
    const char* keymap = (char*)mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (keymap == MAP_FAILED)
      return;

    m_keymap = xkb_keymap_new_from_buffer(m_xkb, keymap, strnlen(keymap, size),
                                          XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);

    if (m_keymap)
      m_keystate = xkb_state_new(m_keymap);

    munmap((void*)keymap, size);
  }
}
void Seat::wl_keyboard_listener_keymap(void* data, struct wl_keyboard* wl_keyboard, uint32_t format,
                                       int32_t fd, uint32_t size)
{
  reinterpret_cast<Seat*>(data)->wl_keyboard_listener_keymap(wl_keyboard, format, fd, size);
}

void Seat::wl_keyboard_listener_enter(struct wl_keyboard* wl_keyboard, uint32_t serial,
                                      struct wl_surface* surface, struct wl_array* keys)
{
}
void Seat::wl_keyboard_listener_enter(void* data, struct wl_keyboard* wl_keyboard, uint32_t serial,
                                      struct wl_surface* surface, struct wl_array* keys)
{
  reinterpret_cast<Seat*>(data)->wl_keyboard_listener_enter(wl_keyboard, serial, surface, keys);
}

void Seat::wl_keyboard_listener_leave(struct wl_keyboard* wl_keyboard, uint32_t serial,
                                      struct wl_surface* surface)
{
}
void Seat::wl_keyboard_listener_leave(void* data, struct wl_keyboard* wl_keyboard, uint32_t serial,
                                      struct wl_surface* surface)
{
  reinterpret_cast<Seat*>(data)->wl_keyboard_listener_leave(wl_keyboard, serial, surface);
}

void Seat::wl_keyboard_listener_key(struct wl_keyboard* wl_keyboard, uint32_t serial, uint32_t time,
                                    uint32_t key, uint32_t state)
{
  key = key + 8;
  if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
    m_state.keyboard[key / 8] |= 1 << (key % 8);
  else
    m_state.keyboard[key / 8] &= ~(1 << (key % 8));
}
void Seat::wl_keyboard_listener_key(void* data, struct wl_keyboard* wl_keyboard, uint32_t serial,
                                    uint32_t time, uint32_t key, uint32_t state)
{
  reinterpret_cast<Seat*>(data)->wl_keyboard_listener_key(wl_keyboard, serial, time, key, state);
}

void Seat::wl_keyboard_listener_modifiers(struct wl_keyboard* wl_keyboard, uint32_t serial,
                                          uint32_t mods_depressed, uint32_t mods_latched,
                                          uint32_t mods_locked, uint32_t group)
{
  if (m_keystate)
  {
    xkb_state_update_mask(m_keystate, mods_depressed, mods_latched, mods_locked, group, group,
                          group);
  }
}
void Seat::wl_keyboard_listener_modifiers(void* data, struct wl_keyboard* wl_keyboard,
                                          uint32_t serial, uint32_t mods_depressed,
                                          uint32_t mods_latched, uint32_t mods_locked,
                                          uint32_t group)
{
  reinterpret_cast<Seat*>(data)->wl_keyboard_listener_modifiers(wl_keyboard, serial, mods_depressed,
                                                                mods_latched, mods_locked, group);
}

void Seat::wl_keyboard_listener_repeat_info(struct wl_keyboard* wl_keyboard, int32_t rate,
                                            int32_t delay)
{
}
void Seat::wl_keyboard_listener_repeat_info(void* data, struct wl_keyboard* wl_keyboard,
                                            int32_t rate, int32_t delay)
{
  reinterpret_cast<Seat*>(data)->wl_keyboard_listener_repeat_info(wl_keyboard, rate, delay);
}

const wl_keyboard_listener Seat::s_keyboard_listener = {
    wl_keyboard_listener_keymap, wl_keyboard_listener_enter,     wl_keyboard_listener_leave,
    wl_keyboard_listener_key,    wl_keyboard_listener_modifiers, wl_keyboard_listener_repeat_info};

void Seat::wl_seat_listener_capabilities(struct wl_seat* wl_seat, uint32_t capabilities)
{
  // The seat capabilities are only considered at construct-time so dolphin's view of the device
  // does not change. Removal of the pointer or keyboard invalidates the entire device.
  if (m_constructed)
  {
    if ((m_pointer && !(capabilities & WL_SEAT_CAPABILITY_POINTER)) ||
        (m_keyboard && !(capabilities & WL_SEAT_CAPABILITY_KEYBOARD)))
      m_valid = false;
    return;
  }

  if (!m_pointer && (capabilities & WL_SEAT_CAPABILITY_POINTER))
  {
    m_pointer = wl_seat_get_pointer(wl_seat);
    wl_pointer_add_listener(m_pointer, &s_pointer_listener, this);

    // Mouse Buttons
    for (int i = 0; i < 32; i++)
      AddInput(new Button(i, &m_state.buttons));

    // Mouse Cursor, X-/+ and Y-/+
    for (int i = 0; i != 4; ++i)
      AddInput(new Cursor(!!(i & 2), !!(i & 1), (i & 2) ? &m_state.cursor.y : &m_state.cursor.x));

    // Mouse Axis, X-/+ and Y-/+
    for (int i = 0; i != 4; ++i)
      AddInput(new Axis(!!(i & 2), !!(i & 1), (i & 2) ? &m_state.axis.y : &m_state.axis.x));
  }

  if (!m_keyboard && (capabilities & WL_SEAT_CAPABILITY_KEYBOARD))
  {
    m_keyboard = wl_seat_get_keyboard(wl_seat);
    wl_keyboard_add_listener(m_keyboard, &s_keyboard_listener, this);

    // Round-trip to get keymap.
    g_proxy.Roundtrip();

    // Keyboard Keys
    if (m_keystate)
    {
      const xkb_keycode_t min_keycode = xkb_keymap_min_keycode(m_keymap);
      const xkb_keycode_t max_keycode = xkb_keymap_max_keycode(m_keymap);

      if (max_keycode > 0)
      {
        const size_t bits_char_length = max_keycode / 8 + 1;
        m_state.keyboard.reset(new char[bits_char_length]);
        std::memset(m_state.keyboard.get(), 0, bits_char_length);
      }

      for (xkb_keycode_t i = min_keycode; i <= max_keycode; ++i)
      {
        Key* temp_key = new Key(m_keystate, i, m_state.keyboard.get());
        if (temp_key->m_keyname.length())
          AddInput(temp_key);
        else
          delete temp_key;
      }
    }
  }
}
void Seat::wl_seat_listener_capabilities(void* data, struct wl_seat* wl_seat, uint32_t capabilities)
{
  reinterpret_cast<Seat*>(data)->wl_seat_listener_capabilities(wl_seat, capabilities);
}

void Seat::wl_seat_listener_name(struct wl_seat* wl_seat, const char* new_name)
{
  if (std::strlen(new_name))
    name = new_name;
  else
    name = "Seat";
}
void Seat::wl_seat_listener_name(void* data, struct wl_seat* wl_seat, const char* name)
{
  reinterpret_cast<Seat*>(data)->wl_seat_listener_name(wl_seat, name);
}

const wl_seat_listener Seat::s_seat_listener = {wl_seat_listener_capabilities,
                                                wl_seat_listener_name};

void Seat::ClearKeymap()
{
  if (m_keystate)
  {
    xkb_state_unref(m_keystate);
    m_keystate = nullptr;
  }
  if (m_keymap)
  {
    xkb_keymap_unref(m_keymap);
    m_keymap = nullptr;
  }
}

void Seat::DeleteKeyboard()
{
  if (m_keyboard)
  {
    if (wl_keyboard_get_version(m_keyboard) >= WL_KEYBOARD_RELEASE_SINCE_VERSION)
      wl_keyboard_release(m_keyboard);
    else
      wl_keyboard_destroy(m_keyboard);
    m_keyboard = nullptr;
  }
}

void Seat::DeletePointer()
{
  if (m_pointer)
  {
    if (wl_pointer_get_version(m_pointer) >= WL_POINTER_RELEASE_SINCE_VERSION)
      wl_pointer_release(m_pointer);
    else
      wl_pointer_destroy(m_pointer);
    m_pointer = nullptr;
  }
}

void Seat::DeleteSeat()
{
  if (m_seat)
  {
    if (wl_seat_get_version(m_seat) >= WL_SEAT_RELEASE_SINCE_VERSION)
      wl_seat_release(m_seat);
    else
      wl_seat_destroy(m_seat);
    m_seat = nullptr;
  }
}

void Seat::UpdateInput()
{
  // Round-trip to get input events.
  g_proxy.Roundtrip();

  // Apply axis smoothing.
  m_state.axis.x *= MOUSE_AXIS_SMOOTHING;
  m_state.axis.x += m_state.accum_axis.x;
  m_state.axis.x /= MOUSE_AXIS_SMOOTHING + 1.0f;
  m_state.axis.y *= MOUSE_AXIS_SMOOTHING;
  m_state.axis.y += m_state.accum_axis.y;
  m_state.axis.y /= MOUSE_AXIS_SMOOTHING + 1.0f;

  // Zero out axis for accumulating next round of events.
  m_state.accum_axis = Common::Vec2{};

  // Notify dolphin if seat has been removed by server.
  if (!g_proxy.HasSeatId(m_seat_id))
    m_valid = false;
}

Seat::Seat(uint32_t seat_id, uint32_t seat_version, wl_surface* surface)
    : m_seat_id(seat_id), m_seat(g_proxy.BindSeat(seat_id, seat_version)), m_surface(surface)
{
  m_xkb = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  if (m_seat)
    wl_seat_add_listener(m_seat, &s_seat_listener, this);
  g_proxy.Roundtrip();
  m_constructed = true;
}

Seat::~Seat()
{
  ClearKeymap();
  if (m_xkb)
    xkb_context_unref(m_xkb);
  DeletePointer();
  DeleteKeyboard();
  DeleteSeat();
}

std::string Seat::GetName() const
{
  return name;
}

std::string Seat::GetSource() const
{
  return "Wayland";
}

Seat::Key::Key(xkb_state* state, xkb_keycode_t keycode, const char* keyboard)
    : m_keyboard(keyboard), m_keycode(keycode)
{
  xkb_keysym_t keysym = XKB_KEY_NoSymbol;
  const xkb_keysym_t* syms;
  if (xkb_state_key_get_syms(state, keycode, &syms) > 0)
    keysym = syms[0];

  // Convert to upper case for the keyname
  if (keysym >= 97 && keysym <= 122)
    keysym -= 32;

  // 0x0110ffff is the top of the unicode character range according
  // to keysymdef.h although it is probably more than we need.
  if (keysym == XKB_KEY_NoSymbol || keysym > 0x0110ffff)
    return;

  char keysym_name[64];
  unsigned keysym_name_size = xkb_keysym_get_name(keysym, keysym_name, 64);
  if (keysym_name_size < 64)
    m_keyname = std::string(keysym_name, keysym_name_size);
}

ControlState Seat::Key::GetState() const
{
  return (m_keyboard[m_keycode / 8] & (1 << (m_keycode % 8))) != 0;
}

Seat::Button::Button(uint32_t index, uint32_t* buttons) : m_buttons(buttons), m_index(index)
{
  name = fmt::format("Click {}", m_index + 1);
}

ControlState Seat::Button::GetState() const
{
  return ((*m_buttons & (1 << m_index)) != 0);
}

Seat::Cursor::Cursor(u8 index, bool positive, const float* cursor)
    : m_cursor(cursor), m_index(index), m_positive(positive)
{
  name = fmt::format("Cursor {}{}", static_cast<char>('X' + m_index), (m_positive ? '+' : '-'));
}

ControlState Seat::Cursor::GetState() const
{
  return std::max(0.0f, *m_cursor / (m_positive ? 1.0f : -1.0f));
}

Seat::Axis::Axis(u8 index, bool positive, const float* axis)
    : m_axis(axis), m_index(index), m_positive(positive)
{
  name = fmt::format("Axis {}{}", static_cast<char>('X' + m_index), (m_positive ? '+' : '-'));
}

ControlState Seat::Axis::GetState() const
{
  return std::max(0.0f, *m_axis / (m_positive ? MOUSE_AXIS_SENSITIVITY : -MOUSE_AXIS_SENSITIVITY));
}

// This function will create an independent Wayland display proxy, queue
// and registry to listen for all available and added wl_seat interfaces.
void Init(void* const display)
{
  g_proxy.Setup(display);
}

// This function will add zero or more KeyboardMouse objects to devices.
void PopulateDevices(void* const hwnd)
{
  // Round-trip to get all registry events.
  g_proxy.Roundtrip();

  // Create seat wrappers.
  for (auto& seat : g_proxy.SeatIds())
  {
    g_controller_interface.AddDevice(
        std::make_shared<Seat>(seat.first, seat.second, reinterpret_cast<wl_surface*>(hwnd)));
  }
}

// Tears down all wayland objects, closing display proxy.
void DeInit()
{
  g_proxy.Finish();
}
}  // namespace ciface::Wayland
