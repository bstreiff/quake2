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
#include "client.h"
#include "SDL.h"

/*

key up events are sent even if in console mode

*/


#define		MAXCMDLINE	256
char	key_lines[32][MAXCMDLINE];
int		key_linepos;
int		shift_down=false;
int	anykeydown;

int		edit_line=0;
int		history_line=0;

int		key_waiting;
char	*keybindings[256];
qboolean	consolekeys[256];	// if true, can't be rebound while in console
qboolean	menubound[256];	// if true, can't be rebound while in menu
int		keyshift[256];		// key to map to if shift held down in console
int		key_repeats[256];	// if > 1, it is autorepeating
qboolean	keydown[256];

typedef struct
{
	char	*name;
	int		keynum;
} keyname_t;

keyname_t keynames[] =
{
	{"TAB", K_TAB},
	{"ENTER", K_ENTER},
	{"ESCAPE", K_ESCAPE},
	{"SPACE", K_SPACE},
	{"BACKSPACE", K_BACKSPACE},
	{"UPARROW", K_UPARROW},
	{"DOWNARROW", K_DOWNARROW},
	{"LEFTARROW", K_LEFTARROW},
	{"RIGHTARROW", K_RIGHTARROW},

	{"ALT", K_ALT},
	{"CTRL", K_CTRL},
	{"SHIFT", K_SHIFT},
	
	{"F1", K_F1},
	{"F2", K_F2},
	{"F3", K_F3},
	{"F4", K_F4},
	{"F5", K_F5},
	{"F6", K_F6},
	{"F7", K_F7},
	{"F8", K_F8},
	{"F9", K_F9},
	{"F10", K_F10},
	{"F11", K_F11},
	{"F12", K_F12},

	{"INS", K_INS},
	{"DEL", K_DEL},
	{"PGDN", K_PGDN},
	{"PGUP", K_PGUP},
	{"HOME", K_HOME},
	{"END", K_END},

	{"MOUSE1", K_MOUSE1},
	{"MOUSE2", K_MOUSE2},
	{"MOUSE3", K_MOUSE3},
	{"MOUSE4", K_MOUSE4},
	{"MOUSE5", K_MOUSE5},

	{"GAMEPAD_A", K_GAMEPAD_A},
	{"GAMEPAD_B", K_GAMEPAD_B},
	{"GAMEPAD_X", K_GAMEPAD_X},
	{"GAMEPAD_Y", K_GAMEPAD_Y},
	{"GAMEPAD_BACK", K_GAMEPAD_BACK},
	{"GAMEPAD_GUIDE", K_GAMEPAD_GUIDE},
	{"GAMEPAD_START", K_GAMEPAD_START},
	{"GAMEPAD_LSTICK", K_GAMEPAD_LSTICK},
	{"GAMEPAD_RSTICK", K_GAMEPAD_RSTICK},
	{"GAMEPAD_L1", K_GAMEPAD_L1},
	{"GAMEPAD_R1", K_GAMEPAD_R1},
	{"GAMEPAD_DPADUP", K_GAMEPAD_DPADUP},
	{"GAMEPAD_DPADDOWN", K_GAMEPAD_DPADDOWN},
	{"GAMEPAD_DPADLEFT", K_GAMEPAD_DPADLEFT},
	{"GAMEPAD_DPADRIGHT", K_GAMEPAD_DPADRIGHT},

	{"KP_HOME",			K_KP_HOME },
	{"KP_UPARROW",		K_KP_UPARROW },
	{"KP_PGUP",			K_KP_PGUP },
	{"KP_LEFTARROW",	K_KP_LEFTARROW },
	{"KP_5",			K_KP_5 },
	{"KP_RIGHTARROW",	K_KP_RIGHTARROW },
	{"KP_END",			K_KP_END },
	{"KP_DOWNARROW",	K_KP_DOWNARROW },
	{"KP_PGDN",			K_KP_PGDN },
	{"KP_ENTER",		K_KP_ENTER },
	{"KP_INS",			K_KP_INS },
	{"KP_DEL",			K_KP_DEL },
	{"KP_SLASH",		K_KP_SLASH },
	{"KP_TIMES",		K_KP_TIMES },
	{"KP_MINUS",		K_KP_MINUS },
	{"KP_PLUS",			K_KP_PLUS },

	{"MWHEELUP", K_MWHEELUP },
	{"MWHEELDOWN", K_MWHEELDOWN },
	{"MWHEELLEFT", K_MWHEELLEFT },
	{"MWHEELRIGHT", K_MWHEELRIGHT },

	{"PAUSE", K_PAUSE},

	{"SEMICOLON", ';'},	// because a raw semicolon seperates commands

	{NULL,0}
};

