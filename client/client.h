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
// client.h -- primary header for client

//define	PARANOID			// speed sapping error checking

#include <math.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "ref.h"

#include "vid.h"
#include "screen.h"
#include "sound.h"
#include "input.h"
#include "keys.h"
#include "console.h"
#include "cdaudio.h"

//=============================================================================

typedef struct
{
	qboolean		valid;			// cleared if delta parsing was invalid
	int				serverframe;
	int				servertime;		// server time the message is valid for (in msec)
	int				deltaframe;
	byte			areabits[MAX_MAP_AREAS/8];		// portalarea visibility bits
	player_state_t	playerstate;
	int				num_entities;
	int				parse_entities;	// non-masked index into cl_parse_entities array
} frame_t;

typedef struct
{
	entity_state_t	baseline;		// delta from this if not from a previous frame
	entity_state_t	current;
	entity_state_t	prev;			// will always be valid, but might just be a copy of current

	int			serverframe;		// if not current, this ent isn't in the frame

	int			trailcount;			// for diminishing grenade trails
	vec3_t		lerp_origin;		// for trails (variable hz)

	int			fly_stoptime;
} centity_t;

#define MAX_CLIENTWEAPONMODELS		20		// PGM -- upped from 16 to fit the chainfist vwep

typedef struct
{
	char	name[MAX_QPATH];
	char	cinfo[MAX_QPATH];
	struct image_s	*skin;
	struct image_s	*icon;
	char	iconname[MAX_QPATH];
	struct model_s	*model;
	struct model_s	*weaponmodel[MAX_CLIENTWEAPONMODELS];
} clientinfo_t;

extern char cl_weaponmodels[MAX_CLIENTWEAPONMODELS][MAX_QPATH];
extern int num_cl_weaponmodels;

#define	CMD_BACKUP		64	// allow a lot of command backups for very fast systems

//
// the client_state_t structure is wiped completely at every
// server map change
//
typedef struct
{
	int			timeoutcount;

	int			timedemo_frames;
	int			timedemo_start;

	qboolean	refresh_prepped;	// false if on new level or new ref dll
	qboolean	sound_prepped;		// ambient sounds can start
	qboolean	force_refdef;		// vid has changed, so we can't use a paused refdef

	int			parse_entities;		// index (not anded off) into cl_parse_entities[]

	usercmd_t	cmd;
	usercmd_t	cmds[CMD_BACKUP];	// each mesage will send several old cmds
	int			cmd_time[CMD_BACKUP];	// time sent, for calculating pings
	short		predicted_origins[CMD_BACKUP][3];	// for debug comparing against server

	float		predicted_step;				// for stair up smoothing
	unsigned	predicted_step_time;

	vec3_t		predicted_origin;	// generated by CL_PredictMovement
	vec3_t		predicted_angles;
	vec3_t		prediction_error;

	frame_t		frame;				// received from server
	int			surpressCount;		// number of messages rate supressed
	frame_t		frames[UPDATE_BACKUP];

	// the client maintains its own idea of view angles, which are
	// sent to the server each frame.  It is cleared to 0 upon entering each level.
	// the server sends a delta each frame which is added to the locally
	// tracked view angles to account for standing on rotating objects,
	// and teleport direction changes
	vec3_t		viewangles;

	int			time;			// this is the time value that the client
								// is rendering at.  always <= cls.realtime
	float		lerpfrac;		// between oldframe and frame

	refdef_t	refdef;

	vec3_t		v_forward, v_right, v_up;	// set when refdef.angles is set

	//
	// transient data from server
	//
	char		layout[1024];		// general 2D overlay
	int			inventory[MAX_ITEMS];

	//
	// non-gameserver infornamtion
	// FIXME: move this cinematic stuff into the cin_t structure
	FILE		*cinematic_file;
	int			cinematictime;		// cls.realtime for first cinematic frame
	int			cinematicframe;
	char		cinematicpalette[768];
	qboolean	cinematicpalette_active;

	//
	// server state information
	//
	qboolean	attractloop;		// running the attract loop, any key will menu
	int			servercount;	// server identification for prespawns
	char		gamedir[MAX_QPATH];
	int			playernum;

	char		configstrings[MAX_CONFIGSTRINGS][MAX_QPATH];

	//
	// locally derived information from server state
	//
	struct model_s	*model_draw[MAX_MODELS];
	struct cmodel_s	*model_clip[MAX_MODELS];

	struct sfx_s	*sound_precache[MAX_SOUNDS];
	struct image_s	*image_precache[MAX_IMAGES];

	clientinfo_t	clientinfo[MAX_CLIENTS];
	clientinfo_t	baseclientinfo;
} client_state_t;

