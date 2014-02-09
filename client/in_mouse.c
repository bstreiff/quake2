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

extern qboolean	in_appactive;
extern cvar_t	*in_mouse;

/*
============================================================

MOUSE CONTROL

============================================================
*/

// mouse variables
qboolean	mlooking;

void IN_MLookDown(void) { mlooking = true; }
void IN_MLookUp(void) {
	mlooking = false;
	if (!freelook->value && lookspring->value)
		IN_CenterView();
}

int			mouse_buttons;
int			mouse_oldbuttonstate;

static int	mouse_rel_x;
static int	mouse_rel_y;

qboolean	mouseactive;	// false when not focus app

qboolean	restore_spi;
qboolean	mouseinitialized;

/*
===========
IN_ActivateMouse

Called when the window gains focus or changes in some way
===========
*/
void IN_ActivateMouse(void)
{
	if (!mouseinitialized)
		return;
	if (!in_mouse->value)
	{
		mouseactive = false;
		return;
	}
	if (mouseactive)
		return;

	mouseactive = true;

	mouse_rel_x = 0;
	mouse_rel_y = 0;

	SDL_SetRelativeMouseMode(true);
}


/*
===========
IN_DeactivateMouse

Called when the window loses focus
===========
*/
void IN_DeactivateMouse(void)
{
	if (!mouseinitialized)
		return;
	if (!mouseactive)
		return;

	mouse_rel_x = 0;
	mouse_rel_y = 0;

	SDL_SetRelativeMouseMode(false);
}



/*
===========
IN_StartupMouse
===========
*/
void IN_StartupMouse(void)
{
	cvar_t		*cv;

	cv = Cvar_Get("in_initmouse", "1", CVAR_NOSET);
	if (!cv->value)
		return;

	mouseinitialized = true;
	mouse_buttons = 5;
}

/*
===========
IN_MouseMove
===========
*/
void IN_MouseMove(usercmd_t *cmd)
{
	int		this_mouse_x, this_mouse_y;

	if (!mouseactive)
		return;

	// Get the relative mouse movement since the last time we called IN_MouseMove.
	this_mouse_x = mouse_rel_x;
	this_mouse_y = mouse_rel_y;

	// Reset it.
	mouse_rel_x = 0;
	mouse_rel_y = 0;

	// Apply sensitivity.
	this_mouse_x *= sensitivity->value;
	this_mouse_y *= sensitivity->value;

	// add mouse X/Y movement to cmd
	if ((in_strafe.state & 1) || (lookstrafe->value && mlooking))
		cmd->sidemove += m_side->value * this_mouse_x;
	else
		cl.viewangles[YAW] -= m_yaw->value * this_mouse_x;

	if ((mlooking || freelook->value) && !(in_strafe.state & 1))
	{
		cl.viewangles[PITCH] += m_pitch->value * this_mouse_y;
	}
	else
	{
		cmd->forwardmove -= m_forward->value * this_mouse_y;
	}
}


void IN_HandleMouseMotionEvent(const SDL_MouseMotionEvent* motion)
{
	mouse_rel_x += motion->xrel;
	mouse_rel_y += motion->yrel;
}

void IN_HandleMouseButtonEvent(const SDL_MouseButtonEvent* button)
{
	// buttons are 1-indexed.
	const int button_id = button->button - 1;

	if (!mouseinitialized)
		return;

	Key_Event(
		K_MOUSE1 + button_id,
		(button->state == SDL_PRESSED ? true : false),
		button->timestamp);
}

void IN_HandleMouseWheelEvent(const SDL_MouseWheelEvent* wheel)
{
	if (!mouseinitialized)
		return;

	if (wheel->x > 0)
	{
		Key_Event(K_MWHEELLEFT, true, wheel->timestamp);
		Key_Event(K_MWHEELLEFT, false, wheel->timestamp);
	}
	else if (wheel->x < 0)
	{
		Key_Event(K_MWHEELRIGHT, true, wheel->timestamp);
		Key_Event(K_MWHEELRIGHT, false, wheel->timestamp);
	}

	if (wheel->y > 0)
	{
		Key_Event(K_MWHEELUP, true, wheel->timestamp);
		Key_Event(K_MWHEELUP, false, wheel->timestamp);
	}
	else if (wheel->y < 0)
	{
		Key_Event(K_MWHEELDOWN, true, wheel->timestamp);
		Key_Event(K_MWHEELDOWN, false, wheel->timestamp);
	}
}
