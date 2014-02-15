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
// sys_win.h

#include "../qcommon/qcommon.h"
#include "winquake.h"
#include "resource.h"
#include <errno.h>
#include <float.h>
#include <fcntl.h>
#include <stdio.h>
#include <direct.h>
#include <io.h>
#include <conio.h>
#include "../win32/conproc.h"
#include "../client/input.h"
#include "../client/vid.h"
#include "../client/keys.h"

#include "SDL.h"

int			starttime;
int			ActiveApp;
qboolean	Minimized;

static HANDLE		hinput, houtput;

unsigned	sys_frame_time;


static HANDLE		qwclsemaphore;

static int g_argc;
static char** g_argv;

/*
===============================================================================

SYSTEM IO

===============================================================================
*/


void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[1024];

	CL_Shutdown ();
	Qcommon_Shutdown ();

	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);

	SDL_ShowSimpleMessageBox(
		SDL_MESSAGEBOX_ERROR,
		"Error",
		text,
		NULL);

	if (qwclsemaphore)
		CloseHandle (qwclsemaphore);

// shut down QHOST hooks if necessary
	DeinitConProc ();

	exit (1);
}

void Sys_Quit (void)
{
	//timeEndPeriod( 1 );

	CL_Shutdown();
	Qcommon_Shutdown ();
	CloseHandle (qwclsemaphore);
	if (dedicated && dedicated->value)
		FreeConsole ();

// shut down QHOST hooks if necessary
	DeinitConProc ();

	exit (0);
}


//================================================================


/*
================
Sys_ScanForCD

================
*/
char *Sys_ScanForCD (void)
{
	// Nobody has CD drives anymore.
	return NULL;
}

//================================================================


/*
================
Sys_Init
================
*/
void Sys_Init (void)
{
#if 0
	// allocate a named semaphore on the client so the
	// front end can tell if it is alive

	// mutex will fail if semephore already exists
    qwclsemaphore = CreateMutex(
        NULL,         /* Security attributes */
        0,            /* owner       */
        "qwcl"); /* Semaphore name      */
	if (!qwclsemaphore)
		Sys_Error ("QWCL is already running on this system");
	CloseHandle (qwclsemaphore);

    qwclsemaphore = CreateSemaphore(
        NULL,         /* Security attributes */
        0,            /* Initial count       */
        1,            /* Maximum count       */
        "qwcl"); /* Semaphore name      */
#endif

	if (dedicated->value)
	{
		if (!AllocConsole ())
			Sys_Error ("Couldn't create dedicated server console");
		hinput = GetStdHandle (STD_INPUT_HANDLE);
		houtput = GetStdHandle (STD_OUTPUT_HANDLE);
	
		// let QHOST hook in
		InitConProc (g_argc, g_argv);
	}
}


static char	console_text[256];
static int	console_textlen;

/*
================
Sys_ConsoleInput
================
*/
char *Sys_ConsoleInput (void)
{
	INPUT_RECORD	recs[1024];
	int		dummy;
	int		ch, numread, numevents;

	if (!dedicated || !dedicated->value)
		return NULL;


	for ( ;; )
	{
		if (!GetNumberOfConsoleInputEvents (hinput, &numevents))
			Sys_Error ("Error getting # of console events");

		if (numevents <= 0)
			break;

		if (!ReadConsoleInput(hinput, recs, 1, &numread))
			Sys_Error ("Error reading console input");

		if (numread != 1)
			Sys_Error ("Couldn't read console input");

		if (recs[0].EventType == KEY_EVENT)
		{
			if (!recs[0].Event.KeyEvent.bKeyDown)
			{
				ch = recs[0].Event.KeyEvent.uChar.AsciiChar;

				switch (ch)
				{
					case '\r':
						WriteFile(houtput, "\r\n", 2, &dummy, NULL);	

						if (console_textlen)
						{
							console_text[console_textlen] = 0;
							console_textlen = 0;
							return console_text;
						}
						break;

					case '\b':
						if (console_textlen)
						{
							console_textlen--;
							WriteFile(houtput, "\b \b", 3, &dummy, NULL);	
						}
						break;

					default:
						if (ch >= ' ')
						{
							if (console_textlen < sizeof(console_text)-2)
							{
								WriteFile(houtput, &ch, 1, &dummy, NULL);	
								console_text[console_textlen] = ch;
								console_textlen++;
							}
						}

						break;

				}
			}
		}
	}

	return NULL;
}


/*
================
Sys_ConsoleOutput

Print text to the dedicated console
================
*/
void Sys_ConsoleOutput (char *string)
{
	int		dummy;
	char	text[256];

	if (!dedicated || !dedicated->value)
		return;

	if (console_textlen)
	{
		text[0] = '\r';
		memset(&text[1], ' ', console_textlen);
		text[console_textlen+1] = '\r';
		text[console_textlen+2] = 0;
		WriteFile(houtput, text, console_textlen+2, &dummy, NULL);
	}

	WriteFile(houtput, string, strlen(string), &dummy, NULL);

	if (console_textlen)
		WriteFile(houtput, console_text, console_textlen, &dummy, NULL);
}


