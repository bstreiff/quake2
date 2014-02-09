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
extern cvar_t	*m_filter;

qboolean	mlooking;

void IN_MLookDown(void) { mlooking = true; }
void IN_MLookUp(void) {
	mlooking = false;
	if (!freelook->value && lookspring->value)
		IN_CenterView();
}

int			mouse_buttons;
int			mouse_oldbuttonstate;
POINT		current_pos;
int			mouse_x, mouse_y, old_mouse_x, old_mouse_y, mx_accum, my_accum;

int			old_x, old_y;

qboolean	mouseactive;	// false when not focus app

qboolean	restore_spi;
qboolean	mouseinitialized;
int		originalmouseparms[3], newmouseparms[3] = { 0, 0, 1 };
qboolean	mouseparmsvalid;

int			window_center_x, window_center_y;
RECT		window_rect;


/*
===========
IN_ActivateMouse

Called when the window gains focus or changes in some way
===========
*/
void IN_ActivateMouse(void)
{
	//int		width, height;

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
	/*
	if (mouseparmsvalid)
		restore_spi = SystemParametersInfo(SPI_SETMOUSE, 0, newmouseparms, 0);

	width = GetSystemMetrics(SM_CXSCREEN);
	height = GetSystemMetrics(SM_CYSCREEN);

	GetWindowRect(cl_hwnd, &window_rect);
	if (window_rect.left < 0)
		window_rect.left = 0;
	if (window_rect.top < 0)
		window_rect.top = 0;
	if (window_rect.right >= width)
		window_rect.right = width - 1;
	if (window_rect.bottom >= height - 1)
		window_rect.bottom = height - 1;

	window_center_x = (window_rect.right + window_rect.left) / 2;
	window_center_y = (window_rect.top + window_rect.bottom) / 2;

	SetCursorPos(window_center_x, window_center_y);

	old_x = window_center_x;
	old_y = window_center_y;

	SetCapture(cl_hwnd);
	ClipCursor(&window_rect);
	while (ShowCursor(FALSE) >= 0)
		;
	*/
	Com_Printf("setting relative mouse mode\n");
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

	/*
	if (restore_spi)
		SystemParametersInfo(SPI_SETMOUSE, 0, originalmouseparms, 0);

	mouseactive = false;

	ClipCursor(NULL);
	ReleaseCapture();
	while (ShowCursor(TRUE) < 0)
		;
	*/
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
	mouseparmsvalid = SystemParametersInfo(SPI_GETMOUSE, 0, originalmouseparms, 0);
	mouse_buttons = 5;
}

/*
===========
IN_MouseMove
===========
*/
void IN_MouseMove(usercmd_t *cmd)
{
	int		mx, my;

	if (!mouseactive)
		return;

	// find mouse movement
	//if (!GetCursorPos(&current_pos))
	//	return;

	mx = current_pos.x - window_center_x;
	my = current_pos.y - window_center_y;

#if 0
	if (!mx && !my)
		return;
#endif

	if (m_filter->value)
	{
		mouse_x = (mx + old_mouse_x) * 0.5;
		mouse_y = (my + old_mouse_y) * 0.5;
	}
	else
	{
		mouse_x = mx;
		mouse_y = my;
	}

	old_mouse_x = mx;
	old_mouse_y = my;

	mouse_x *= sensitivity->value;
	mouse_y *= sensitivity->value;

	// add mouse X/Y movement to cmd
	if ((in_strafe.state & 1) || (lookstrafe->value && mlooking))
		cmd->sidemove += m_side->value * mouse_x;
	else
		cl.viewangles[YAW] -= m_yaw->value * mouse_x;

	if ((mlooking || freelook->value) && !(in_strafe.state & 1))
	{
		cl.viewangles[PITCH] += m_pitch->value * mouse_y;
	}
	else
	{
		cmd->forwardmove -= m_forward->value * mouse_y;
	}

	// force the mouse to the center, so there's room to move
	//if (mx || my)
	//	SetCursorPos(window_center_x, window_center_y);
}


void IN_HandleMouseMotionEvent(const SDL_MouseMotionEvent* motion)
{
	mouse_x = motion->x;
	mouse_y = motion->y;
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
