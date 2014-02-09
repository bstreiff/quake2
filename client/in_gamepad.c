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
#include "../win32/winquake.h"
#include "SDL.h"

cvar_t	*in_gamepad;

Sint16 s_axis_value[SDL_CONTROLLER_AXIS_MAX] = {0};

// none of these cvars are saved over a session
// this means that advanced controller configuration needs to be executed
// each time.  this avoids any problems with getting back to a default usage
// or when changing from one controller to another.  this way at least something
// works.
cvar_t	*gamepad_name;
cvar_t	*gamepad_advanced;
cvar_t	*gamepad_advaxisx;
cvar_t	*gamepad_advaxisy;
cvar_t	*gamepad_advaxisz;
cvar_t	*gamepad_advaxisr;
cvar_t	*gamepad_advaxisu;
cvar_t	*gamepad_advaxisv;
cvar_t	*gamepad_forwardthreshold;
cvar_t	*gamepad_sidethreshold;
cvar_t	*gamepad_pitchthreshold;
cvar_t	*gamepad_yawthreshold;
cvar_t	*gamepad_forwardsensitivity;
cvar_t	*gamepad_sidesensitivity;
cvar_t	*gamepad_pitchsensitivity;
cvar_t	*gamepad_yawsensitivity;
cvar_t	*gamepad_upthreshold;
cvar_t	*gamepad_upsensitivity;

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
	gamepad_forwardthreshold = Cvar_Get("gamepad_forwardthreshold", "0.15", CVAR_ARCHIVE);
	gamepad_sidethreshold = Cvar_Get("gamepad_sidethreshold", "0.15", CVAR_ARCHIVE);
	gamepad_pitchthreshold = Cvar_Get("gamepad_pitchthreshold", "0.15", CVAR_ARCHIVE);
	gamepad_yawthreshold = Cvar_Get("gamepad_yawthreshold", "0.15", CVAR_ARCHIVE);
	gamepad_forwardsensitivity = Cvar_Get("gamepad_forwardsensitivity", "-1", CVAR_ARCHIVE);
	gamepad_sidesensitivity = Cvar_Get("gamepad_sidesensitivity", "1", CVAR_ARCHIVE);
	gamepad_pitchsensitivity = Cvar_Get("gamepad_pitchsensitivity", "-1", CVAR_ARCHIVE);
	gamepad_yawsensitivity = Cvar_Get("gamepad_yawsensitivity", "-1", CVAR_ARCHIVE);

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
		if (SDL_IsGameController(i))
		{
			controller = SDL_GameControllerOpen(i);
			if (controller)
			{
				controller_index = i;
				Cvar_Set("gamepad_name", (char *)SDL_GameControllerName(controller));
			}
		}
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

/*
===========
IN_Commands
===========
*/
void IN_Commands(void)
{
}


/*
===========
IN_GamepadMove
===========
*/
void IN_GamepadMove(usercmd_t *cmd)
{
	float	speed, aspeed;
	float	fAxisValue;
	int		i;

	// verify gamepad is available and that the user wants to use it
	if (!in_gamepad->value || !controller)
	{
		return;
	}

	if ((in_speed.state & 1) ^ (int)cl_run->value)
		speed = 2;
	else
		speed = 1;
	aspeed = speed * cls.frametime;

	// loop through the axes
	for (i = 0; i < SDL_CONTROLLER_AXIS_MAX; i++)
	{
		// get the floating point zero-centered, potentially-inverted data for the current axis
		fAxisValue = (float)s_axis_value[i];
		// convert range from -32768..32767 to -1..1 
		fAxisValue /= 32768.0;

		// TODO: make it so the user can rebind these?
		if (i == SDL_CONTROLLER_AXIS_LEFTY)
		{
			if (fabs(fAxisValue) > gamepad_forwardthreshold->value)
			{
				cmd->forwardmove += (fAxisValue * gamepad_forwardsensitivity->value) * speed * cl_forwardspeed->value;
			}
		}
		else if (i == SDL_CONTROLLER_AXIS_LEFTX)
		{
			if (fabs(fAxisValue) > gamepad_sidethreshold->value)
			{
				cmd->sidemove += (fAxisValue * gamepad_sidesensitivity->value) * speed * cl_sidespeed->value;
			}
		}
		else if (i == SDL_CONTROLLER_AXIS_RIGHTY)
		{
			if (fabs(fAxisValue) > gamepad_pitchthreshold->value)
			{
				cl.viewangles[PITCH] += (fAxisValue * gamepad_pitchsensitivity->value) * aspeed * cl_pitchspeed->value;
			}
		}
		else if (i == SDL_CONTROLLER_AXIS_RIGHTX)
		{
			if (fabs(fAxisValue) > gamepad_yawthreshold->value)
			{
				cl.viewangles[YAW] += (fAxisValue * gamepad_yawsensitivity->value) * aspeed * cl_yawspeed->value;
			}
		}
	}
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
		s_axis_value[axisev->axis] = axisev->value;
	}
}

// 1:1 mapping between SDL buttons and Quake buttons
static const keysym_t controller_button[] =
{
	K_GAMEPAD_A,		// SDL_CONTROLLER_BUTTON_A
	K_GAMEPAD_B,		// SDL_CONTROLLER_BUTTON_B
	K_GAMEPAD_X,		// SDL_CONTROLLER_BUTTON_X
	K_GAMEPAD_Y,		// SDL_CONTROLLER_BUTTON_Y
	K_GAMEPAD_BACK,		// SDL_CONTROLLER_BUTTON_BACK
	K_GAMEPAD_GUIDE,	// SDL_CONTROLLER_BUTTON_GUIDE
	K_GAMEPAD_START,	// SDL_CONTROLLER_BUTTON_START
	K_GAMEPAD_LSTICK,	// SDL_CONTROLLER_BUTTON_LEFTSTICK
	K_GAMEPAD_RSTICK,	// SDL_CONTROLLER_BUTTON_RIGHTSTICK
	K_GAMEPAD_L1,		// SDL_CONTROLLER_BUTTON_LEFTSHOULDER
	K_GAMEPAD_R1,		// SDL_CONTROLLER_BUTTON_RIGHTSHOULDER
	K_GAMEPAD_DPADUP,	// SDL_CONTROLLER_BUTTON_DPAD_UP
	K_GAMEPAD_DPADDOWN,	// SDL_CONTROLLER_BUTTON_DPAD_DOWN
	K_GAMEPAD_DPADLEFT,	// SDL_CONTROLLER_BUTTON_DPAD_LEFT
	K_GAMEPAD_DPADRIGHT	// SDL_CONTROLLER_BUTTON_DPAD_RIGHT
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
	// TODO: watch for controllers coming and going
}
