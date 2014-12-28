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
#include "../client/qmenu.h"

#define REF_OPENGL	0
#define REF_3DFX	1
#define REF_POWERVR	2
#define REF_VERITE	3

extern cvar_t *vid_ref;
extern cvar_t *vid_fullscreen;
extern cvar_t *vid_gamma;
extern cvar_t *vid_hudscale;
extern cvar_t *vid_width;
extern cvar_t *vid_height;

static cvar_t *gl_mode;
static cvar_t *gl_driver;
static cvar_t *gl_picmip;
static cvar_t *gl_ext_palettedtexture;
static cvar_t *gl_finish;

extern void M_ForceMenuOff( void );
extern qboolean VID_GetModeInfo(int *width, int *height, int mode);

/*
====================================================================

MENU INTERACTION

====================================================================
*/
#define OPENGL_MENU   0

static menuframework_s	s_opengl_menu;
static menuframework_s *s_current_menu;
static int				s_current_menu_index;

static menulist_s		s_mode_list[1];
static menulist_s		s_ref_list[1];
static menuslider_s		s_tq_slider;
static menuslider_s		s_brightness_slider[1];
static menulist_s  		s_fs_box[1];
static menulist_s  		s_stipple_box;
static menulist_s  		s_paletted_texture_box;
static menulist_s  		s_finish_box;
static menuaction_s		s_cancel_action[1];
static menuaction_s		s_defaults_action[1];

static viddef_t*        s_possible_resolutions_list = NULL;
static size_t           s_possible_resolutions_list_count = 0;
static char**           s_possible_resolutions_str_list = NULL;
static void VID_MenuDestroy();

static void DriverCallback( void *unused )
{
	s_current_menu = &s_opengl_menu;
	s_current_menu_index = 1;
}

static void ScreenSizeCallback( void *s )
{
	menuslider_s *slider = ( menuslider_s * ) s;

	Cvar_SetValue( "viewsize", slider->curvalue * 10 );
}

static void BrightnessCallback( void *s )
{
}

static void ResetDefaults( void *unused )
{
	VID_MenuInit();
}

static void ApplyChanges( void *unused )
{
	float gamma;

	/*
	** make values consistent
	*/
	s_fs_box[!s_current_menu_index].curvalue = s_fs_box[s_current_menu_index].curvalue;
	s_brightness_slider[!s_current_menu_index].curvalue = s_brightness_slider[s_current_menu_index].curvalue;
	s_ref_list[!s_current_menu_index].curvalue = s_ref_list[s_current_menu_index].curvalue;

	/*
	** invert sense so greater = brighter, and scale to a range of 0.5 to 1.3
	*/
	gamma = ( 0.8 - ( s_brightness_slider[s_current_menu_index].curvalue/10.0 - 0.5 ) ) + 0.5;

	Cvar_SetValue( "vid_gamma", gamma );
	Cvar_SetValue( "gl_picmip", 3 - s_tq_slider.curvalue );
	Cvar_SetValue( "vid_fullscreen", s_fs_box[s_current_menu_index].curvalue );
	Cvar_SetValue( "gl_ext_palettedtexture", s_paletted_texture_box.curvalue );
	Cvar_SetValue( "gl_finish", s_finish_box.curvalue );
   Cvar_SetValue( "gl_mode", -1);
   Cvar_SetValue( "vid_height", s_possible_resolutions_list[s_mode_list[OPENGL_MENU].curvalue].height);
   Cvar_SetValue( "vid_width", s_possible_resolutions_list[s_mode_list[OPENGL_MENU].curvalue].width);

	switch ( s_ref_list[s_current_menu_index].curvalue )
	{
	case REF_OPENGL:
		Cvar_Set( "vid_ref", "gl" );
		Cvar_Set( "gl_driver", "opengl32" );
		break;
	case REF_3DFX:
		Cvar_Set( "vid_ref", "gl" );
		Cvar_Set( "gl_driver", "3dfxgl" );
		break;
	case REF_POWERVR:
		Cvar_Set( "vid_ref", "gl" );
		Cvar_Set( "gl_driver", "pvrgl" );
		break;
	case REF_VERITE:
		Cvar_Set( "vid_ref", "gl" );
		Cvar_Set( "gl_driver", "veritegl" );
		break;
	}

	/*
	** update appropriate stuff if we're running OpenGL and gamma
	** has been modified
	*/
	if ( stricmp( vid_ref->string, "gl" ) == 0 )
	{
		if ( vid_gamma->modified )
		{
			vid_ref->modified = true;
			if ( stricmp( gl_driver->string, "3dfxgl" ) == 0 )
			{
				char envbuffer[1024];
				float g;

				vid_ref->modified = true;

				g = 2.00 * ( 0.8 - ( vid_gamma->value - 0.5 ) ) + 1.0F;
				Com_sprintf( envbuffer, sizeof(envbuffer), "SSTV2_GAMMA=%f", g );
				putenv( envbuffer );
				Com_sprintf( envbuffer, sizeof(envbuffer), "SST_GAMMA=%f", g );
				putenv( envbuffer );

				vid_gamma->modified = false;
			}
		}

		if ( gl_driver->modified )
			vid_ref->modified = true;
	}

   VID_MenuDestroy();
	M_ForceMenuOff();
}