extern	client_state_t	cl;

/*
==================================================================

the client_static_t structure is persistant through an arbitrary number
of server connections

==================================================================
*/

typedef enum {
	ca_uninitialized,
	ca_disconnected, 	// not talking to a server
	ca_connecting,		// sending request packets to the server
	ca_connected,		// netchan_t established, waiting for svc_serverdata
	ca_active			// game views should be displayed
} connstate_t;

typedef enum {
	dl_none,
	dl_model,
	dl_sound,
	dl_skin,
	dl_single
} dltype_t;		// download type

typedef enum {key_game, key_console, key_message, key_menu} keydest_t;

typedef struct
{
	connstate_t	state;
	keydest_t	key_dest;

	int			framecount;
	int			realtime;			// always increasing, no clamping, etc
	float		frametime;			// seconds since last frame

// screen rendering information
	float		disable_screen;		// showing loading plaque between levels
									// or changing rendering dlls
									// if time gets > 30 seconds ahead, break it
	int			disable_servercount;	// when we receive a frame and cl.servercount
									// > cls.disable_servercount, clear disable_screen

// connection information
	char		servername[MAX_OSPATH];	// name of server from original connect
	float		connect_time;		// for connection retransmits

	int			quakePort;			// a 16 bit value that allows quake servers
									// to work around address translating routers
	netchan_t	netchan;
	int			serverProtocol;		// in case we are doing some kind of version hack

	int			challenge;			// from the server to use for connecting

	FILE		*download;			// file transfer from server
	char		downloadtempname[MAX_OSPATH];
	char		downloadname[MAX_OSPATH];
	int			downloadnumber;
	dltype_t	downloadtype;
	int			downloadpercent;

// demo recording info must be here, so it isn't cleared on level change
	qboolean	demorecording;
	qboolean	demowaiting;	// don't record until a non-delta message is received
	FILE		*demofile;
} client_static_t;

extern client_static_t	cls;

//=============================================================================

//
// cvars
//
extern	cvar_t	*cl_stereo_separation;
extern	cvar_t	*cl_stereo;

extern	cvar_t	*cl_gun;
extern	cvar_t	*cl_add_blend;
extern	cvar_t	*cl_add_lights;
extern	cvar_t	*cl_add_particles;
extern	cvar_t	*cl_add_entities;
extern	cvar_t	*cl_predict;
extern	cvar_t	*cl_footsteps;
extern	cvar_t	*cl_noskins;
extern	cvar_t	*cl_autoskins;

extern	cvar_t	*cl_upspeed;
extern	cvar_t	*cl_forwardspeed;
extern	cvar_t	*cl_sidespeed;

extern	cvar_t	*cl_yawspeed;
extern	cvar_t	*cl_pitchspeed;

extern	cvar_t	*cl_run;

extern	cvar_t	*cl_anglespeedkey;

extern	cvar_t	*cl_shownet;
extern	cvar_t	*cl_showmiss;
extern	cvar_t	*cl_showclamp;

extern	cvar_t	*lookspring;
extern	cvar_t	*lookstrafe;
extern	cvar_t	*sensitivity;

extern	cvar_t	*m_pitch;
extern	cvar_t	*m_yaw;
extern	cvar_t	*m_forward;
extern	cvar_t	*m_side;

extern	cvar_t	*freelook;

extern	cvar_t	*cl_lightlevel;	// FIXME HACK

extern	cvar_t	*cl_paused;
extern	cvar_t	*cl_timedemo;

extern	cvar_t	*cl_vwep;

typedef struct
{
	int		key;				// so entities can reuse same entry
	vec3_t	color;
	vec3_t	origin;
	float	radius;
	float	die;				// stop lighting after this time
	float	decay;				// drop this each second
	float	minlight;			// don't add when contributing less
} cdlight_t;

