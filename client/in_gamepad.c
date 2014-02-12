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

#include "../client/client.h"
#include "SDL.h"

cvar_t	*in_gamepad;

static Sint16 s_axis_value[SDL_CONTROLLER_AXIS_MAX] = {0};

// none of these cvars are saved over a session
// this means that advanced controller configuration needs to be executed
// each time.  this avoids any problems with getting back to a default usage
// or when changing from one controller to another.  this way at least something
// works.
static cvar_t	*gamepad_name;

static cvar_t	*gamepad_threshold[SDL_CONTROLLER_AXIS_MAX];
static cvar_t	*gamepad_sensitivity[SDL_CONTROLLER_AXIS_MAX];

static const struct {
	const char* threshold_cvar_name;
	const char* sensitivity_cvar_name;
	keysym_t	neg_keysym;
	keysym_t	pos_keysym;
} axis_info[SDL_CONTROLLER_AXIS_MAX] = {
	{ "gamepad_threshold_lx", "gamepad_sensitivity_lx", K_GAMEPAD_LSTICKLEFT,  K_GAMEPAD_LSTICKRIGHT },
	{ "gamepad_threshold_ly", "gamepad_sensitivity_ly", K_GAMEPAD_LSTICKUP,    K_GAMEPAD_LSTICKDOWN },
	{ "gamepad_threshold_rx", "gamepad_sensitivity_rx", K_GAMEPAD_RSTICKLEFT,  K_GAMEPAD_RSTICKRIGHT },
	{ "gamepad_threshold_ry", "gamepad_sensitivity_ry", K_GAMEPAD_RSTICKUP,    K_GAMEPAD_RSTICKDOWN },
	{ "gamepad_threshold_lt", "gamepad_sensitivity_lt", K_UNHANDLED,           K_GAMEPAD_LTRIGGER },
	{ "gamepad_threshold_rt", "gamepad_sensitivity_rt", K_UNHANDLED,           K_GAMEPAD_RTRIGGER },
};

extern qboolean	in_appactive;

// forward-referenced functions
void IN_StartupGamepad(void);
void IN_GamepadMove(usercmd_t *cmd);

/*
=========================================================================

GAMEPAD

=========================================================================
*/

static SDL_GameController* controller = NULL;
static int controller_index = -1;

qboolean IN_ConnectToController(int index)
{
	if (SDL_IsGameController(index))
	{
		controller = SDL_GameControllerOpen(index);
		if (controller)
		{
			controller_index = index;
			Cvar_Set("gamepad_name", (char *)SDL_GameControllerName(controller));
			return true;
		}
	}
	return false;
}

/*
===============
IN_StartupGamepad
===============
*/
void IN_StartupGamepad(void)
{
	// gamepad variables
	in_gamepad = Cvar_Get("in_gamepad", "0", CVAR_ARCHIVE);
	gamepad_name = Cvar_Get("gamepad_name", "", 0);

	for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; ++i)
	{
		gamepad_threshold[i] = Cvar_Get(axis_info[i].threshold_cvar_name, "0.17", CVAR_ARCHIVE);
		gamepad_sensitivity[i] = Cvar_Get(axis_info[i].sensitivity_cvar_name, "1.0", CVAR_ARCHIVE);
	}

	// abort startup if user requests no gamepad
	if (!in_gamepad->value)
		return;

	if (SDL_NumJoysticks() == 0)
	{
		Com_Printf("No game controllers available.\n");
		return;
	}

	// Open the first valid game controller.
	for (int i = 0; i < SDL_NumJoysticks() && controller == NULL; ++i)
	{
		if (IN_ConnectToController(i))
			break;
	}

	// abort startup if we didn't find a valid gamepad
	if (!controller)
	{
		Com_Printf("No valid game controllers.\n");
		return;
	}

	Com_Printf("Found game controller: %s\n", SDL_GameControllerName(controller));

	SDL_GameControllerEventState(SDL_ENABLE);
}

void IN_ShutdownGamepad(void)
{
	if (controller)
	{
		SDL_GameControllerClose(controller);
		controller = NULL;
		controller_index = -1;
	}
}

void IN_ActivateGamepad(void)
{
	SDL_GameControllerEventState(SDL_ENABLE);
}

void IN_DeactivateGamepad(void)
{
	SDL_GameControllerEventState(SDL_IGNORE);
}

float IN_GetGamepadAxisDistance(keysym_t axiskey)
{
	if (axiskey == 0)
		return 0.0;

	for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; ++i)
	{
		float fAxisValue = (float)s_axis_value[i];
		// convert range from -32768..32767 to -1..1 
		fAxisValue /= 32768.0;
		fAxisValue *= gamepad_sensitivity[i]->value;

		if (axiskey == axis_info[i].neg_keysym)
		{
			return (fAxisValue < 0 ? -fAxisValue : 0.0);
		}
		else if (axiskey == axis_info[i].pos_keysym)
		{
			return (fAxisValue > 0 ? fAxisValue : 0.0);
		}
	}

	return 0.0;
}