void M_PopMenu(void);
static void CancelChanges( void *unused )
{
   VID_MenuDestroy();
	M_PopMenu();
}

static void VID_DestroyResolutionList();

static void VID_CreateResolutionList()
{
   cvar_t* vid_resolution_list = Cvar_Get("vid_resolution_list", "640x480 800x600 1024x768 1152x864 1280x960 1600x1200", 0);
   char* itr = vid_resolution_list->string;
   int res_count = 0;

   // Make sure the list is cleared out before we start...
   VID_DestroyResolutionList();

   // Count number of spaces in the string. This is number of resolutions, minus one.
   while (*itr != '\0')
   {
      if (*itr == ' ')
         res_count++;
      ++itr;
   }
   res_count++;

   s_possible_resolutions_list_count = res_count;
   s_possible_resolutions_list = (viddef_t*)calloc(res_count, sizeof(viddef_t));
   if (!s_possible_resolutions_list)
   {
      Sys_Error(ERR_FATAL, "VID_CreateResolutionList: alloc failure");
      return;
   }

   // Move itr back to start of string
   itr = vid_resolution_list->string;

   for (int res = 0; res < res_count; ++res)
   {
      long int width, height;

      width = strtol(itr, &itr, 10);
      ++itr; // skip over "x"
      height = strtol(itr, &itr, 10);

      s_possible_resolutions_list[res].height = (int)height;
      s_possible_resolutions_list[res].width = (int)width;
   }

   // Now, convert the resolution list into strings. Need +1 for terminator.
   s_possible_resolutions_str_list = (char**)calloc(res_count+1, sizeof(char*));
   if (!s_possible_resolutions_str_list)
   {
      Sys_Error(ERR_FATAL, "VID_CreateResolutionList: alloc failure");
      return;
   }

   for (int res = 0; res < res_count; ++res)
   {
      const int field_width = 14;

      s_possible_resolutions_str_list[res] = (char*)calloc(field_width, sizeof(char));
      if (!s_possible_resolutions_str_list[res])
      {
         Sys_Error(ERR_FATAL, "VID_CreateResolutionList: alloc failure");
         return;
      }

      Com_sprintf(s_possible_resolutions_str_list[res], field_width-1, "[%d %d",
         s_possible_resolutions_list[res].width,
         s_possible_resolutions_list[res].height);

      // From right, add the ']' and pad with spaces.
      s_possible_resolutions_str_list[res][field_width-1] = '\0';
      s_possible_resolutions_str_list[res][field_width-2] = ']';
      for (int i = field_width-3; i > 0; --i)
      {
         if (s_possible_resolutions_str_list[res][i] == '\0')
            s_possible_resolutions_str_list[res][i] = ' ';
      }
   }
}

static int VID_FindIndexForResolution(int width, int height)
{
   // First, look for an exact match.
   for (int res = 0; res < s_possible_resolutions_list_count; ++res)
   {
      if (s_possible_resolutions_list[res].width == width &&
          s_possible_resolutions_list[res].height == height)
      {
         return res;
      }
   }

   // No exact match, just pick the first one.
   return 0;
}

static void VID_DestroyResolutionList()
{
   if (s_possible_resolutions_list)
   {
      free(s_possible_resolutions_list);
      s_possible_resolutions_list = NULL;
      s_possible_resolutions_list_count = 0;
   }
   if (s_possible_resolutions_str_list)
   {
      char** itr = s_possible_resolutions_str_list;
      while (*itr != NULL)
      {
         free(*itr);
         *itr = NULL;
         ++itr;
      }
      free(s_possible_resolutions_str_list);
      s_possible_resolutions_str_list = NULL;
   }
}

