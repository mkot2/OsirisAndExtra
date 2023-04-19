#pragma once

#include "UserCmd.h"
#include "Pad.h"
#include "Vector.h"

class Input {
public:
	PAD(12)
		bool isTrackIRAvailable;
	bool isMouseInitialized;
	bool isMouseActive;
	PAD(154)
		bool isCameraInThirdPerson;
	PAD(2)
		Vector cameraOffset;
	PAD(56)
		UserCmd* commands;
	VerifiedUserCmd* verifiedCommands;

	VIRTUAL_METHOD(UserCmd*, getUserCmd, 8, (int slot, int sequenceNumber), (this, slot, sequenceNumber))

		VerifiedUserCmd* getVerifiedUserCmd(int sequenceNumber) noexcept
	{
		return &verifiedCommands[sequenceNumber % 150];
	}
};

static const char* ButtonCodes[] =
{
	xorstr_("NONE"),
	xorstr_("0"),
	xorstr_("1"),
	xorstr_("2"),
	xorstr_("3"),
	xorstr_("4"),
	xorstr_("5"),
	xorstr_("6"),
	xorstr_("7"),
	xorstr_("8"),
	xorstr_("9"),
	xorstr_("A"),
	xorstr_("B"),
	xorstr_("C"),
	xorstr_("D"),
	xorstr_("E"),
	xorstr_("F"),
	xorstr_("G"),
	xorstr_("H"),
	xorstr_("I"),
	xorstr_("J"),
	xorstr_("K"),
	xorstr_("L"),
	xorstr_("M"),
	xorstr_("N"),
	xorstr_("O"),
	xorstr_("P"),
	xorstr_("Q"),
	xorstr_("R"),
	xorstr_("S"),
	xorstr_("T"),
	xorstr_("U"),
	xorstr_("V"),
	xorstr_("W"),
	xorstr_("X"),
	xorstr_("Y"),
	xorstr_("Z"),
	xorstr_("PAD_0"),
	xorstr_("PAD_1"),
	xorstr_("PAD_2"),
	xorstr_("PAD_3"),
	xorstr_("PAD_4"),
	xorstr_("PAD_5"),
	xorstr_("PAD_6"),
	xorstr_("PAD_7"),
	xorstr_("PAD_8"),
	xorstr_("PAD_9"),
	xorstr_("PAD_DIVIDE"),
	xorstr_("PAD_MULTIPLY"),
	xorstr_("PAD_MINUS"),
	xorstr_("PAD_PLUS"),
	xorstr_("PAD_ENTER"),
	xorstr_("PAD_DECIMAL"),
	xorstr_("LBRACKET"),
	xorstr_("RBRACKET"),
	xorstr_("SEMICOLON"),
	xorstr_("APOSTROPHE"),
	xorstr_("BACKQUOTE"),
	xorstr_("COMMA"),
	xorstr_("PERIOD"),
	xorstr_("SLASH"),
	xorstr_("BACKSLASH"),
	xorstr_("MINUS"),
	xorstr_("EQUAL"),
	xorstr_("ENTER"),
	xorstr_("SPACE"),
	xorstr_("BACKSPACE"),
	xorstr_("TAB"),
	xorstr_("CAPSLOCK"),
	xorstr_("NUMLOCK"),
	xorstr_("ESCAPE"),
	xorstr_("SCROLLLOCK"),
	xorstr_("INSERT"),
	xorstr_("DELETE"),
	xorstr_("HOME"),
	xorstr_("END"),
	xorstr_("PAGEUP"),
	xorstr_("PAGEDOWN"),
	xorstr_("BREAK"),
	xorstr_("LSHIFT"),
	xorstr_("RSHIFT"),
	xorstr_("LALT"),
	xorstr_("RALT"),
	xorstr_("LCONTROL"),
	xorstr_("RCONTROL"),
	xorstr_("LWIN"),
	xorstr_("RWIN"),
	xorstr_("APP"),
	xorstr_("UP"),
	xorstr_("LEFT"),
	xorstr_("DOWN"),
	xorstr_("RIGHT"),
	xorstr_("F1"),
	xorstr_("F2"),
	xorstr_("F3"),
	xorstr_("F4"),
	xorstr_("F5"),
	xorstr_("F6"),
	xorstr_("F7"),
	xorstr_("F8"),
	xorstr_("F9"),
	xorstr_("F10"),
	xorstr_("F11"),
	xorstr_("F12"),
	xorstr_("CAPSLOCKTOGGLE"),
	xorstr_("NUMLOCKTOGGLE"),
	xorstr_("SCROLLLOCKTOGGLE"),
};
