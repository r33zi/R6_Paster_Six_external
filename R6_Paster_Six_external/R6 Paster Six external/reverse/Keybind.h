#include <WinUser.h>
#include <string>
#include <string.h>
#include <processthreadsapi.h>
#include "Imgui/imgui.h"

#define IM_ARRAYSIZE(_ARR)          ((int)(sizeof(_ARR)/sizeof(*_ARR)))

namespace hotkeys
{
	int aimkey;
	int aimkeyy;
	int backkey;
	int aimkeyyy;
	int aimkeyyyy;
	int airstuckey;
}

static int keystatus = 0;
static int keystatuss = 0;
static int keystatusssss = 0;
static int keystatusss = 0;
static int keystatussss = 0;

static const char* keyNames[] =
{
	"",
	"Left Mouse",
	"Right Mouse",
	"Cancel",
	"Middle Mouse",
	"Mouse 5",
	"Mouse 4",
	"",
	"Backspace",
	"Tab",
	"",
	"",
	"Clear",
	"Enter",
	"",
	"",
	"Shift",
	"Control",
	"Alt",
	"Pause",
	"Caps",
	"",
	"",
	"",
	"",
	"",
	"",
	"Escape",
	"",
	"",
	"",
	"",
	"Space",
	"Page Up",
	"Page Down",
	"End",
	"Home",
	"Left",
	"Up",
	"Right",
	"Down",
	"",
	"",
	"",
	"Print",
	"Insert",
	"Delete",
	"",
	"0",
	"1",
	"2",
	"3",
	"4",
	"5",
	"6",
	"7",
	"8",
	"9",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"A",
	"B",
	"C",
	"D",
	"E",
	"F",
	"G",
	"H",
	"I",
	"J",
	"K",
	"L",
	"M",
	"N",
	"O",
	"P",
	"Q",
	"R",
	"S",
	"T",
	"U",
	"V",
	"W",
	"X",
	"Y",
	"Z",
	"",
	"",
	"",
	"",
	"",
	"Numpad 0",
	"Numpad 1",
	"Numpad 2",
	"Numpad 3",
	"Numpad 4",
	"Numpad 5",
	"Numpad 6",
	"Numpad 7",
	"Numpad 8",
	"Numpad 9",
	"Multiply",
	"Add",
	"",
	"Subtract",
	"Decimal",
	"Divide",
	"F1",
	"F2",
	"F3",
	"F4",
	"F5",
	"F6",
	"F7",
	"F8",
	"F9",
	"F10",
	"F11",
	"F12",
};

static bool Items_ArrayGetter(void* data, int idx, const char** out_text)
{
	const char* const* items = (const char* const*)data;
	if (out_text)
		*out_text = items[idx];
	return true;
}

void ChangeKey(void* blank)
{
	keystatus = 1;
	while (true)
	{
		for (int i = 0; i < 0x87; i++)
		{
			if (GetKeyState(i) & 0x8000)
			{
				hotkeys::aimkey = i;
				keystatus = 0;
				return;
			}
		}
	}
}
void ChangeKeyy(void* blank)
{
	keystatuss = 1;
	while (true)
	{
		for (int i = 0; i < 0x87; i++)
		{
			if (GetKeyState(i) & 0x8000)
			{
				hotkeys::aimkeyy = i;
				keystatuss = 0;
				return;
			}
		}
	}
}
void ChangeKeyyy(void* blank)
{
	keystatusss = 1;
	while (true)
	{
		for (int i = 0; i < 0x87; i++)
		{
			if (GetKeyState(i) & 0x8000)
			{
				hotkeys::aimkeyyy = i;
				keystatusss = 0;
				return;
			}
		}
	}
}
void ChangeKeyyyy(void* blank)
{
	keystatussss = 1;
	while (true)
	{
		for (int i = 0; i < 0x87; i++)
		{
			if (GetKeyState(i) & 0x8000)
			{
				hotkeys::aimkeyyyy = i;
				keystatussss = 0;
				return;
			}
		}
	}
}
void ChangeKeyyyyy(void* blank)
{
	keystatusssss = 1;
	while (true)
	{
		for (int i = 0; i < 0x87; i++)
		{
			if (GetKeyState(i) & 0x8000)
			{
				hotkeys::backkey = i;
				keystatusssss = 0;
				return;
			}
		}
	}
}

void HotkeyButton(int key, void* changeFunc, int status)
{
	const char* preview_value = NULL;
	if (key >= 0 && key < IM_ARRAYSIZE(keyNames))
		Items_ArrayGetter(keyNames, key, &preview_value);

	std::string label;
	if (status == 1)
		label = "Press the key";
	else if (preview_value == NULL)
		label = "Select Key";
	else
		label = preview_value;

	if (ImGui::Button(label.c_str(), ImVec2(125, 20)))
	{
		if (status == 0)
		{
			CreateThread(0, 0, (LPTHREAD_START_ROUTINE)changeFunc, nullptr, 0, nullptr);
		}
	}
}
