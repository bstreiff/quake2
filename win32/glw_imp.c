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
/*
** GLW_IMP.C
**
** This file contains ALL Win32 specific stuff having to do with the
** OpenGL refresh.  When a port is being made the following functions
** must be implemented by the port:
**
** GLimp_EndFrame
** GLimp_Init
** GLimp_Shutdown
** GLimp_SwitchFullscreen
**
*/
#include <assert.h>
#include <windows.h>
#include "../ref_gl/gl_local.h"
#include "glw_win.h"
#include "winquake.h"
#include "SDL2/SDL.h"

static qboolean GLimp_SwitchFullscreen( int width, int height );
qboolean GLimp_InitGL (void);

glwstate_t glw_state;

extern cvar_t *vid_fullscreen;
extern cvar_t *vid_ref;

static qboolean VerifyDriver( void )
{
	char buffer[1024];

	strcpy( buffer, (const char*)qglGetString( GL_RENDERER ) );
	strlwr( buffer );
	if ( strcmp( buffer, "gdi generic" ) == 0 )
		if ( !glw_state.mcd_accelerated )
			return false;
	return true;
}

/*
** VID_CreateWindow
*/
#define	WINDOW_CLASS_NAME	"Quake 2"

qboolean VID_CreateWindow( int width, int height, qboolean fullscreen )
{
	SDL_Window *win = NULL;
	SDL_WindowFlags windowFlags;
	SDL_RendererFlags renderFlags;

	windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS | SDL_WINDOW_ALLOW_HIGHDPI;

	if (fullscreen)
		windowFlags |= SDL_WINDOW_FULLSCREEN;

	renderFlags = SDL_RENDERER_ACCELERATED;

	win = SDL_CreateWindow(
		WINDOW_CLASS_NAME,
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		width,
		height,
		windowFlags);

	glw_state.window = win;
	glw_state.glContext = SDL_GL_CreateContext(win);

	// let the sound and input subsystems know about the new window
	ri.Vid_NewWindow(width, height);

	return true;
}


/*
** GLimp_SetMode
*/
rserr_t GLimp_SetMode( unsigned int *pwidth, unsigned int *pheight, int mode, qboolean fullscreen )
{
	int width, height;
	const char *win_fs[] = { "W", "FS" };

	ri.Con_Printf( PRINT_ALL, "Initializing OpenGL display\n");

	ri.Con_Printf (PRINT_ALL, "...setting mode %d:", mode );

	if ( !ri.Vid_GetModeInfo( &width, &height, mode ) )
	{
		ri.Con_Printf( PRINT_ALL, " invalid mode\n" );
		return rserr_invalid_mode;
	}

	ri.Con_Printf( PRINT_ALL, " %d %d %s\n", width, height, win_fs[fullscreen] );

	// destroy the existing window
	if (glw_state.window)
	{
		GLimp_Shutdown ();
	}

	*pwidth = width;
	*pheight = height;
	gl_state.fullscreen = fullscreen;
	if (!VID_CreateWindow(width, height, fullscreen))
		return rserr_invalid_mode;

	return rserr_ok;
}

/*
** GLimp_Shutdown
**
** This routine does all OS specific shutdown procedures for the OpenGL
** subsystem.  Under OpenGL this means NULLing out the current DC and
** HGLRC, deleting the rendering context, and releasing the DC acquired
** for the window.  The state structure is also nulled out.
**
*/
void GLimp_Shutdown( void )
{
	if (glw_state.glContext)
	{
		SDL_GL_DeleteContext(glw_state.glContext);
		glw_state.glContext = NULL;
	}

	if (glw_state.window)
	{
		SDL_DestroyWindow(glw_state.window);
		glw_state.window = NULL;
	}

	if ( glw_state.log_fp )
	{
		fclose( glw_state.log_fp );
		glw_state.log_fp = 0;
	}
}

static int VidDefCompare(const void* a, const void* b)
{
	const viddef_t* aa = (const viddef_t*)a;
	const viddef_t* bb = (const viddef_t*)b;

	// Sort by width first...
	if (aa->width != bb->width)
	{
		return (aa->width - bb->width);
	}
	else
	{
		// If width matches, sort by height.
		return (aa->height - bb->height);
	}
}

