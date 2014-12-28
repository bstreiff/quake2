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
// r_misc.c

#include "gl_local.h"
#include <png.h>
#include <zlib.h>
#include "SDL.h"

/*
==================
R_InitParticleTexture
==================
*/
byte	dottexture[8][8] =
{
	{0,0,0,0,0,0,0,0},
	{0,0,1,1,0,0,0,0},
	{0,1,1,1,1,0,0,0},
	{0,1,1,1,1,0,0,0},
	{0,0,1,1,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
};

void R_InitParticleTexture (void)
{
	int		x,y;
	byte	data[8][8][4];

	//
	// particle texture
	//
	for (x=0 ; x<8 ; x++)
	{
		for (y=0 ; y<8 ; y++)
		{
			data[y][x][0] = 255;
			data[y][x][1] = 255;
			data[y][x][2] = 255;
			data[y][x][3] = dottexture[x][y]*255;
		}
	}
	r_particletexture = GL_LoadPic ("***particle***", (byte *)data, 8, 8, it_sprite, 32);

	//
	// also use this for bad textures, but without alpha
	//
	for (x=0 ; x<8 ; x++)
	{
		for (y=0 ; y<8 ; y++)
		{
			data[y][x][0] = dottexture[x&3][y&3]*255;
			data[y][x][1] = 0; // dottexture[x&3][y&3]*255;
			data[y][x][2] = 0; //dottexture[x&3][y&3]*255;
			data[y][x][3] = 255;
		}
	}
	r_notexture = GL_LoadPic ("***r_notexture***", (byte *)data, 8, 8, it_wall, 32);
}


/* 
============================================================================== 
 
						SCREEN SHOTS 
 
============================================================================== 
*/ 

/* 
================== 
GL_ScreenShot_f
================== 
*/  
void GL_ScreenShot_f (void) 
{
	png_byte*	buffer;
	char		filename[MAX_OSPATH] = {0};
	char		timestamp[MAX_OSPATH] = {0};
	FILE*		f;
	png_structp	png = NULL;
	png_infop	pnginfo = NULL;
	png_byte**	row_pointers = NULL;
	struct timeval tv;
	struct tm	tm;
	time_t		now;

	// create the scrnshots directory if it doesn't exist
	Com_sprintf(filename, sizeof(filename), "%s/scrnshot", ri.FS_Gamedir());
	Sys_Mkdir(filename);

	Sys_GetTimeOfDay(&tv);
	now = tv.tv_sec;
#if defined(WIN32)
	localtime_s(&tm, &now);
#else
	localtime_r(&now, &tm);
#endif
	strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", &tm);

	// strftime doesn't handle fractional seconds, we add msec ourselves.
	Com_sprintf(filename, sizeof(filename),
		"%s/scrnshot/quake_%s_%03d.png",
		ri.FS_Gamedir(), timestamp, (tv.tv_usec / 1000));

	f = fopen(filename, "wb");
	if (!f)
	{
		ri.Con_Printf(PRINT_ALL, "SCR_ScreenShot_f: Couldn't create a file\n");
		goto fopen_failed;
	}

   png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png)
	{
		ri.Con_Printf(PRINT_ALL, "SCR_ScreenShot_f: Unable to create PNG.\n");
		goto png_create_write_struct_failed;
	}

	pnginfo = png_create_info_struct(png);
	if (!pnginfo)
	{
		ri.Con_Printf(PRINT_ALL, "SCR_ScreenShot_f: Unable to create PNG info pointer.\n");
		goto png_create_info_struct_failed;
	}

	png_set_IHDR(
		png, pnginfo,
		vid.width, vid.height, 8,
		PNG_COLOR_TYPE_RGB,
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT);

	// Get the pixels.
	buffer = (png_byte*)png_malloc(png, vid.width*vid.height*3);
	if (!buffer)
	{
		ri.Con_Printf(PRINT_ALL, "SCR_ScreenShot_f: Unable to alloc memory for buffer.\n");
		goto alloc_buffer_failed;
	}
	qglReadPixels (0, 0, vid.width, vid.height, GL_RGB, GL_UNSIGNED_BYTE, buffer); 

	// We need to build an array of row pointers.
	row_pointers = (png_byte**)png_malloc(png, vid.height * sizeof(png_byte*));
	if (!row_pointers)
	{
		ri.Con_Printf(PRINT_ALL, "SCR_ScreenShot_f: Unable to alloc memory for buffer.\n");
		goto alloc_row_pointers_failed;
	}
	// OpenGL returns the buffer flipped from the way we want to write it, so set
	// up the row pointers into the buffer in reverse.
	for (int i = 0; i < vid.height; ++i)
	{
		row_pointers[i] = buffer + (vid.height-i-1)*(vid.width*3);
	}

	png_init_io(png, f);
	png_set_rows(png, pnginfo, row_pointers);
	png_write_png(png, pnginfo, PNG_TRANSFORM_IDENTITY, NULL);

	ri.Con_Printf(PRINT_ALL, "Wrote %s\n", filename);

	png_free(png, row_pointers);
	row_pointers = NULL;
alloc_row_pointers_failed:
	png_free(png, buffer);
	buffer = NULL;
alloc_buffer_failed:
png_create_info_struct_failed:
	png_destroy_write_struct(&png, &pnginfo);
png_create_write_struct_failed:
	fclose(f);
fopen_failed:
	return;
} 