/*
==============================================================================

			LINE TYPING INTO THE CONSOLE

==============================================================================
*/

void CompleteCommand (void)
{
	char	*cmd, *s;

	s = key_lines[edit_line]+1;
	if (*s == '\\' || *s == '/')
		s++;

	cmd = Cmd_CompleteCommand (s);
	if (!cmd)
		cmd = Cvar_CompleteVariable (s);
	if (cmd)
	{
		key_lines[edit_line][1] = '/';
		strcpy (key_lines[edit_line]+2, cmd);
		key_linepos = strlen(cmd)+2;
		key_lines[edit_line][key_linepos] = ' ';
		key_linepos++;
		key_lines[edit_line][key_linepos] = 0;
		return;
	}
}

/*
====================
Key_Console

Interactive line editing and console scrollback
====================
*/
void Key_Console (int key)
{

	switch ( key )
	{
	case K_KP_SLASH:
		key = '/';
		break;
	case K_KP_MINUS:
		key = '-';
		break;
	case K_KP_PLUS:
		key = '+';
		break;
	case K_KP_HOME:
		key = '7';
		break;
	case K_KP_UPARROW:
		key = '8';
		break;
	case K_KP_PGUP:
		key = '9';
		break;
	case K_KP_LEFTARROW:
		key = '4';
		break;
	case K_KP_5:
		key = '5';
		break;
	case K_KP_RIGHTARROW:
		key = '6';
		break;
	case K_KP_END:
		key = '1';
		break;
	case K_KP_DOWNARROW:
		key = '2';
		break;
	case K_KP_PGDN:
		key = '3';
		break;
	case K_KP_INS:
		key = '0';
		break;
	case K_KP_DEL:
		key = '.';
		break;
	}

	if ( ( toupper( key ) == 'V' && keydown[K_CTRL] ) ||
		 ( ( ( key == K_INS ) || ( key == K_KP_INS ) ) && keydown[K_SHIFT] ) )
	{
		char *cbd;
		
		if ( ( cbd = Sys_GetClipboardData() ) != 0 )
		{
			int i;

			strtok( cbd, "\n\r\b" );

			i = strlen( cbd );
			if ( i + key_linepos >= MAXCMDLINE)
				i= MAXCMDLINE - key_linepos;

			if ( i > 0 )
			{
				cbd[i]=0;
				strcat( key_lines[edit_line], cbd );
				key_linepos += i;
			}
			free( cbd );
		}

		return;
	}

	if ( key == 'l' ) 
	{
		if ( keydown[K_CTRL] )
		{
			Cbuf_AddText ("clear\n");
			return;
		}
	}

	if ( key == K_ENTER || key == K_KP_ENTER )
	{	// backslash text are commands, else chat
		if (key_lines[edit_line][1] == '\\' || key_lines[edit_line][1] == '/')
			Cbuf_AddText (key_lines[edit_line]+2);	// skip the >
		else
			Cbuf_AddText (key_lines[edit_line]+1);	// valid command

		Cbuf_AddText ("\n");
		Com_Printf ("%s\n",key_lines[edit_line]);
		edit_line = (edit_line + 1) & 31;
		history_line = edit_line;
		key_lines[edit_line][0] = ']';
		key_linepos = 1;
		if (cls.state == ca_disconnected)
			SCR_UpdateScreen ();	// force an update, because the command
									// may take some time
		return;
	}

	if (key == K_TAB)
	{	// command completion
		CompleteCommand ();
		return;
	}
	
	if ( ( key == K_BACKSPACE ) || ( key == K_LEFTARROW ) || ( key == K_KP_LEFTARROW ) || ( ( key == 'h' ) && ( keydown[K_CTRL] ) ) )
	{
		if (key_linepos > 1)
			key_linepos--;
		return;
	}

	if ( ( key == K_UPARROW ) || ( key == K_KP_UPARROW ) ||
		 ( ( key == 'p' ) && keydown[K_CTRL] ) )
	{
		do
		{
			history_line = (history_line - 1) & 31;
		} while (history_line != edit_line
				&& !key_lines[history_line][1]);
		if (history_line == edit_line)
			history_line = (edit_line+1)&31;
		strcpy(key_lines[edit_line], key_lines[history_line]);
		key_linepos = strlen(key_lines[edit_line]);
		return;
	}

	if ( ( key == K_DOWNARROW ) || ( key == K_KP_DOWNARROW ) ||
		 ( ( key == 'n' ) && keydown[K_CTRL] ) )
	{
		if (history_line == edit_line) return;
		do
		{
			history_line = (history_line + 1) & 31;
		}
		while (history_line != edit_line
			&& !key_lines[history_line][1]);
		if (history_line == edit_line)
		{
			key_lines[edit_line][0] = ']';
			key_linepos = 1;
		}
		else
		{
			strcpy(key_lines[edit_line], key_lines[history_line]);
			key_linepos = strlen(key_lines[edit_line]);
		}
		return;
	}

	if (key == K_PGUP || key == K_KP_PGUP )
	{
		con.display -= 2;
		return;
	}

	if (key == K_PGDN || key == K_KP_PGDN ) 
	{
		con.display += 2;
		if (con.display > con.current)
			con.display = con.current;
		return;
	}

	if (key == K_HOME || key == K_KP_HOME )
	{
		con.display = con.current - con.totallines + 10;
		return;
	}

	if (key == K_END || key == K_KP_END )
	{
		con.display = con.current;
		return;
	}
	
	if (key < 32 || key > 127)
		return;	// non printable
		
	if (key_linepos < MAXCMDLINE-1)
	{
		key_lines[edit_line][key_linepos] = key;
		key_linepos++;
		key_lines[edit_line][key_linepos] = 0;
	}

}