void GLimp_GetResolutions(void)
{
	int display_count = 0;
	int max_modes = 0;
	SDL_DisplayMode mode_settings = {SDL_PIXELFORMAT_UNKNOWN, 0, 0, 0, 0};
	viddef_t* resolutions = NULL;
	size_t resolutions_count = 0;
	char* resolution_string = NULL;
	size_t resolution_string_len = 0;

	if ((display_count = SDL_GetNumVideoDisplays()) < 1)
		return;

	// First try and figure out an upper bound on number of modes.
	for (int display = 0; display < display_count; ++display)
	{
		int mode_count = 0;
		if ((mode_count = SDL_GetNumDisplayModes(display)) < 1)
			continue;
		max_modes += mode_count;
	}

	resolutions = (viddef_t*)calloc(max_modes, sizeof(viddef_t));
	if (!resolutions)
	{
		ri.Sys_Error(ERR_FATAL, "GetResolutions: alloc failure");
		goto fail;
	}

	for (int display = 0; display < display_count; ++display)
	{
		int mode_count = 0;
		if ((mode_count = SDL_GetNumDisplayModes(display)) < 1)
			continue;

		for (int mode = 0; mode < mode_count; ++mode)
		{
			viddef_t new_res;
			qboolean found_res = false;
			if ((SDL_GetDisplayMode(display, mode, &mode_settings)) != 0)
			continue;

			// Is this a new resolution?
			new_res.width = mode_settings.w;
			new_res.height = mode_settings.h;

			for (int res = 0; res < resolutions_count; ++res)
			{
				if (VidDefCompare(&new_res, &resolutions[res]) == 0)
				{
					found_res = true;
					break;
				}
			}

			if (!found_res)
			{
			resolutions[resolutions_count].width = new_res.width;
			resolutions[resolutions_count].height = new_res.height;
			++resolutions_count;
			}
		}
		max_modes += mode_count;
	}

	// Sort the resolutions.
	qsort(resolutions, resolutions_count, sizeof(viddef_t), VidDefCompare);

	// Now turn them into a string. Conservatively assume ten characters
	// per mode... should be enough for "XXXXXxXXXXX "
	resolution_string_len = 10 * resolutions_count;
	resolution_string = (char*)calloc(resolution_string_len, sizeof(char));
	if (!resolution_string)
	{
		ri.Sys_Error(ERR_FATAL, "GetResolutions: alloc failure");
		goto fail;
	}

	for (int res = 0; res < resolutions_count; ++res)
	{
		char tmp[20] = {0};
		sprintf(tmp, "%s%dx%d",
			(res == 0 ? "" : " "),
			resolutions[res].width,
			resolutions[res].height);
		strcat(resolution_string, tmp);
	}

	// Without breaking the DLL interface, the only way to pass the resolution list
	// back to the main engine is via a cvar...
	ri.Cvar_Set("vid_resolution_list", resolution_string);

fail:
	if (resolution_string)
		free(resolution_string);
	if (resolutions)
		free(resolutions);
	return;
}

/*
** GLimp_Init
**
** This routine is responsible for initializing the OS specific portions
** of OpenGL.  Under Win32 this means dealing with the pixelformats and
** doing the wgl interface stuff.
*/
qboolean GLimp_Init( void *hinstance, void *wndproc )
{
	if (!(SDL_WasInit(SDL_INIT_VIDEO)))
	{
		SDL_InitSubSystem(SDL_INIT_VIDEO);
	}

	GLimp_GetResolutions();

	return true;
}

qboolean GLimp_InitGL (void)
{
	if ( !VerifyDriver() )
	{
		ri.Con_Printf( PRINT_ALL, "GLimp_Init() - no hardware acceleration detected\n" );
		return false;
	}

	return true;
}

/*
** GLimp_BeginFrame
*/
void GLimp_BeginFrame( float camera_separation )
{
	if ( gl_bitdepth->modified )
	{
		if ( gl_bitdepth->value != 0 && !glw_state.allowdisplaydepthchange )
		{
			ri.Cvar_SetValue( "gl_bitdepth", 0 );
			ri.Con_Printf( PRINT_ALL, "gl_bitdepth requires Win95 OSR2.x or WinNT 4.x\n" );
		}
		gl_bitdepth->modified = false;
	}

	if ( camera_separation < 0 && gl_state.stereo_enabled )
	{
		qglDrawBuffer( GL_BACK_LEFT );
	}
	else if ( camera_separation > 0 && gl_state.stereo_enabled )
	{
		qglDrawBuffer( GL_BACK_RIGHT );
	}
	else
	{
		qglDrawBuffer( GL_BACK );
	}
}

/*
** GLimp_EndFrame
** 
** Responsible for doing a swapbuffers and possibly for other stuff
** as yet to be determined.  Probably better not to make this a GLimp
** function and instead do a call to GLimp_SwapBuffers.
*/
void GLimp_EndFrame (void)
{
	int		err;

	err = qglGetError();
	assert( err == GL_NO_ERROR );

	if ( stricmp( gl_drawbuffer->string, "GL_BACK" ) == 0 )
	{
		//if ( !qwglSwapBuffers( glw_state.hDC ) )
		//	ri.Sys_Error( ERR_FATAL, "GLimp_EndFrame() - SwapBuffers() failed!\n" );
		SDL_GL_SwapWindow(glw_state.window);
	}
}

/*
** GLimp_AppActivate
*/
void GLimp_AppActivate( qboolean active )
{
	if ( active )
	{
		//SetForegroundWindow( glw_state.hWnd );
		//ShowWindow( glw_state.hWnd, SW_RESTORE );
		SDL_RaiseWindow(glw_state.window);
	}
	else
	{
		if (vid_fullscreen->value)
			//ShowWindow( glw_state.hWnd, SW_MINIMIZE );
			SDL_MinimizeWindow(glw_state.window);
	}
}