/*
================
Sys_SendKeyEvents

Send Key_Event calls
================
*/
void Sys_SendKeyEvents (void)
{
	SDL_PumpEvents();
	sys_frame_time = SDL_GetTicks();
}



/*
================
Sys_GetClipboardData

================
*/
char *Sys_GetClipboardData( void )
{
	char *data = NULL;

	if (SDL_HasClipboardText())
	{
		// TODO: We copy this from the buffer SDL gives us, just so we can SDL_free()
		// it so that all the clients can just do regular free(). Simplify?
		char* clipData = SDL_GetClipboardText();
		if (clipData)
		{
			data = malloc(strlen(clipData) + 1);
			strcpy(data, clipData);
		}
		SDL_free(clipData);
	}
	return data;
}

/*
==============================================================================

 WINDOWS CRAP

==============================================================================
*/

/*
=================
Sys_AppActivate
=================
*/
void Sys_AppActivate (void)
{
	SDL_Window* mainWindow = SDL_GetWindowFromID(0);

	if (mainWindow)
		SDL_RaiseWindow(mainWindow);
}

/*
========================================================================

GAME DLL

========================================================================
*/

static void*	game_library = NULL;

#if defined(__linux__)
	#define LIBRARY_EXTENSION "so"
#elif defined(_WIN32)
	#define LIBRARY_EXTENSION "dll"
#elif defined(__APPLE__)
	#define LIBRARY_EXTENSION "dylib"
#else
	#error Unknown platform, unknown library extension!
#endif

#if defined(_M_IX86) || defined(__i386__)
	#define LIBRARY_ARCH "x86"
#elif defined(_M_ARM) || defined(__arm__)
	#define LIBRARY_ARCH "arm"
#elif defined(_M_X64) || defined(_M_AMD64) || defined(__amd64__) || defined(__x86_64__)
	#define LIBRARY_ARCH "x64"
#elif defined(_M_ALPHA) || defined(__alpha__)
	#define LIBRARY_ARCH "axp"
#elif defined(_M_PPC) || defined(__ppc__)
	#define LIBRARY_ARCH "ppc"
#else
	#error Unknown architecture.
#endif

#define GAME_LIBRARY_NAME "game" LIBRARY_ARCH "." LIBRARY_EXTENSION

/*
=================
Sys_UnloadGame
=================
*/
void Sys_UnloadGame (void)
{
	if (game_library)
		SDL_UnloadObject(game_library);
	game_library = NULL;
}

