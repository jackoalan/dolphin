// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include "Common/Matrix.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace ciface::Wayland
{
void Init(void* const display);
void PopulateDevices(void* const hwnd);
void DeInit();

class Seat final : public Core::Device
{
  struct State
  {
    std::unique_ptr<char[]> keyboard;
    uint32_t buttons;
    Common::Vec2 cursor;
    Common::Vec2 accum_axis;
    Common::Vec2 axis;
  };

  class Key : public Input
  {
    friend class Seat;

  public:
    std::string GetName() const override { return m_keyname; }
    Key(xkb_state* state, xkb_keycode_t keycode, const char* keyboard);
    ControlState GetState() const override;

  private:
    std::string m_keyname;
    const char* const m_keyboard;
    const xkb_keycode_t m_keycode;
  };

  class Button : public Input
  {
  public:
    std::string GetName() const override { return name; }
    Button(uint32_t index, uint32_t* buttons);
    ControlState GetState() const override;

  private:
    const uint32_t* m_buttons;
    const uint32_t m_index;
    std::string name;
  };

  class Cursor : public Input
  {
  public:
    std::string GetName() const override { return name; }
    bool IsDetectable() const override { return false; }
    Cursor(u8 index, bool positive, const float* cursor);
    ControlState GetState() const override;

  private:
    const float* m_cursor;
    const u8 m_index;
    const bool m_positive;
    std::string name;
  };

  class Axis : public Input
  {
  public:
    std::string GetName() const override { return name; }
    bool IsDetectable() const override { return false; }
    Axis(u8 index, bool positive, const float* axis);
    ControlState GetState() const override;

  private:
    const float* m_axis;
    const u8 m_index;
    const bool m_positive;
    std::string name;
  };

private:
  static const wl_pointer_listener s_pointer_listener;

  void wl_pointer_listener_enter(struct wl_pointer* wl_pointer, uint32_t serial,
                                 struct wl_surface* surface, wl_fixed_t surface_x,
                                 wl_fixed_t surface_y);
  static void wl_pointer_listener_enter(void* data, struct wl_pointer* wl_pointer, uint32_t serial,
                                        struct wl_surface* surface, wl_fixed_t surface_x,
                                        wl_fixed_t surface_y);

  void wl_pointer_listener_leave(struct wl_pointer* wl_pointer, uint32_t serial,
                                 struct wl_surface* surface);
  static void wl_pointer_listener_leave(void* data, struct wl_pointer* wl_pointer, uint32_t serial,
                                        struct wl_surface* surface);

  void wl_pointer_listener_motion(struct wl_pointer* wl_pointer, uint32_t time,
                                  wl_fixed_t surface_x, wl_fixed_t surface_y);
  static void wl_pointer_listener_motion(void* data, struct wl_pointer* wl_pointer, uint32_t time,
                                         wl_fixed_t surface_x, wl_fixed_t surface_y);

  void wl_pointer_listener_button(struct wl_pointer* wl_pointer, uint32_t serial, uint32_t time,
                                  uint32_t button, uint32_t state);
  static void wl_pointer_listener_button(void* data, struct wl_pointer* wl_pointer, uint32_t serial,
                                         uint32_t time, uint32_t button, uint32_t state);

  void wl_pointer_listener_axis(struct wl_pointer* wl_pointer, uint32_t time, uint32_t axis,
                                wl_fixed_t value);
  static void wl_pointer_listener_axis(void* data, struct wl_pointer* wl_pointer, uint32_t time,
                                       uint32_t axis, wl_fixed_t value);

  void wl_pointer_listener_frame(struct wl_pointer* wl_pointer);
  static void wl_pointer_listener_frame(void* data, struct wl_pointer* wl_pointer);

  void wl_pointer_listener_axis_source(struct wl_pointer* wl_pointer, uint32_t axis_source);
  static void wl_pointer_listener_axis_source(void* data, struct wl_pointer* wl_pointer,
                                              uint32_t axis_source);

  void wl_pointer_listener_axis_stop(struct wl_pointer* wl_pointer, uint32_t time, uint32_t axis);
  static void wl_pointer_listener_axis_stop(void* data, struct wl_pointer* wl_pointer,
                                            uint32_t time, uint32_t axis);

  void wl_pointer_listener_axis_discrete(struct wl_pointer* wl_pointer, uint32_t axis,
                                         int32_t discrete);
  static void wl_pointer_listener_axis_discrete(void* data, struct wl_pointer* wl_pointer,
                                                uint32_t axis, int32_t discrete);

  static const wl_keyboard_listener s_keyboard_listener;

  void wl_keyboard_listener_keymap(struct wl_keyboard* wl_keyboard, uint32_t format, int32_t fd,
                                   uint32_t size);
  static void wl_keyboard_listener_keymap(void* data, struct wl_keyboard* wl_keyboard,
                                          uint32_t format, int32_t fd, uint32_t size);

  void wl_keyboard_listener_enter(struct wl_keyboard* wl_keyboard, uint32_t serial,
                                  struct wl_surface* surface, struct wl_array* keys);
  static void wl_keyboard_listener_enter(void* data, struct wl_keyboard* wl_keyboard,
                                         uint32_t serial, struct wl_surface* surface,
                                         struct wl_array* keys);

  void wl_keyboard_listener_leave(struct wl_keyboard* wl_keyboard, uint32_t serial,
                                  struct wl_surface* surface);
  static void wl_keyboard_listener_leave(void* data, struct wl_keyboard* wl_keyboard,
                                         uint32_t serial, struct wl_surface* surface);

  void wl_keyboard_listener_key(struct wl_keyboard* wl_keyboard, uint32_t serial, uint32_t time,
                                uint32_t key, uint32_t state);
  static void wl_keyboard_listener_key(void* data, struct wl_keyboard* wl_keyboard, uint32_t serial,
                                       uint32_t time, uint32_t key, uint32_t state);

  void wl_keyboard_listener_modifiers(struct wl_keyboard* wl_keyboard, uint32_t serial,
                                      uint32_t mods_depressed, uint32_t mods_latched,
                                      uint32_t mods_locked, uint32_t group);
  static void wl_keyboard_listener_modifiers(void* data, struct wl_keyboard* wl_keyboard,
                                             uint32_t serial, uint32_t mods_depressed,
                                             uint32_t mods_latched, uint32_t mods_locked,
                                             uint32_t group);

  void wl_keyboard_listener_repeat_info(struct wl_keyboard* wl_keyboard, int32_t rate,
                                        int32_t delay);
  static void wl_keyboard_listener_repeat_info(void* data, struct wl_keyboard* wl_keyboard,
                                               int32_t rate, int32_t delay);

  static const wl_seat_listener s_seat_listener;

  void wl_seat_listener_capabilities(struct wl_seat* wl_seat, uint32_t capabilities);
  static void wl_seat_listener_capabilities(void* data, struct wl_seat* wl_seat,
                                            uint32_t capabilities);

  void wl_seat_listener_name(struct wl_seat* wl_seat, const char* new_name);
  static void wl_seat_listener_name(void* data, struct wl_seat* wl_seat, const char* name);

  void ClearKeymap();

  void DeleteKeyboard();
  void DeletePointer();
  void DeleteSeat();

public:
  void UpdateInput() override;
  bool IsValid() const override { return m_valid; }

  explicit Seat(uint32_t seat_id, uint32_t seat_version, wl_surface* surface);
  ~Seat() override;

  std::string GetName() const override;
  std::string GetSource() const override;

private:
  u32 m_seat_id;
  wl_seat* m_seat;
  wl_surface* m_surface;
  wl_pointer* m_pointer = nullptr;
  wl_keyboard* m_keyboard = nullptr;
  xkb_context* m_xkb = nullptr;
  xkb_keymap* m_keymap = nullptr;
  xkb_state* m_keystate = nullptr;
  State m_state{};
  std::string name = "Seat";
  bool m_in_surface = false;
  bool m_constructed = false;
  bool m_valid = true;
};
}  // namespace ciface::Wayland