//============================================================================

qboolean	chat_team;
qboolean	chat_console;
char		chat_buffer[MAXCMDLINE];
int			chat_bufferlen = 0;

void Key_Message (int key)
{

	if ( key == K_ENTER || key == K_KP_ENTER )
	{
		if (chat_console) {
			Cbuf_AddText(chat_buffer);
		} else {
			if (chat_team)
				Cbuf_AddText ("say_team \"");
			else
				Cbuf_AddText ("say \"");
			Cbuf_AddText(chat_buffer);
			Cbuf_AddText("\"\n");
		}

		cls.key_dest = key_game;
		chat_bufferlen = 0;
		chat_buffer[0] = 0;
		return;
	}

	if (key == K_ESCAPE)
	{
		cls.key_dest = key_game;
		chat_bufferlen = 0;
		chat_buffer[0] = 0;
		return;
	}

	if (key < 32 || key > 127)
		return;	// non printable

	if (key == K_BACKSPACE)
	{
		if (chat_bufferlen)
		{
			chat_bufferlen--;
			chat_buffer[chat_bufferlen] = 0;
		}
		return;
	}

	if (chat_bufferlen == sizeof(chat_buffer)-1)
		return; // all full

	chat_buffer[chat_bufferlen++] = key;
	chat_buffer[chat_bufferlen] = 0;
}

//============================================================================


/*
===================
Key_StringToKeynum

Returns a key number to be used to index keybindings[] by looking at
the given string.  Single ascii characters return themselves, while
the K_* names are matched up.
===================
*/
int Key_StringToKeynum (char *str)
{
	keyname_t	*kn;
	
	if (!str || !str[0])
		return -1;
	if (!str[1])
		return str[0];

	for (kn=keynames ; kn->name ; kn++)
	{
		if (!Q_strcasecmp(str,kn->name))
			return kn->keynum;
	}
	return -1;
}

/*
===================
Key_KeynumToString

Returns a string (either a single ascii char, or a K_* name) for the
given keynum.
FIXME: handle quote special (general escape sequence?)
===================
*/
char *Key_KeynumToString (int keynum)
{
	keyname_t	*kn;	
	static	char	tinystr[2];
	
	if (keynum == -1)
		return "<KEY NOT FOUND>";
	if (keynum > 32 && keynum < 127)
	{	// printable ascii
		tinystr[0] = keynum;
		tinystr[1] = 0;
		return tinystr;
	}
	
	for (kn=keynames ; kn->name ; kn++)
		if (keynum == kn->keynum)
			return kn->name;

	return "<UNKNOWN KEYNUM>";
}


/*
===================
Key_SetBinding
===================
*/
void Key_SetBinding (int keynum, char *binding)
{
	char	*new;
	int		l;
			
	if (keynum == -1)
		return;

// free old bindings
	if (keybindings[keynum])
	{
		Z_Free (keybindings[keynum]);
		keybindings[keynum] = NULL;
	}
			
// allocate memory for new binding
	l = strlen (binding);	
	new = Z_Malloc (l+1);
	strcpy (new, binding);
	new[l] = 0;
	keybindings[keynum] = new;	
}

