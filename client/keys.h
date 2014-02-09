/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

typedef enum
{
	K_UNHANDLED = 0,

	K_TAB = 9,
	K_ENTER = 13,
	K_ESCAPE = 27,
	K_SPACE = 32,

	// normal keys should be passed as lowercased ascii

	K_BACKSPACE = 127,
	K_UPARROW,
	K_DOWNARROW,
	K_LEFTARROW,
	K_RIGHTARROW,

	K_ALT,
	K_CTRL,
	K_SHIFT,
	K_F1,
	K_F2,
	K_F3,
	K_F4,
	K_F5,
	K_F6,
	K_F7,
	K_F8,
	K_F9,
	K_F10,
	K_F11,
	K_F12,
	K_INS,
	K_DEL,
	K_PGDN,
	K_PGUP,
	K_HOME,
	K_END,

	K_KP_HOME,
	K_KP_UPARROW,
	K_KP_PGUP,
	K_KP_LEFTARROW,
	K_KP_5,
	K_KP_RIGHTARROW,
	K_KP_END,
	K_KP_DOWNARROW,
	K_KP_PGDN,
	K_KP_ENTER,
	K_KP_INS,
	K_KP_DEL,
	K_KP_SLASH,
	K_KP_TIMES,
	K_KP_MINUS,
	K_KP_PLUS,
	K_PAUSE,

	//
	// mouse buttons generate virtual keys
	//
	K_MOUSE1 = 200,
	K_MOUSE2,
	K_MOUSE3,
	K_MOUSE4,
	K_MOUSE5,
	K_MWHEELDOWN,
	K_MWHEELUP,
	K_MWHEELLEFT,
	K_MWHEELRIGHT,

	//
	// gamepad buttons
	//
	K_GAMEPAD_A,
	K_GAMEPAD_B,
	K_GAMEPAD_X,
	K_GAMEPAD_Y,
	K_GAMEPAD_BACK,
	K_GAMEPAD_GUIDE,
	K_GAMEPAD_START,
	K_GAMEPAD_LSTICK,
	K_GAMEPAD_RSTICK,
	K_GAMEPAD_L1,
	K_GAMEPAD_R1,
	K_GAMEPAD_DPADUP,
	K_GAMEPAD_DPADDOWN,
	K_GAMEPAD_DPADLEFT,
	K_GAMEPAD_DPADRIGHT,

	K_LAST_SYM
} keysym_t;

extern char		*keybindings[256];
extern	int		key_repeats[256];

extern	int	anykeydown;
extern char chat_buffer[];
extern	int chat_bufferlen;
extern	qboolean	chat_team;
extern	qboolean	chat_console;

void Key_Event (int key, qboolean down, unsigned time);
void Key_Init (void);
void Key_WriteBindings (FILE *f);
void Key_SetBinding (int keynum, char *binding);
void Key_ClearStates (void);
int Key_GetKey (void);

struct SDL_KeyboardEvent;
void Key_HandleKeyboardEvent(const struct SDL_KeyboardEvent* key);