/*
** GL_Strings_f
*/
void GL_Strings_f( void )
{
	ri.Con_Printf (PRINT_ALL, "GL_VENDOR: %s\n", gl_config.vendor_string );
	ri.Con_Printf (PRINT_ALL, "GL_RENDERER: %s\n", gl_config.renderer_string );
	ri.Con_Printf (PRINT_ALL, "GL_VERSION: %s\n", gl_config.version_string );
	ri.Con_Printf (PRINT_ALL, "GL_EXTENSIONS: %s\n", gl_config.extensions_string );
}

/*
** GL_SetDefaultState
*/
void GL_SetDefaultState( void )
{
	qglClearColor (1,0, 0.5 , 0.5);
	qglCullFace(GL_FRONT);
	qglEnable(GL_TEXTURE_2D);

	qglEnable(GL_ALPHA_TEST);
	qglAlphaFunc(GL_GREATER, 0.666);

	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_CULL_FACE);
	qglDisable (GL_BLEND);

	qglColor4f (1,1,1,1);

	qglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	qglShadeModel (GL_FLAT);

	GL_TextureMode( gl_texturemode->string );
	GL_TextureAlphaMode( gl_texturealphamode->string );
	GL_TextureSolidMode( gl_texturesolidmode->string );

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	GL_TexEnv( GL_REPLACE );

	if ( qglPointParameterfEXT )
	{
		float attenuations[3];

		attenuations[0] = gl_particle_att_a->value;
		attenuations[1] = gl_particle_att_b->value;
		attenuations[2] = gl_particle_att_c->value;

		qglEnable( GL_POINT_SMOOTH );
		qglPointParameterfEXT( GL_POINT_SIZE_MIN_EXT, gl_particle_min_size->value );
		qglPointParameterfEXT( GL_POINT_SIZE_MAX_EXT, gl_particle_max_size->value );
		qglPointParameterfvEXT( GL_DISTANCE_ATTENUATION_EXT, attenuations );
	}

	GL_UpdateSwapInterval();
}

void GL_UpdateSwapInterval( void )
{
	if ( gl_swapinterval->modified )
	{
		gl_swapinterval->modified = false;

		if ( !gl_state.stereo_enabled ) 
		{
#ifdef _WIN32
			SDL_GL_SetSwapInterval(gl_swapinterval->value);
#endif
		}
	}
}