/*
=================
Sys_GetGameAPI

Loads the game dll
=================
*/
void *Sys_GetGameAPI (void *parms)
{
	void	*(*GetGameAPI) (void *);
	char	name[MAX_OSPATH];
	char	*path;
	char	cwd[MAX_OSPATH];

	const char *gamename = GAME_LIBRARY_NAME;

#ifdef NDEBUG
	const char *debugdir = "release";
#else
	const char *debugdir = "debug";
#endif

	if (game_library)
		Com_Error (ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadingGame");

	// check the current debug directory first for development purposes
	_getcwd (cwd, sizeof(cwd));
	Com_sprintf (name, sizeof(name), "%s/%s/%s", cwd, debugdir, gamename);
	game_library = SDL_LoadObject ( name );
	if (game_library)
	{
		Com_DPrintf ("SDL_LoadObject (%s)\n", name);
	}
	else
	{
#ifdef DEBUG
		// check the current directory for other development purposes
		Com_sprintf (name, sizeof(name), "%s/%s", cwd, gamename);
		game_library = SDL_LoadObject ( name );
		if (game_library)
		{
			Com_DPrintf ("SDL_LoadObject (%s)\n", name);
		}
		else
#endif
		{
			// now run through the search paths
			path = NULL;
			while (1)
			{
				path = FS_NextPath (path);
				if (!path)
					return NULL;		// couldn't find one anywhere
				Com_sprintf (name, sizeof(name), "%s/%s", path, gamename);
				game_library = SDL_LoadObject(name);
				if (game_library)
				{
					Com_DPrintf ("SDL_LoadObject (%s)\n",name);
					break;
				}
			}
		}
	}

	GetGameAPI = (void (*)(void *))SDL_LoadFunction(game_library, "GetGameAPI");
	if (!GetGameAPI)
	{
		Sys_UnloadGame ();		
		return NULL;
	}

	return GetGameAPI (parms);
}

//=======================================================================

/*
==================
Sys_GetSteamDirectory
==================
*/

static size_t QueryRegistryKeyForPath(
	HKEY hive,
	const char* keypath,
	const char* valuename,
	char* outBuffer,
	size_t bufferLength)
{
	HKEY key = INVALID_HANDLE_VALUE;
	DWORD pathtype;
	DWORD pathlen;
	LONG retval;
	size_t result = 0;

	// Open the key.
	if (RegOpenKeyExA(hive, keypath, 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
	{
		// Make sure the value is the right type, and that it exists.
		if (RegQueryValueEx(key, valuename, 0, &pathtype, NULL, &pathlen) == ERROR_SUCCESS)
		{
			if (pathtype == REG_SZ && pathlen != 0)
			{
				// Cool, it does, copy it.

				// RegQueryValueEx can only take a DWORD length size, so if we for some crazy
				// reason want to try and get more than 4GB out of it... cap that.
				if (bufferLength > 0xFFFFFFFEUL)
					pathlen = 0xFFFFFFFFUL;
				else
					pathlen = (DWORD)bufferLength;

				retval = RegQueryValueEx(key, valuename, 0, NULL, (LPBYTE)outBuffer, &pathlen);
				if (retval == ERROR_MORE_DATA)
					result = 0; // What? We created the size it asked for. Give up.
				else
					result = pathlen;
			}
		}
		RegCloseKey(key);
	}

	return result;
}

size_t Sys_GetSteamDirectory(char* steamDirectory, size_t length)
{
	size_t retval;
	retval = QueryRegistryKeyForPath(HKEY_CURRENT_USER, "Software\\Valve\\Steam", "SteamPath", steamDirectory, length);
	if (retval != 0)
		return retval;

	retval = QueryRegistryKeyForPath(HKEY_CURRENT_USER, "Software\\Valve\\Steam", "InstallPath", steamDirectory, length);
	if (retval != 0)
		return retval;

	return 0;
}

/*
==================
WinMain

==================
*/
#if 0
int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    MSG				msg;
	int				time, oldtime, newtime;
	char			*cddir;

    /* previous instances do not exist in Win32 */
    if (hPrevInstance)
        return 0;

	global_hInstance = hInstance;

	ParseCommandLine (lpCmdLine);

	// if we find the CD, add a +set cddir xxx command line
	cddir = Sys_ScanForCD ();
	if (cddir && argc < MAX_NUM_ARGVS - 3)
	{
		int		i;

		// don't override a cddir on the command line
		for (i=0 ; i<argc ; i++)
			if (!strcmp(argv[i], "cddir"))
				break;
		if (i == argc)
		{
			argv[argc++] = "+set";
			argv[argc++] = "cddir";
			argv[argc++] = cddir;
		}
	}

	Qcommon_Init (argc, argv);
	oldtime = Sys_Milliseconds ();

    /* main window message loop */
	while (1)
	{
		// if at a full screen console, don't update unless needed
		if (Minimized || (dedicated && dedicated->value) )
		{
			Sleep (1);
		}

		while (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE))
		{
			if (!GetMessage (&msg, NULL, 0, 0))
				Com_Quit ();
			TranslateMessage (&msg);
   			DispatchMessage (&msg);
		}

		do
		{
			newtime = Sys_Milliseconds ();
			time = newtime - oldtime;
		} while (time < 1);
//			Con_Printf ("time:%5.2f - %5.2f = %5.2f\n", newtime, oldtime, time);

		Qcommon_Frame (time);

		oldtime = newtime;
	}

	// never gets here
    return TRUE;
}
#endif

int main(int argc, char* argv[])
{
	int				time, oldtime, newtime;
	SDL_Event		ev;

	SDL_Init(SDL_INIT_EVERYTHING);

	g_argc = argc;
	g_argv = argv;

	Qcommon_Init(argc, argv);
	oldtime = Sys_Milliseconds();

	/* main window message loop */
	while (1)
	{
		// if at a full screen console, don't update unless needed
		if (Minimized || (dedicated && dedicated->value))
		{
			Sleep(1);
		}

		while (SDL_PollEvent(&ev))
		{
			switch (ev.type)
			{
				case SDL_QUIT:
					Com_Quit();
					return 0;
				case SDL_KEYDOWN:
				case SDL_KEYUP:
					Key_HandleKeyboardEvent(&(ev.key));
					break;
				case SDL_WINDOWEVENT:
					VID_HandleWindowEvent(&(ev.window));
					break;
				case SDL_MOUSEMOTION:
					IN_HandleMouseMotionEvent(&(ev.motion));
					break;
				case SDL_MOUSEBUTTONDOWN:
				case SDL_MOUSEBUTTONUP:
					IN_HandleMouseButtonEvent(&(ev.button));
					break;
				case SDL_MOUSEWHEEL:
					IN_HandleMouseWheelEvent(&(ev.wheel));
					break;
				case SDL_CONTROLLERAXISMOTION:
					IN_HandleControllerAxisEvent(&(ev.caxis));
					break;
				case SDL_CONTROLLERBUTTONDOWN:
				case SDL_CONTROLLERBUTTONUP:
					IN_HandleControllerButtonEvent(&(ev.cbutton));
					break;
				case SDL_CONTROLLERDEVICEADDED:
				case SDL_CONTROLLERDEVICEREMAPPED:
				case SDL_CONTROLLERDEVICEREMOVED:
					IN_HandleControllerDeviceEvent(&(ev.cdevice));
					break;
				default:
					break;
			}
		}

		do
		{
			newtime = Sys_Milliseconds();
			time = newtime - oldtime;
		} while (time < 1);
		//			Con_Printf ("time:%5.2f - %5.2f = %5.2f\n", newtime, oldtime, time);

		Qcommon_Frame(time);

		oldtime = newtime;
	}

	// never gets here
	return 0;
}
