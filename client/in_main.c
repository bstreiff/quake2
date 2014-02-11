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

void IN_MLookDown(void);
void IN_MLookUp(void);

extern qboolean	mouseactive;	// false when not focus app
extern qboolean	mouseinitialized;

qboolean	in_appactive;

void IN_ActivateMouse(void);
void IN_DeactivateMouse(void);
void IN_StartupMouse(void);
void IN_ShutdownMouse(void);
void IN_MouseMove(usercmd_t *cmd);

void IN_ActivateGamepad(void);
void IN_DeactivateGamepad(void);
void IN_StartupGamepad(void);
void IN_ShutdownGamepad(void);
void IN_GamepadMove(usercmd_t *cmd);


/*
=========================================================================

VIEW CENTERING

=========================================================================
*/

cvar_t	*v_centermove;
cvar_t	*v_centerspeed;


/*
===========
IN_Init
===========
*/
void IN_Init (void)
{
	// centering
	v_centermove			= Cvar_Get ("v_centermove",				"0.15",		0);
	v_centerspeed			= Cvar_Get ("v_centerspeed",			"500",		0);

	Cmd_AddCommand ("+mlook", IN_MLookDown);
	Cmd_AddCommand ("-mlook", IN_MLookUp);

	IN_StartupMouse();
	IN_StartupGamepad();
}

/*
===========
IN_Shutdown
===========
*/
void IN_Shutdown (void)
{
	IN_DeactivateMouse();
	IN_DeactivateGamepad();

	IN_ShutdownMouse();
	IN_ShutdownGamepad();
}


/*
===========
IN_Activate

Called when the main window gains or loses focus.
The window may have been destroyed and recreated
between a deactivate and an activate.
===========
*/
void IN_Activate (qboolean active)
{
	in_appactive = active;
	mouseactive = !active;		// force a new window check or turn off
	Com_Printf("IN_Activate(%s)\n", (active ? "true" : "false"));

	if (active)
	{
		IN_ActivateMouse();
		IN_ActivateGamepad();
	}
	else
	{
		IN_DeactivateMouse();
		IN_DeactivateGamepad();
	}
}


/*
==================
IN_Frame

Called every frame, even if not generating commands
==================
*/
void IN_Frame (void)
{
	if (!mouseinitialized)
		return;

	if (!in_appactive)
	{
		IN_DeactivateMouse();
		IN_DeactivateGamepad();
		return;
	}

	if ( !cl.refresh_prepped
		|| cls.key_dest == key_console
		|| cls.key_dest == key_menu)
	{
		// temporarily deactivate if in fullscreen
		if (Cvar_VariableValue ("vid_fullscreen") == 0)
		{
			IN_DeactivateMouse();
			IN_DeactivateGamepad();
			return;
		}
	}

	IN_ActivateMouse();
	IN_ActivateGamepad();
}

/*
===========
IN_Move
===========
*/
void IN_Move (usercmd_t *cmd)
{
	if (in_appactive)
	{
		IN_MouseMove (cmd);
		IN_GamepadMove (cmd);
	}
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
===================
IN_ClearStates
===================
*/
void IN_ClearStates (void)
{
}