extern	centity_t	cl_entities[MAX_EDICTS];
extern	cdlight_t	cl_dlights[MAX_DLIGHTS];

// the cl_parse_entities must be large enough to hold UPDATE_BACKUP frames of
// entities, so that when a delta compressed message arives from the server
// it can be un-deltad from the original 
#define	MAX_PARSE_ENTITIES	1024
extern	entity_state_t	cl_parse_entities[MAX_PARSE_ENTITIES];

//=============================================================================

extern	netadr_t	net_from;
extern	sizebuf_t	net_message;

void DrawString (int x, int y, char *s);
void DrawAltString (int x, int y, char *s);	// toggle high bit
qboolean	CL_CheckOrDownloadFile (char *filename);

void CL_AddNetgraph (void);

//ROGUE
typedef struct cl_sustain
{
	int			id;
	int			type;
	int			endtime;
	int			nextthink;
	int			thinkinterval;
	vec3_t		org;
	vec3_t		dir;
	int			color;
	int			count;
	int			magnitude;
	void		(*think)(struct cl_sustain *self);
} cl_sustain_t;

#define MAX_SUSTAINS		32
void CL_ParticleSteamEffect2(cl_sustain_t *self);

void CL_TeleporterParticles (entity_state_t *ent);
void CL_ParticleEffect (vec3_t org, vec3_t dir, int color, int count);
void CL_ParticleEffect2 (vec3_t org, vec3_t dir, int color, int count);

// RAFAEL
void CL_ParticleEffect3 (vec3_t org, vec3_t dir, int color, int count);


//=================================================

// ========
// PGM
typedef struct particle_s
{
	struct particle_s	*next;

	float		time;

	vec3_t		org;
	vec3_t		vel;
	vec3_t		accel;
	float		color;
	float		colorvel;
	float		alpha;
	float		alphavel;
} cparticle_t;


#define	PARTICLE_GRAVITY	40
#define BLASTER_PARTICLE_COLOR		0xe0
// PMM
#define INSTANT_PARTICLE	-10000.0
// PGM
// ========

void CL_ClearEffects (void);
void CL_ClearTEnts (void);
void CL_BlasterTrail (vec3_t start, vec3_t end);
void CL_QuadTrail (vec3_t start, vec3_t end);
void CL_RailTrail (vec3_t start, vec3_t end);
void CL_BubbleTrail (vec3_t start, vec3_t end);
void CL_FlagTrail (vec3_t start, vec3_t end, float color);

// RAFAEL
void CL_IonripperTrail (vec3_t start, vec3_t end);

// ========
// PGM
void CL_BlasterParticles2 (vec3_t org, vec3_t dir, unsigned int color);
void CL_BlasterTrail2 (vec3_t start, vec3_t end);
void CL_DebugTrail (vec3_t start, vec3_t end);
void CL_SmokeTrail (vec3_t start, vec3_t end, int colorStart, int colorRun, int spacing);
void CL_Flashlight (int ent, vec3_t pos);
void CL_ForceWall (vec3_t start, vec3_t end, int color);
void CL_FlameEffects (centity_t *ent, vec3_t origin);
void CL_GenericParticleEffect (vec3_t org, vec3_t dir, int color, int count, int numcolors, int dirspread, float alphavel);
void CL_BubbleTrail2 (vec3_t start, vec3_t end, int dist);
void CL_Heatbeam (vec3_t start, vec3_t end);
void CL_ParticleSteamEffect (vec3_t org, vec3_t dir, int color, int count, int magnitude);
void CL_TrackerTrail (vec3_t start, vec3_t end, int particleColor);
void CL_Tracker_Explode(vec3_t origin);
void CL_TagTrail (vec3_t start, vec3_t end, float color);
void CL_ColorFlash (vec3_t pos, int ent, int intensity, float r, float g, float b);
void CL_Tracker_Shell(vec3_t origin);
void CL_MonsterPlasma_Shell(vec3_t origin);
void CL_ColorExplosionParticles (vec3_t org, int color, int run);
void CL_ParticleSmokeEffect (vec3_t org, vec3_t dir, int color, int count, int magnitude);
void CL_Widowbeamout (cl_sustain_t *self);
void CL_Nukeblast (cl_sustain_t *self);
void CL_WidowSplash (vec3_t org);
// PGM
// ========