/*
===================
Key_Unbind_f
===================
*/
void Key_Unbind_f (void)
{
	int		b;

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("unbind <key> : remove commands from a key\n");
		return;
	}
	
	b = Key_StringToKeynum (Cmd_Argv(1));
	if (b==-1)
	{
		Com_Printf ("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	Key_SetBinding (b, "");
}

void Key_Unbindall_f (void)
{
	int		i;
	
	for (i=0 ; i<256 ; i++)
		if (keybindings[i])
			Key_SetBinding (i, "");
}


/*
===================
Key_Bind_f
===================
*/
void Key_Bind_f (void)
{
	int			i, c, b;
	char		cmd[1024];
	
	c = Cmd_Argc();

	if (c < 2)
	{
		Com_Printf ("bind <key> [command] : attach a command to a key\n");
		return;
	}
	b = Key_StringToKeynum (Cmd_Argv(1));
	if (b==-1)
	{
		Com_Printf ("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	if (c == 2)
	{
		if (keybindings[b])
			Com_Printf ("\"%s\" = \"%s\"\n", Cmd_Argv(1), keybindings[b] );
		else
			Com_Printf ("\"%s\" is not bound\n", Cmd_Argv(1) );
		return;
	}
	
// copy the rest of the command line
	cmd[0] = 0;		// start out with a null string
	for (i=2 ; i< c ; i++)
	{
		strcat (cmd, Cmd_Argv(i));
		if (i != (c-1))
			strcat (cmd, " ");
	}

	Key_SetBinding (b, cmd);
}

/*
============
Key_WriteBindings

Writes lines containing "bind key value"
============
*/
void Key_WriteBindings (FILE *f)
{
	int		i;

	for (i=0 ; i<256 ; i++)
		if (keybindings[i] && keybindings[i][0])
			fprintf (f, "bind %s \"%s\"\n", Key_KeynumToString(i), keybindings[i]);
}


/*
============
Key_Bindlist_f

============
*/
void Key_Bindlist_f (void)
{
	int		i;

	for (i=0 ; i<256 ; i++)
		if (keybindings[i] && keybindings[i][0])
			Com_Printf ("%s \"%s\"\n", Key_KeynumToString(i), keybindings[i]);
}


/*
===================
Key_Init
===================
*/
void Key_Init (void)
{
	int		i;

	for (i=0 ; i<32 ; i++)
	{
		key_lines[i][0] = ']';
		key_lines[i][1] = 0;
	}
	key_linepos = 1;
	
//
// init ascii characters in console mode
//
	for (i=32 ; i<128 ; i++)
		consolekeys[i] = true;
	consolekeys[K_ENTER] = true;
	consolekeys[K_KP_ENTER] = true;
	consolekeys[K_TAB] = true;
	consolekeys[K_LEFTARROW] = true;
	consolekeys[K_KP_LEFTARROW] = true;
	consolekeys[K_RIGHTARROW] = true;
	consolekeys[K_KP_RIGHTARROW] = true;
	consolekeys[K_UPARROW] = true;
	consolekeys[K_KP_UPARROW] = true;
	consolekeys[K_DOWNARROW] = true;
	consolekeys[K_KP_DOWNARROW] = true;
	consolekeys[K_BACKSPACE] = true;
	consolekeys[K_HOME] = true;
	consolekeys[K_KP_HOME] = true;
	consolekeys[K_END] = true;
	consolekeys[K_KP_END] = true;
	consolekeys[K_PGUP] = true;
	consolekeys[K_KP_PGUP] = true;
	consolekeys[K_PGDN] = true;
	consolekeys[K_KP_PGDN] = true;
	consolekeys[K_SHIFT] = true;
	consolekeys[K_INS] = true;
	consolekeys[K_KP_INS] = true;
	consolekeys[K_KP_DEL] = true;
	consolekeys[K_KP_SLASH] = true;
	consolekeys[K_KP_PLUS] = true;
	consolekeys[K_KP_MINUS] = true;
	consolekeys[K_KP_5] = true;

	consolekeys['`'] = false;
	consolekeys['~'] = false;

	for (i=0 ; i<256 ; i++)
		keyshift[i] = i;
	for (i='a' ; i<='z' ; i++)
		keyshift[i] = i - 'a' + 'A';
	keyshift['1'] = '!';
	keyshift['2'] = '@';
	keyshift['3'] = '#';
	keyshift['4'] = '$';
	keyshift['5'] = '%';
	keyshift['6'] = '^';
	keyshift['7'] = '&';
	keyshift['8'] = '*';
	keyshift['9'] = '(';
	keyshift['0'] = ')';
	keyshift['-'] = '_';
	keyshift['='] = '+';
	keyshift[','] = '<';
	keyshift['.'] = '>';
	keyshift['/'] = '?';
	keyshift[';'] = ':';
	keyshift['\''] = '"';
	keyshift['['] = '{';
	keyshift[']'] = '}';
	keyshift['`'] = '~';
	keyshift['\\'] = '|';

	menubound[K_ESCAPE] = true;
	for (i=0 ; i<12 ; i++)
		menubound[K_F1+i] = true;

//
// register our functions
//
	Cmd_AddCommand ("bind",Key_Bind_f);
	Cmd_AddCommand ("unbind",Key_Unbind_f);
	Cmd_AddCommand ("unbindall",Key_Unbindall_f);
	Cmd_AddCommand ("bindlist",Key_Bindlist_f);
}

/*
===================
Key_Event

Called by the system between frames for both key up and key down events
Should NOT be called during an interrupt!
===================
*/
void Key_Event (int key, qboolean down, unsigned time)
{
	char	*kb;
	char	cmd[1024];

	// hack for modal presses
	if (key_waiting == -1)
	{
		if (down)
			key_waiting = key;
		return;
	}

	// update auto-repeat status
	if (down)
	{
		key_repeats[key]++;
		if (key != K_BACKSPACE 
			&& key != K_PAUSE 
			&& key != K_PGUP 
			&& key != K_KP_PGUP 
			&& key != K_PGDN
			&& key != K_KP_PGDN
			&& key_repeats[key] > 1)
			return;	// ignore most autorepeats
			
		if (key >= 200 && !keybindings[key])
			Com_Printf ("%s is unbound, hit F4 to set.\n", Key_KeynumToString (key) );
	}
	else
	{
		key_repeats[key] = 0;
	}

	if (key == K_SHIFT)
		shift_down = down;

	// console key is hardcoded, so the user can never unbind it
	if (key == '`' || key == '~')
	{
		if (!down)
			return;
		Con_ToggleConsole_f ();
		return;
	}

	// any key during the attract mode will bring up the menu
	if (cl.attractloop && cls.key_dest != key_menu &&
		!(key >= K_F1 && key <= K_F12))
		key = K_ESCAPE;

	// menu key is hardcoded, so the user can never unbind it
	if (key == K_ESCAPE)
	{
		if (!down)
			return;

		if (cl.frame.playerstate.stats[STAT_LAYOUTS] && cls.key_dest == key_game)
		{	// put away help computer / inventory
			Cbuf_AddText ("cmd putaway\n");
			return;
		}
		switch (cls.key_dest)
		{
		case key_message:
			Key_Message (key);
			break;
		case key_menu:
			M_Keydown (key);
			break;
		case key_game:
		case key_console:
			M_Menu_Main_f ();
			break;
		default:
			Com_Error (ERR_FATAL, "Bad cls.key_dest");
		}
		return;
	}

	// track if any key is down for BUTTON_ANY
	keydown[key] = down;
	if (down)
	{
		if (key_repeats[key] == 1)
			anykeydown++;
	}
	else
	{
		anykeydown--;
		if (anykeydown < 0)
			anykeydown = 0;
	}

//
// key up events only generate commands if the game key binding is
// a button command (leading + sign).  These will occur even in console mode,
// to keep the character from continuing an action started before a console
// switch.  Button commands include the kenum as a parameter, so multiple
// downs can be matched with ups
//
	if (!down)
	{
		kb = keybindings[key];
		if (kb && kb[0] == '+')
		{
			Com_sprintf (cmd, sizeof(cmd), "-%s %i %i\n", kb+1, key, time);
			Cbuf_AddText (cmd);
		}
		if (keyshift[key] != key)
		{
			kb = keybindings[keyshift[key]];
			if (kb && kb[0] == '+')
			{
				Com_sprintf (cmd, sizeof(cmd), "-%s %i %i\n", kb+1, key, time);
				Cbuf_AddText (cmd);
			}
		}
		return;
	}

//
// if not a consolekey, send to the interpreter no matter what mode is
//
	if ( (cls.key_dest == key_menu && menubound[key])
	|| (cls.key_dest == key_console && !consolekeys[key])
	|| (cls.key_dest == key_game && ( cls.state == ca_active || !consolekeys[key] ) ) )
	{
		kb = keybindings[key];
		if (kb)
		{
			if (kb[0] == '+')
			{	// button commands add keynum and time as a parm
				Com_sprintf (cmd, sizeof(cmd), "%s %i %i\n", kb, key, time);
				Cbuf_AddText (cmd);
			}
			else
			{
				Cbuf_AddText (kb);
				Cbuf_AddText ("\n");
			}
		}
		return;
	}

	if (!down)
		return;		// other systems only care about key down events

	if (shift_down)
		key = keyshift[key];

	switch (cls.key_dest)
	{
	case key_message:
		Key_Message (key);
		break;
	case key_menu:
		M_Keydown (key);
		break;

	case key_game:
	case key_console:
		Key_Console (key);
		break;
	default:
		Com_Error (ERR_FATAL, "Bad cls.key_dest");
	}
}

/*
===================
Key_ClearStates
===================
*/
void Key_ClearStates (void)
{
	int		i;

	anykeydown = false;

	for (i=0 ; i<256 ; i++)
	{
		if ( keydown[i] || key_repeats[i] )
			Key_Event( i, false, 0 );
		keydown[i] = 0;
		key_repeats[i] = 0;
	}
}


/*
===================
Key_GetKey
===================
*/
int Key_GetKey (void)
{
	key_waiting = -1;

	while (key_waiting == -1)
		Sys_SendKeyEvents ();

	return key_waiting;
}


// The glue to connect SDL key events to Quake key events.
//
// We could make the Quake key event system use the SDL keysyms
// directly, but I think that'd end up threading SDL through more
// places than I'd like.

typedef struct {
	int sdl_key;
	keysym_t quake_key;
} sdl_to_quake_keymap_t;

static const sdl_to_quake_keymap_t sdl_to_quake_keymap[] =
{
	{ SDLK_UNKNOWN, 0 },
	{ SDLK_RETURN, K_ENTER },
	{ SDLK_ESCAPE, K_ESCAPE },
	{ SDLK_BACKSPACE, K_BACKSPACE },
	{ SDLK_TAB, K_TAB },
	{ SDLK_SPACE, K_SPACE },
	{ SDLK_F1, K_F1 },
	{ SDLK_F2, K_F2 },
	{ SDLK_F3, K_F3 },
	{ SDLK_F4, K_F4 },
	{ SDLK_F5, K_F5 },
	{ SDLK_F6, K_F6 },
	{ SDLK_F7, K_F7 },
	{ SDLK_F8, K_F8 },
	{ SDLK_F9, K_F9 },
	{ SDLK_F10, K_F10 },
	{ SDLK_F11, K_F11 },
	{ SDLK_F12, K_F12 },
	{ SDLK_PRINTSCREEN, K_UNHANDLED },
	{ SDLK_PAUSE, K_PAUSE },
	{ SDLK_INSERT, K_INS },
	{ SDLK_HOME, K_HOME },
	{ SDLK_PAGEUP, K_PGUP },
	{ SDLK_DELETE, K_DEL },
	{ SDLK_END, K_END },
	{ SDLK_PAGEDOWN, K_PGDN },
	{ SDLK_RIGHT, K_RIGHTARROW },
	{ SDLK_LEFT, K_LEFTARROW },
	{ SDLK_DOWN, K_DOWNARROW },
	{ SDLK_UP, K_UPARROW },
	{ SDLK_KP_DIVIDE, K_KP_SLASH },
	{ SDLK_KP_MULTIPLY, K_KP_TIMES },
	{ SDLK_KP_MINUS, K_KP_MINUS },
	{ SDLK_KP_PLUS, K_KP_PLUS },
	{ SDLK_KP_ENTER, K_KP_ENTER },
	{ SDLK_KP_1, K_KP_END },
	{ SDLK_KP_2, K_KP_DOWNARROW },
	{ SDLK_KP_3, K_KP_PGDN },
	{ SDLK_KP_4, K_KP_LEFTARROW },
	{ SDLK_KP_5, K_KP_5 },
	{ SDLK_KP_6, K_KP_RIGHTARROW },
	{ SDLK_KP_7, K_KP_HOME },
	{ SDLK_KP_8, K_KP_UPARROW },
	{ SDLK_KP_9, K_KP_PGUP },
	{ SDLK_KP_0, K_KP_INS },
	{ SDLK_KP_PERIOD, K_KP_DEL },

	{ SDLK_APPLICATION, K_UNHANDLED },
	{ SDLK_POWER, K_UNHANDLED },
	{ SDLK_KP_EQUALS, K_UNHANDLED},
	{ SDLK_F13, K_UNHANDLED },
	{ SDLK_F14, K_UNHANDLED },
	{ SDLK_F15, K_UNHANDLED },
	{ SDLK_F16, K_UNHANDLED },
	{ SDLK_F17, K_UNHANDLED },
	{ SDLK_F18, K_UNHANDLED },
	{ SDLK_F19, K_UNHANDLED },
	{ SDLK_F20, K_UNHANDLED },
	{ SDLK_F21, K_UNHANDLED },
	{ SDLK_F22, K_UNHANDLED },
	{ SDLK_F23, K_UNHANDLED },
	{ SDLK_F24, K_UNHANDLED },
	{ SDLK_EXECUTE, K_UNHANDLED },
	{ SDLK_HELP, K_UNHANDLED },
	{ SDLK_MENU, K_UNHANDLED },
	{ SDLK_SELECT, K_UNHANDLED },
	{ SDLK_STOP, K_UNHANDLED },
	{ SDLK_AGAIN, K_UNHANDLED },
	{ SDLK_UNDO, K_UNHANDLED },
	{ SDLK_CUT, K_UNHANDLED },
	{ SDLK_COPY, K_UNHANDLED },
	{ SDLK_PASTE, K_UNHANDLED },
	{ SDLK_FIND, K_UNHANDLED },
	{ SDLK_MUTE, K_UNHANDLED },
	{ SDLK_VOLUMEUP, K_UNHANDLED },
	{ SDLK_VOLUMEDOWN, K_UNHANDLED },
	{ SDLK_KP_COMMA, K_UNHANDLED },
	{ SDLK_KP_EQUALSAS400, K_UNHANDLED },
	{ SDLK_ALTERASE, K_UNHANDLED },
	{ SDLK_SYSREQ, K_UNHANDLED },
	{ SDLK_CANCEL, K_UNHANDLED },
	{ SDLK_CLEAR, K_UNHANDLED },
	{ SDLK_PRIOR, K_UNHANDLED },
	{ SDLK_RETURN2, K_UNHANDLED },
	{ SDLK_SEPARATOR, K_UNHANDLED },
	{ SDLK_OUT, K_UNHANDLED },
	{ SDLK_OPER, K_UNHANDLED },
	{ SDLK_CLEARAGAIN, K_UNHANDLED },
	{ SDLK_CRSEL, K_UNHANDLED },
	{ SDLK_EXSEL, K_UNHANDLED },
	{ SDLK_KP_00, K_UNHANDLED },
	{ SDLK_KP_000, K_UNHANDLED },
	{ SDLK_THOUSANDSSEPARATOR, K_UNHANDLED },
	{ SDLK_DECIMALSEPARATOR, K_UNHANDLED },
	{ SDLK_CURRENCYUNIT, K_UNHANDLED },
	{ SDLK_CURRENCYSUBUNIT, K_UNHANDLED },
	{ SDLK_KP_LEFTPAREN, K_UNHANDLED },
	{ SDLK_KP_RIGHTPAREN, K_UNHANDLED },
	{ SDLK_KP_LEFTBRACE, K_UNHANDLED },
	{ SDLK_KP_RIGHTBRACE, K_UNHANDLED },
	{ SDLK_KP_TAB, K_UNHANDLED },
	{ SDLK_KP_BACKSPACE, K_UNHANDLED },
	{ SDLK_KP_A, K_UNHANDLED },
	{ SDLK_KP_B, K_UNHANDLED },
	{ SDLK_KP_C, K_UNHANDLED },
	{ SDLK_KP_D, K_UNHANDLED },
	{ SDLK_KP_E, K_UNHANDLED },
	{ SDLK_KP_F, K_UNHANDLED },
	{ SDLK_KP_XOR, K_UNHANDLED },
	{ SDLK_KP_POWER, K_UNHANDLED },
	{ SDLK_KP_PERCENT, K_UNHANDLED },
	{ SDLK_KP_LESS, K_UNHANDLED },
	{ SDLK_KP_GREATER, K_UNHANDLED },
	{ SDLK_KP_AMPERSAND, K_UNHANDLED },
	{ SDLK_KP_DBLAMPERSAND, K_UNHANDLED },
	{ SDLK_KP_VERTICALBAR, K_UNHANDLED },
	{ SDLK_KP_DBLVERTICALBAR, K_UNHANDLED },
	{ SDLK_KP_COLON, K_UNHANDLED },
	{ SDLK_KP_HASH, K_UNHANDLED },
	{ SDLK_KP_SPACE, K_UNHANDLED },
	{ SDLK_KP_AT, K_UNHANDLED },
	{ SDLK_KP_EXCLAM, K_UNHANDLED },
	{ SDLK_KP_MEMSTORE, K_UNHANDLED },
	{ SDLK_KP_MEMRECALL, K_UNHANDLED },
	{ SDLK_KP_MEMCLEAR, K_UNHANDLED },
	{ SDLK_KP_MEMADD, K_UNHANDLED },
	{ SDLK_KP_MEMSUBTRACT, K_UNHANDLED },
	{ SDLK_KP_MEMMULTIPLY, K_UNHANDLED },
	{ SDLK_KP_MEMDIVIDE, K_UNHANDLED },
	{ SDLK_KP_PLUSMINUS, K_UNHANDLED },
	{ SDLK_KP_CLEAR, K_UNHANDLED },
	{ SDLK_KP_CLEARENTRY, K_UNHANDLED },
	{ SDLK_KP_BINARY, K_UNHANDLED },
	{ SDLK_KP_OCTAL, K_UNHANDLED },
	{ SDLK_KP_DECIMAL, K_UNHANDLED },
	{ SDLK_KP_HEXADECIMAL, K_UNHANDLED },

	{ SDLK_LCTRL, K_CTRL },
	{ SDLK_LSHIFT, K_SHIFT },
	{ SDLK_LALT, K_ALT },
	{ SDLK_LGUI, K_UNHANDLED },
	{ SDLK_RCTRL, K_CTRL },
	{ SDLK_RSHIFT, K_SHIFT },
	{ SDLK_RALT, K_ALT },
	{ SDLK_RGUI, K_UNHANDLED },

	{ SDLK_MODE, K_UNHANDLED },

	{ SDLK_AUDIONEXT, K_UNHANDLED },
	{ SDLK_AUDIOPREV, K_UNHANDLED },
	{ SDLK_AUDIOSTOP, K_UNHANDLED },
	{ SDLK_AUDIOPLAY, K_UNHANDLED },
	{ SDLK_AUDIOMUTE, K_UNHANDLED },
	{ SDLK_MEDIASELECT, K_UNHANDLED },
	{ SDLK_WWW, K_UNHANDLED },
	{ SDLK_MAIL, K_UNHANDLED },
	{ SDLK_CALCULATOR, K_UNHANDLED },
	{ SDLK_COMPUTER, K_UNHANDLED },
	{ SDLK_AC_SEARCH, K_UNHANDLED },
	{ SDLK_AC_HOME, K_UNHANDLED },
	{ SDLK_AC_BACK, K_UNHANDLED },
	{ SDLK_AC_FORWARD, K_UNHANDLED },
	{ SDLK_AC_STOP, K_UNHANDLED },
	{ SDLK_AC_REFRESH, K_UNHANDLED },
	{ SDLK_AC_BOOKMARKS, K_UNHANDLED },

	{ SDLK_BRIGHTNESSDOWN, K_UNHANDLED },
	{ SDLK_BRIGHTNESSUP, K_UNHANDLED },
	{ SDLK_DISPLAYSWITCH, K_UNHANDLED },
	{ SDLK_KBDILLUMTOGGLE, K_UNHANDLED },
	{ SDLK_KBDILLUMDOWN, K_UNHANDLED },
	{ SDLK_KBDILLUMUP, K_UNHANDLED },
	{ SDLK_EJECT, K_UNHANDLED },
	{ SDLK_SLEEP, K_UNHANDLED },
};
static const size_t sdl_to_quake_keymap_length = sizeof(sdl_to_quake_keymap) / sizeof(sdl_to_quake_keymap[0]);

static keysym_t QuakeKeyFromSDLKeySym(const SDL_Keysym* keysym)
{
	// All of these ones map 1:1.
	if (keysym->sym >= SDLK_SPACE && keysym->sym <= SDLK_z)
		return (keysym_t)(keysym->sym);

	// Otherwise, we have to look it up.
	for (size_t i = 0; i < sdl_to_quake_keymap_length; ++i)
	{
		if (keysym->sym == sdl_to_quake_keymap[i].sdl_key)
		{
			return sdl_to_quake_keymap[i].quake_key;
		}
	}

	return K_UNHANDLED;
}

void Key_HandleKeyboardEvent(const SDL_KeyboardEvent* key)
{
	keysym_t qkey = QuakeKeyFromSDLKeySym(&(key->keysym));

	if (qkey != K_UNHANDLED)
	{
		Key_Event(
			qkey,
			(key->type == SDL_KEYDOWN ? true : false),
			key->timestamp);
	}
}

