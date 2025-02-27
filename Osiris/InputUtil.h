#pragma once
#include <string>

enum KeyMode {
	Off,
	Always,
	Hold,
	Toggle
};

class KeyBind {
public:
	enum KeyCode {
		APOSTROPHE = 0,
		COMMA,
		MINUS,
		PERIOD,
		SLASH,
		KEY_0,
		KEY_1,
		KEY_2,
		KEY_3,
		KEY_4,
		KEY_5,
		KEY_6,
		KEY_7,
		KEY_8,
		KEY_9,
		SEMICOLON,
		EQUALS,
		A,
		ADD,
		B,
		BACKSPACE,
		C,
		CAPSLOCK,
		D,
		DECIMAL,
		DEL,
		DIVIDE,
		DOWN,
		E,
		END,
		ENTER,
		F,
		F1,
		F10,
		F11,
		F12,
		F2,
		F3,
		F4,
		F5,
		F6,
		F7,
		F8,
		F9,
		G,
		H,
		HOME,
		I,
		INSERT,
		J,
		K,
		L,
		LALT,
		LCTRL,
		LEFT,
		LSHIFT,
		M,
		MOUSE1,
		MOUSE2,
		MOUSE3,
		MOUSE4,
		MOUSE5,
		MULTIPLY,
		MOUSEWHEEL_DOWN,
		MOUSEWHEEL_UP,
		N,
		NONE,
		NUMPAD_0,
		NUMPAD_1,
		NUMPAD_2,
		NUMPAD_3,
		NUMPAD_4,
		NUMPAD_5,
		NUMPAD_6,
		NUMPAD_7,
		NUMPAD_8,
		NUMPAD_9,
		O,
		P,
		PAGE_DOWN,
		PAGE_UP,
		Q,
		R,
		RALT,
		RCTRL,
		RIGHT,
		RSHIFT,
		S,
		SPACE,
		SUBTRACT,
		T,
		TAB,
		U,
		UP,
		V,
		W,
		X,
		Y,
		Z,
		LEFTBRACKET,
		BACKSLASH,
		RIGHTBRACKET,
		BACKTICK,

		MAX
	};

	KeyBind(KeyCode keyCode) noexcept;
	KeyBind(const char* keyName) noexcept;
	KeyBind(const std::string name, KeyMode keyMode = Always) noexcept;

	bool operator==(KeyCode keyCode) const noexcept { return this->keyCode == keyCode; }
	bool operator==(const KeyBind& other) const noexcept { return this->keyCode == other.keyCode && this->keyMode == other.keyMode; }

	const char* toString() const noexcept;
	bool isPressed() const noexcept;
	bool isDown() const noexcept;
	bool isSet() const noexcept { return keyCode != NONE; }

	bool setToPressedKey() noexcept;

	void handleToggle() noexcept;
	bool isToggled() const noexcept { return toggledOn; }
	void setToggleTo(bool value) noexcept { toggledOn = value; }

	bool isActive() const noexcept;
	bool canShowKeybind() noexcept;
	void showKeybind() noexcept;

	void reset() noexcept { keyCode = NONE; }

	KeyMode keyMode = Always;
	std::string activeName = { };
private:
	KeyCode keyCode;
	bool toggledOn = true;
};