int CL_ParseEntityBits (unsigned *bits);
void CL_ParseDelta (entity_state_t *from, entity_state_t *to, int number, int bits);
void CL_ParseFrame (void);

void CL_ParseTEnt (void);
void CL_ParseConfigString (void);
void CL_ParseMuzzleFlash (void);
void CL_ParseMuzzleFlash2 (void);
void SmokeAndFlash(vec3_t origin);

void CL_SetLightstyle (int i);

void CL_RunParticles (void);
void CL_RunDLights (void);
void CL_RunLightStyles (void);

void CL_AddEntities (void);
void CL_AddDLights (void);
void CL_AddTEnts (void);
void CL_AddLightStyles (void);

//=================================================

void CL_PrepRefresh (void);
void CL_RegisterSounds (void);

void CL_Quit_f (void);

void IN_Accumulate (void);

void CL_ParseLayout (void);


//
// cl_main
//
extern	refexport_t	re;		// interface to refresh .dll

void CL_Init (void);

void CL_FixUpGender(void);
void CL_Disconnect (void);
void CL_Disconnect_f (void);
void CL_GetChallengePacket (void);
void CL_PingServers_f (void);
void CL_Snd_Restart_f (void);
void CL_RequestNextDownload (void);

//
// cl_input
//
typedef struct
{
	int			down[2];		// key nums holding it down
	unsigned	downtime;		// msec timestamp
	unsigned	msec;			// msec down this frame
	int			state;
} kbutton_t;

extern	kbutton_t	in_mlook, in_klook;
extern 	kbutton_t 	in_strafe;
extern 	kbutton_t 	in_speed;

void CL_InitInput (void);
void CL_SendCmd (void);
void CL_SendMove (usercmd_t *cmd);

void CL_ClearState (void);

void CL_ReadPackets (void);

int  CL_ReadFromServer (void);
void CL_WriteToServer (usercmd_t *cmd);
void CL_BaseMove (usercmd_t *cmd);

void IN_CenterView (void);

float CL_KeyState (kbutton_t *key);
char *Key_KeynumToString (int keynum);

//
// cl_demo.c
//
void CL_WriteDemoMessage (void);
void CL_Stop_f (void);
void CL_Record_f (void);

//
// cl_parse.c
//
extern	char *svc_strings[256];

void CL_ParseServerMessage (void);
void CL_LoadClientinfo (clientinfo_t *ci, char *s);
void SHOWNET(char *s);
void CL_ParseClientinfo (int player);
void CL_Download_f (void);

//
// cl_view.c
//
extern	int			gun_frame;
extern	struct model_s	*gun_model;

void V_Init (void);
void V_RenderView( float stereo_separation );
void V_AddEntity (entity_t *ent);
void V_AddParticle (vec3_t org, int color, float alpha);
void V_AddLight (vec3_t org, float intensity, float r, float g, float b);
void V_AddLightStyle (int style, float r, float g, float b);

//
// cl_tent.c
//
void CL_RegisterTEntSounds (void);
void CL_RegisterTEntModels (void);
void CL_SmokeAndFlash(vec3_t origin);


//
// cl_pred.c
//
void CL_InitPrediction (void);
void CL_PredictMove (void);
void CL_CheckPredictionError (void);

//
// cl_fx.c
//
cdlight_t *CL_AllocDlight (int key);
void CL_BigTeleportParticles (vec3_t org);
void CL_RocketTrail (vec3_t start, vec3_t end, centity_t *old);
void CL_DiminishingTrail (vec3_t start, vec3_t end, centity_t *old, int flags);
void CL_FlyEffect (centity_t *ent, vec3_t origin);
void CL_BfgParticles (entity_t *ent);
void CL_AddParticles (void);
void CL_EntityEvent (entity_state_t *ent);
// RAFAEL
void CL_TrapParticles (entity_t *ent);

//
// menus
//
void M_Init (void);
void M_Keydown (int key);
void M_Draw (void);
void M_Menu_Main_f (void);
void M_ForceMenuOff (void);
void M_AddToServerList (netadr_t adr, char *info);

//
// cl_inv.c
//
void CL_ParseInventory (void);
void CL_KeyInventory (int key);
void CL_DrawInventory (void);

//
// cl_pred.c
//
void CL_PredictMovement (void);