/*
===========
IN_GamepadMove
===========
*/
void IN_GamepadMove(usercmd_t *cmd)
{
	// No-op. This used to handle control for the axes, but those
	// are synthesized as key events now.
}

void IN_HandleControllerAxisEvent(const SDL_ControllerAxisEvent* axisev)
{
	if (!controller)
		return;

	// We only support one controller at a time.
	if (axisev->which != controller_index)
		return;

	if (axisev->axis < SDL_CONTROLLER_AXIS_MAX)
	{
		float fAxisValue;

		s_axis_value[axisev->axis] = axisev->value;

		fAxisValue = axisev->value;
		// convert range from -32768..32767 to -1..1 
		fAxisValue /= 32768.0;

		if (fAxisValue < 0) // left or up
		{
			if (axis_info[axisev->axis].pos_keysym)
			{
				Key_Event(
					axis_info[axisev->axis].pos_keysym,
					false,
					axisev->timestamp);
			}
			if (axis_info[axisev->axis].neg_keysym)
			{
				Key_Event(
					axis_info[axisev->axis].neg_keysym,
					(fabs(fAxisValue) > gamepad_threshold[axisev->axis]->value),
					axisev->timestamp);
			}
		}
		else if (fAxisValue > 0) // right or down
		{
			if (axis_info[axisev->axis].neg_keysym)
			{
				Key_Event(
					axis_info[axisev->axis].neg_keysym,
					false,
					axisev->timestamp);
			}
			if (axis_info[axisev->axis].pos_keysym)
			{
				Key_Event(
					axis_info[axisev->axis].pos_keysym,
					(fabs(fAxisValue) > gamepad_threshold[axisev->axis]->value),
					axisev->timestamp);
			}
		}
		else if (fAxisValue == 0) // dead center (or this 'axis' is actually a binary on/off control)
		{
			if (axis_info[axisev->axis].neg_keysym)
			{
				Key_Event(
					axis_info[axisev->axis].neg_keysym,
					false,
					axisev->timestamp);
			}
			if (axis_info[axisev->axis].pos_keysym)
			{
				Key_Event(
					axis_info[axisev->axis].pos_keysym,
					false,
					axisev->timestamp);
			}
		}
	}
}

// 1:1 mapping between SDL buttons and Quake buttons
static const keysym_t controller_button[] =
{
	K_GAMEPAD_A,			// SDL_CONTROLLER_BUTTON_A
	K_GAMEPAD_B,			// SDL_CONTROLLER_BUTTON_B
	K_GAMEPAD_X,			// SDL_CONTROLLER_BUTTON_X
	K_GAMEPAD_Y,			// SDL_CONTROLLER_BUTTON_Y
	K_GAMEPAD_BACK,			// SDL_CONTROLLER_BUTTON_BACK
	K_GAMEPAD_GUIDE,		// SDL_CONTROLLER_BUTTON_GUIDE
	K_GAMEPAD_START,		// SDL_CONTROLLER_BUTTON_START
	K_GAMEPAD_LSTICK,		// SDL_CONTROLLER_BUTTON_LEFTSTICK
	K_GAMEPAD_RSTICK,		// SDL_CONTROLLER_BUTTON_RIGHTSTICK
	K_GAMEPAD_LSHOULDER,	// SDL_CONTROLLER_BUTTON_LEFTSHOULDER
	K_GAMEPAD_RSHOULDER,	// SDL_CONTROLLER_BUTTON_RIGHTSHOULDER
	K_GAMEPAD_DPADUP,		// SDL_CONTROLLER_BUTTON_DPAD_UP
	K_GAMEPAD_DPADDOWN,		// SDL_CONTROLLER_BUTTON_DPAD_DOWN
	K_GAMEPAD_DPADLEFT,		// SDL_CONTROLLER_BUTTON_DPAD_LEFT
	K_GAMEPAD_DPADRIGHT		// SDL_CONTROLLER_BUTTON_DPAD_RIGHT
};
static const size_t controller_button_length = sizeof(controller_button) / sizeof(controller_button[0]);

void IN_HandleControllerButtonEvent(const SDL_ControllerButtonEvent* buttonev)
{
	const qboolean is_pressed = (buttonev->state == SDL_PRESSED);

	if (!controller)
		return;

	// We only support one controller at a time.
	if (buttonev->which != controller_index)
		return;

	if (buttonev->button < controller_button_length)
	{
		Key_Event(
			controller_button[buttonev->button],
			is_pressed,
			buttonev->timestamp);
	}
}

void IN_HandleControllerDeviceEvent(const SDL_ControllerDeviceEvent* deviceev)
{
	switch (deviceev->type)
	{
		case SDL_CONTROLLERDEVICEADDED:
			if (controller_index == -1)
			{
				// We don't have one already, try connecting to this one.
				IN_ConnectToController(deviceev->which);
			}
			break;
		case SDL_CONTROLLERDEVICEREMOVED:
			if (deviceev->which == controller_index)
			{
				// We were using that! Clean up.
				IN_ShutdownGamepad();
			}
			break;
	}
}