/*
** VID_MenuInit
*/
void VID_MenuInit( void )
{
   const float scale = vid_hudscale->value;

	static const char *refs[] =
	{
		"[default OpenGL]",
		"[3Dfx OpenGL   ]",
		"[PowerVR OpenGL]",
//		"[Rendition OpenGL]",
		0
	};
	static const char *yesno_names[] =
	{
		"no",
		"yes",
		0
	};
	int i;
   int width;
   int height;

   VID_CreateResolutionList();
   
   if ( !gl_driver )
		gl_driver = Cvar_Get( "gl_driver", "opengl32", 0 );
	if ( !gl_picmip )
		gl_picmip = Cvar_Get( "gl_picmip", "0", 0 );
	if ( !gl_mode )
		gl_mode = Cvar_Get( "gl_mode", "3", 0 );
	if ( !gl_ext_palettedtexture )
		gl_ext_palettedtexture = Cvar_Get( "gl_ext_palettedtexture", "1", CVAR_ARCHIVE );
	if ( !gl_finish )
		gl_finish = Cvar_Get( "gl_finish", "0", CVAR_ARCHIVE );

   // Get current width and height.
   VID_GetModeInfo(&width, &height, gl_mode->value);
   s_mode_list[OPENGL_MENU].curvalue = VID_FindIndexForResolution(width, height);
   
	if ( strcmp( vid_ref->string, "gl" ) == 0 )
	{
		s_current_menu_index = OPENGL_MENU;
		if ( strcmp( gl_driver->string, "3dfxgl" ) == 0 )
			s_ref_list[s_current_menu_index].curvalue = REF_3DFX;
		else if ( strcmp( gl_driver->string, "pvrgl" ) == 0 )
			s_ref_list[s_current_menu_index].curvalue = REF_POWERVR;
		else if ( strcmp( gl_driver->string, "opengl32" ) == 0 )
			s_ref_list[s_current_menu_index].curvalue = REF_OPENGL;
		else
//			s_ref_list[s_current_menu_index].curvalue = REF_VERITE;
			s_ref_list[s_current_menu_index].curvalue = REF_OPENGL;
	}

	s_opengl_menu.x = viddef.width * 0.50;
	s_opengl_menu.nitems = 0;

	for ( i = 0; i < 1; i++ )
	{
		s_ref_list[i].generic.type = MTYPE_SPINCONTROL;
		s_ref_list[i].generic.name = "driver";
		s_ref_list[i].generic.x = 0;
		s_ref_list[i].generic.y = 0;
		s_ref_list[i].generic.callback = DriverCallback;
		s_ref_list[i].itemnames = refs;

		s_mode_list[i].generic.type = MTYPE_SPINCONTROL;
		s_mode_list[i].generic.name = "video mode";
		s_mode_list[i].generic.x = 0;
		s_mode_list[i].generic.y = 10*scale;
		s_mode_list[i].itemnames = s_possible_resolutions_str_list;

		s_brightness_slider[i].generic.type	= MTYPE_SLIDER;
		s_brightness_slider[i].generic.x	= 0;
		s_brightness_slider[i].generic.y	= 30*scale;
		s_brightness_slider[i].generic.name	= "brightness";
		s_brightness_slider[i].generic.callback = BrightnessCallback;
		s_brightness_slider[i].minvalue = 5;
		s_brightness_slider[i].maxvalue = 13;
		s_brightness_slider[i].curvalue = ( 1.3 - vid_gamma->value + 0.5 ) * 10;

		s_fs_box[i].generic.type = MTYPE_SPINCONTROL;
		s_fs_box[i].generic.x	= 0;
		s_fs_box[i].generic.y	= 40*scale;
		s_fs_box[i].generic.name	= "fullscreen";
		s_fs_box[i].itemnames = yesno_names;
		s_fs_box[i].curvalue = vid_fullscreen->value;

		s_defaults_action[i].generic.type = MTYPE_ACTION;
		s_defaults_action[i].generic.name = "reset to defaults";
		s_defaults_action[i].generic.x    = 0;
		s_defaults_action[i].generic.y    = 90*scale;
		s_defaults_action[i].generic.callback = ResetDefaults;

		s_cancel_action[i].generic.type = MTYPE_ACTION;
		s_cancel_action[i].generic.name = "cancel";
		s_cancel_action[i].generic.x    = 0;
		s_cancel_action[i].generic.y    = 100*scale;
		s_cancel_action[i].generic.callback = CancelChanges;
	}

	s_tq_slider.generic.type	= MTYPE_SLIDER;
	s_tq_slider.generic.x		= 0;
	s_tq_slider.generic.y		= 60*scale;
	s_tq_slider.generic.name	= "texture quality";
	s_tq_slider.minvalue = 0;
	s_tq_slider.maxvalue = 3;
	s_tq_slider.curvalue = 3-gl_picmip->value;

	s_paletted_texture_box.generic.type = MTYPE_SPINCONTROL;
	s_paletted_texture_box.generic.x	= 0;
	s_paletted_texture_box.generic.y	= 70*scale;
	s_paletted_texture_box.generic.name	= "8-bit textures";
	s_paletted_texture_box.itemnames = yesno_names;
	s_paletted_texture_box.curvalue = gl_ext_palettedtexture->value;

	s_finish_box.generic.type = MTYPE_SPINCONTROL;
	s_finish_box.generic.x	= 0;
	s_finish_box.generic.y	= 80*scale;
	s_finish_box.generic.name	= "sync every frame";
	s_finish_box.curvalue = gl_finish->value;
	s_finish_box.itemnames = yesno_names;

	Menu_AddItem( &s_opengl_menu, ( void * ) &s_ref_list[OPENGL_MENU] );
	Menu_AddItem( &s_opengl_menu, ( void * ) &s_mode_list[OPENGL_MENU] );
	Menu_AddItem( &s_opengl_menu, ( void * ) &s_brightness_slider[OPENGL_MENU] );
	Menu_AddItem( &s_opengl_menu, ( void * ) &s_fs_box[OPENGL_MENU] );
	Menu_AddItem( &s_opengl_menu, ( void * ) &s_tq_slider );
	Menu_AddItem( &s_opengl_menu, ( void * ) &s_paletted_texture_box );
	Menu_AddItem( &s_opengl_menu, ( void * ) &s_finish_box );

	Menu_AddItem( &s_opengl_menu, ( void * ) &s_defaults_action[OPENGL_MENU] );
	Menu_AddItem( &s_opengl_menu, ( void * ) &s_cancel_action[OPENGL_MENU] );

	Menu_Center( &s_opengl_menu );
	s_opengl_menu.x -= 8;
}

