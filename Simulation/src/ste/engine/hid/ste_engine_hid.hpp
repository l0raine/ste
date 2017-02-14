//	StE
// � Shlomi Steinberg 2015-2016

#pragma once

#include <stdafx.hpp>
#include <signal.hpp>
#include <pointer.hpp>

namespace StE {

class ste_engine_hid {
public:
	using hid_pointer_button_signal_type = signal<HID::pointer::B, HID::Status, HID::ModifierBits>;
	using hid_pointer_movement_signal_type = signal<glm::dvec2>;
	using hid_scroll_signal_type = signal<glm::dvec2>;
	using hid_keyboard_signal_type = signal<HID::keyboard::K, int, HID::Status, HID::ModifierBits>;

private:
	hid_pointer_movement_signal_type hid_pointer_movement_signal;
	hid_pointer_button_signal_type hid_pointer_button_signal;
	hid_scroll_signal_type hid_scroll_signal;
	hid_keyboard_signal_type hid_keyboard_signal;

public:
	ste_engine_hid(GLFWwindow* winptr);

	const auto &signal_pointer_movement() const { return hid_pointer_movement_signal; }
	const auto &signal_pointer_button() const { return hid_pointer_button_signal; }
	const auto &signal_scroll() const { return hid_scroll_signal; }
	const auto &signal_keyboard() const { return hid_keyboard_signal; }
};

}