static void VID_MenuDestroy()
{
   VID_DestroyResolutionList();
}

/*
================
VID_MenuDraw
================
*/
void VID_MenuDraw (void)
{
	int w, h;

	s_current_menu = &s_opengl_menu;

	/*
	** draw the banner
	*/
	re.DrawGetPicSize( &w, &h, "m_banner_video" );
	re.DrawPic( viddef.width / 2 - w / 2, viddef.height /2 - 110*vid_hudscale->value, "m_banner_video" );

	/*
	** move cursor to a reasonable starting position
	*/
	Menu_AdjustCursor( s_current_menu, 1 );

	/*
	** draw the menu
	*/
	Menu_Draw( s_current_menu );
}

/*
================
VID_MenuKey
================
*/
const char *VID_MenuKey( int key )
{
	menuframework_s *m = s_current_menu;
	static const char *sound = "misc/menu1.wav";

	switch ( key )
	{
	case K_ESCAPE:
		ApplyChanges( 0 );
		return NULL;
	case K_KP_UPARROW:
	case K_UPARROW:
		m->cursor--;
		Menu_AdjustCursor( m, -1 );
		break;
	case K_KP_DOWNARROW:
	case K_DOWNARROW:
		m->cursor++;
		Menu_AdjustCursor( m, 1 );
		break;
	case K_KP_LEFTARROW:
	case K_LEFTARROW:
		Menu_SlideItem( m, -1 );
		break;
	case K_KP_RIGHTARROW:
	case K_RIGHTARROW:
		Menu_SlideItem( m, 1 );
		break;
	case K_KP_ENTER:
	case K_ENTER:
		if ( !Menu_SelectItem( m ) )
			ApplyChanges( NULL );
		break;
	}

	return sound;
}


