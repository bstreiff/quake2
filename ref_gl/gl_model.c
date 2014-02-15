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
// models.c -- model loading and caching

#include "gl_local.h"
#include "q_math.h"

model_t	*loadmodel;
int		modfilelen;

void Mod_LoadSpriteModel (model_t *mod, void *buffer);
void Mod_LoadBrushModel (model_t *mod, void *buffer);
void Mod_LoadAliasModel (model_t *mod, void *buffer);
model_t *Mod_LoadModel (model_t *mod, qboolean crash);

byte	mod_novis[MAX_MAP_LEAFS/8];

#define	MAX_MOD_KNOWN	512
model_t	mod_known[MAX_MOD_KNOWN];
int		mod_numknown;

// the inline * models from the current map are kept seperate
model_t	mod_inline[MAX_MOD_KNOWN];

int		registration_sequence;

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t *Mod_PointInLeaf (vec3_t p, model_t *model)
{
	mnode_t		*node;
	float		d;
	cplane_t	*plane;
	
	if (!model || !model->nodes)
		ri.Sys_Error (ERR_DROP, "Mod_PointInLeaf: bad model");

	node = model->nodes;
	while (1)
	{
		if (node->contents != -1)
			return (mleaf_t *)node;
		plane = node->plane;
		d = DotProduct (p,plane->normal) - plane->dist;
		if (d > 0)
			node = node->children[0];
		else
			node = node->children[1];
	}
	
	return NULL;	// never reached
}


/*
===================
Mod_DecompressVis
===================
*/
byte *Mod_DecompressVis (byte *in, model_t *model)
{
	static byte	decompressed[MAX_MAP_LEAFS/8];
	int		c;
	byte	*out;
	int		row;

	row = (model->vis->numclusters+7)>>3;	
	out = decompressed;

	if (!in)
	{	// no vis info, so make all visible
		while (row)
		{
			*out++ = 0xff;
			row--;
		}
		return decompressed;		
	}

	do
	{
		if (*in)
		{
			*out++ = *in++;
			continue;
		}
	
		c = in[1];
		in += 2;
		while (c)
		{
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);
	
	return decompressed;
}

/*
==============
Mod_ClusterPVS
==============
*/
byte *Mod_ClusterPVS (int cluster, model_t *model)
{
	if (cluster == -1 || !model->vis)
		return mod_novis;
	return Mod_DecompressVis ( (byte *)model->vis + model->vis->bitofs[cluster][DVIS_PVS],
		model);
}


//===============================================================================

/*
================
Mod_Modellist_f
================
*/
void Mod_Modellist_f (void)
{
	int		i;
	model_t	*mod;
	int		total;

	total = 0;
	ri.Con_Printf (PRINT_ALL,"Loaded models:\n");
	for (i=0, mod=mod_known ; i < mod_numknown ; i++, mod++)
	{
		if (!mod->name[0])
			continue;
		ri.Con_Printf (PRINT_ALL, "%8i : %s\n",mod->extradatasize, mod->name);
		total += mod->extradatasize;
	}
	ri.Con_Printf (PRINT_ALL, "Total resident: %i\n", total);
}

/*
===============
Mod_Init
===============
*/
void Mod_Init (void)
{
	memset (mod_novis, 0xff, sizeof(mod_novis));
}



/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
model_t *Mod_ForName (char *name, qboolean crash)
{
	model_t	*mod;
	unsigned *buf;
	int		i;
	
	if (!name[0])
		ri.Sys_Error (ERR_DROP, "Mod_ForName: NULL name");
		
	//
	// inline models are grabbed only from worldmodel
	//
	if (name[0] == '*')
	{
		i = atoi(name+1);
		if (i < 1 || !r_worldmodel || i >= r_worldmodel->numsubmodels)
			ri.Sys_Error (ERR_DROP, "bad inline model number");
		return &mod_inline[i];
	}

	//
	// search the currently loaded models
	//
	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
	{
		if (!mod->name[0])
			continue;
		if (!strcmp (mod->name, name) )
			return mod;
	}
	
	//
	// find a free model slot spot
	//
	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
	{
		if (!mod->name[0])
			break;	// free spot
	}
	if (i == mod_numknown)
	{
		if (mod_numknown == MAX_MOD_KNOWN)
			ri.Sys_Error (ERR_DROP, "mod_numknown == MAX_MOD_KNOWN");
		mod_numknown++;
	}
	strcpy (mod->name, name);
	
	//
	// load the file
	//
	modfilelen = ri.FS_LoadFile (mod->name, &buf);
	if (!buf)
	{
		if (crash)
			ri.Sys_Error (ERR_DROP, "Mod_NumForName: %s not found", mod->name);
		memset (mod->name, 0, sizeof(mod->name));
		return NULL;
	}
	
	loadmodel = mod;

	//
	// fill it in
	//


	// call the apropriate loader
	
	switch (LittleLong(*(unsigned *)buf))
	{
	case IDALIASHEADER:
		loadmodel->extradata = Hunk_Begin (0x800000);
		Mod_LoadAliasModel (mod, buf);
		break;
		
	case IDSPRITEHEADER:
		loadmodel->extradata = Hunk_Begin (0x10000);
		Mod_LoadSpriteModel (mod, buf);
		break;
	
	case IDBSPHEADER:
		loadmodel->extradata = Hunk_Begin (0x1000000);
		Mod_LoadBrushModel (mod, buf);
		break;

	default:
		ri.Sys_Error (ERR_DROP,"Mod_NumForName: unknown fileid for %s", mod->name);
		break;
	}

	loadmodel->extradatasize = Hunk_End ();

	ri.FS_FreeFile (buf);

	return mod;
}

/*
===============================================================================

					BRUSHMODEL LOADING

===============================================================================
*/

byte	*mod_base;


/*
=================
Mod_LoadLighting
=================
*/
void Mod_LoadLighting (lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->lightdata = NULL;
		return;
	}
	loadmodel->lightdata = Hunk_Alloc ( l->filelen);	
	memcpy (loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
}


/*
=================
Mod_LoadVisibility
=================
*/
void Mod_LoadVisibility (lump_t *l)
{
	int		i;

	if (!l->filelen)
	{
		loadmodel->vis = NULL;
		return;
	}
	loadmodel->vis = Hunk_Alloc ( l->filelen);	
	memcpy (loadmodel->vis, mod_base + l->fileofs, l->filelen);

	loadmodel->vis->numclusters = LittleLong (loadmodel->vis->numclusters);
	for (i=0 ; i<loadmodel->vis->numclusters ; i++)
	{
		loadmodel->vis->bitofs[i][0] = LittleLong (loadmodel->vis->bitofs[i][0]);
		loadmodel->vis->bitofs[i][1] = LittleLong (loadmodel->vis->bitofs[i][1]);
	}
}


/*
=================
Mod_LoadVertexes
=================
*/
void Mod_LoadVertexes (lump_t *l)
{
	dvertex_t	*in;
	mvertex_t	*out;
	int			i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->vertexes = out;
	loadmodel->numvertexes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->position[0] = LittleFloat (in->point[0]);
		out->position[1] = LittleFloat (in->point[1]);
		out->position[2] = LittleFloat (in->point[2]);
	}
}

/*
=================
RadiusFromBounds
=================
*/
float RadiusFromBounds (vec3_t mins, vec3_t maxs)
{
	int		i;
	vec3_t	corner;

	for (i=0 ; i<3 ; i++)
	{
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);
	}

	return VectorLength (corner);
}


/*
=================
Mod_LoadSubmodels
=================
*/
void Mod_LoadSubmodels (lump_t *l)
{
	dmodel_t	*in;
	mmodel_t	*out;
	int			i, j, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{	// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
			out->origin[j] = LittleFloat (in->origin[j]);
		}
		out->radius = RadiusFromBounds (out->mins, out->maxs);
		out->headnode = LittleLong (in->headnode);
		out->firstface = LittleLong (in->firstface);
		out->numfaces = LittleLong (in->numfaces);
	}
}

/*
=================
Mod_LoadEdges
=================
*/
void Mod_LoadEdges (lump_t *l)
{
	dedge_t *in;
	medge_t *out;
	int 	i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( (count + 1) * sizeof(*out));	

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->v[0] = (unsigned short)LittleShort(in->v[0]);
		out->v[1] = (unsigned short)LittleShort(in->v[1]);
	}
}

/*
=================
Mod_LoadTexinfo
=================
*/
void Mod_LoadTexinfo (lump_t *l)
{
	texinfo_t *in;
	mtexinfo_t *out, *step;
	int 	i, j, count;
	char	name[MAX_QPATH];
	int		next;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<8 ; j++)
			out->vecs[0][j] = LittleFloat (in->vecs[0][j]);

		out->flags = LittleLong (in->flags);
		next = LittleLong (in->nexttexinfo);
		if (next > 0)
			out->next = loadmodel->texinfo + next;
		else
		    out->next = NULL;
		Com_sprintf (name, sizeof(name), "textures/%s.wal", in->texture);

		out->image = GL_FindImage (name, it_wall);
		if (!out->image)
		{
			ri.Con_Printf (PRINT_ALL, "Couldn't load %s\n", name);
			out->image = r_notexture;
		}
	}

	// count animation frames
	for (i=0 ; i<count ; i++)
	{
		out = &loadmodel->texinfo[i];
		out->numframes = 1;
		for (step = out->next ; step && step != out ; step=step->next)
			out->numframes++;
	}
}

/*
================
CalcSurfaceExtents

Fills in s->texturemins[] and s->extents[]
================
*/
void CalcSurfaceExtents (msurface_t *s)
{
	float	mins[2], maxs[2], val;
	int		i,j, e;
	mvertex_t	*v;
	mtexinfo_t	*tex;
	int		bmins[2], bmaxs[2];

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;
	
	for (i=0 ; i<s->numedges ; i++)
	{
		e = loadmodel->surfedges[s->firstedge+i];
		if (e >= 0)
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];
		
		for (j=0 ; j<2 ; j++)
		{
			val = v->position[0] * tex->vecs[j][0] + 
				v->position[1] * tex->vecs[j][1] +
				v->position[2] * tex->vecs[j][2] +
				tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i=0 ; i<2 ; i++)
	{	
		bmins[i] = floor(mins[i]/16);
		bmaxs[i] = ceil(maxs[i]/16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;

//		if ( !(tex->flags & TEX_SPECIAL) && s->extents[i] > 512 /* 256 */ )
//			ri.Sys_Error (ERR_DROP, "Bad surface extents");
	}
}


void GL_BuildPolygonFromSurface(msurface_t *fa);
void GL_CreateSurfaceLightmap (msurface_t *surf);
void GL_EndBuildingLightmaps (void);
void GL_BeginBuildingLightmaps (model_t *m);

/*
=================
Mod_LoadFaces
=================
*/
void Mod_LoadFaces (lump_t *l)
{
	dface_t		*in;
	msurface_t 	*out;
	int			i, count, surfnum;
	int			planenum, side;
	int			ti;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	currentmodel = loadmodel;

	GL_BeginBuildingLightmaps (loadmodel);

	for ( surfnum=0 ; surfnum<count ; surfnum++, in++, out++)
	{
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);		
		out->flags = 0;
		out->polys = NULL;

		planenum = LittleShort(in->planenum);
		side = LittleShort(in->side);
		if (side)
			out->flags |= SURF_PLANEBACK;			

		out->plane = loadmodel->planes + planenum;

		ti = LittleShort (in->texinfo);
		if (ti < 0 || ti >= loadmodel->numtexinfo)
			ri.Sys_Error (ERR_DROP, "MOD_LoadBmodel: bad texinfo number");
		out->texinfo = loadmodel->texinfo + ti;

		CalcSurfaceExtents (out);
				
	// lighting info

		for (i=0 ; i<MAXLIGHTMAPS ; i++)
			out->styles[i] = in->styles[i];
		i = LittleLong(in->lightofs);
		if (i == -1)
			out->samples = NULL;
		else
			out->samples = loadmodel->lightdata + i;
		
	// set the drawing flags
		
		if (out->texinfo->flags & SURF_WARP)
		{
			out->flags |= SURF_DRAWTURB;
			for (i=0 ; i<2 ; i++)
			{
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
			GL_SubdivideSurface (out);	// cut up polygon for warps
		}

		// create lightmaps and polygons
		if ( !(out->texinfo->flags & (SURF_SKY|SURF_TRANS33|SURF_TRANS66|SURF_WARP) ) )
			GL_CreateSurfaceLightmap (out);

		if (! (out->texinfo->flags & SURF_WARP) ) 
			GL_BuildPolygonFromSurface(out);

	}

	GL_EndBuildingLightmaps ();
}


/*
=================
Mod_SetParent
=================
*/
void Mod_SetParent (mnode_t *node, mnode_t *parent)
{
	node->parent = parent;
	if (node->contents != -1)
		return;
	Mod_SetParent (node->children[0], node);
	Mod_SetParent (node->children[1], node);
}

/*
=================
Mod_LoadNodes
=================
*/
void Mod_LoadNodes (lump_t *l)
{
	int			i, j, count, p;
	dnode_t		*in;
	mnode_t 	*out;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}
	
		p = LittleLong(in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = LittleShort (in->firstface);
		out->numsurfaces = LittleShort (in->numfaces);
		out->contents = -1;	// differentiate from leafs

		for (j=0 ; j<2 ; j++)
		{
			p = LittleLong (in->children[j]);
			if (p >= 0)
				out->children[j] = loadmodel->nodes + p;
			else
				out->children[j] = (mnode_t *)(loadmodel->leafs + (-1 - p));
		}
	}
	
	Mod_SetParent (loadmodel->nodes, NULL);	// sets nodes and leafs
}

/*
=================
Mod_LoadLeafs
=================
*/
void Mod_LoadLeafs (lump_t *l)
{
	dleaf_t 	*in;
	mleaf_t 	*out;
	int			i, j, count, p;
//	glpoly_t	*poly;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong(in->contents);
		out->contents = p;

		out->cluster = LittleShort(in->cluster);
		out->area = LittleShort(in->area);

		out->firstmarksurface = loadmodel->marksurfaces +
			LittleShort(in->firstleafface);
		out->nummarksurfaces = LittleShort(in->numleaffaces);
		
		// gl underwater warp
#if 0
		if (out->contents & (CONTENTS_WATER|CONTENTS_SLIME|CONTENTS_LAVA|CONTENTS_THINWATER) )
		{
			for (j=0 ; j<out->nummarksurfaces ; j++)
			{
				out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
				for (poly = out->firstmarksurface[j]->polys ; poly ; poly=poly->next)
					poly->flags |= SURF_UNDERWATER;
			}
		}
#endif
	}	
}

/*
=================
Mod_LoadMarksurfaces
=================
*/
void Mod_LoadMarksurfaces (lump_t *l)
{	
	int		i, j, count;
	short		*in;
	msurface_t **out;
	
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for ( i=0 ; i<count ; i++)
	{
		j = LittleShort(in[i]);
		if (j < 0 ||  j >= loadmodel->numsurfaces)
			ri.Sys_Error (ERR_DROP, "Mod_ParseMarksurfaces: bad surface number");
		out[i] = loadmodel->surfaces + j;
	}
}

/*
=================
Mod_LoadSurfedges
=================
*/
void Mod_LoadSurfedges (lump_t *l)
{	
	int		i, count;
	int		*in, *out;
	
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	if (count < 1 || count >= MAX_MAP_SURFEDGES)
		ri.Sys_Error (ERR_DROP, "MOD_LoadBmodel: bad surfedges count in %s: %i",
		loadmodel->name, count);

	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	for ( i=0 ; i<count ; i++)
		out[i] = LittleLong (in[i]);
}


/*
=================
Mod_LoadPlanes
=================
*/
void Mod_LoadPlanes (lump_t *l)
{
	int			i, j;
	cplane_t	*out;
	dplane_t 	*in;
	int			count;
	int			bits;
	
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*2*sizeof(*out));	
	
	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		bits = 0;
		for (j=0 ; j<3 ; j++)
		{
			out->normal[j] = LittleFloat (in->normal[j]);
			if (out->normal[j] < 0)
				bits |= 1<<j;
		}

		out->dist = LittleFloat (in->dist);
		out->type = LittleLong (in->type);
		out->signbits = bits;
	}
}

/*
=================
Mod_LoadBrushModel
=================
*/
void Mod_LoadBrushModel (model_t *mod, void *buffer)
{
	int			i;
	dheader_t	*header;
	mmodel_t 	*bm;
	
	loadmodel->type = mod_brush;
	if (loadmodel != mod_known)
		ri.Sys_Error (ERR_DROP, "Loaded a brush model after the world");

	header = (dheader_t *)buffer;

	i = LittleLong (header->version);
	if (i != BSPVERSION)
		ri.Sys_Error (ERR_DROP, "Mod_LoadBrushModel: %s has wrong version number (%i should be %i)", mod->name, i, BSPVERSION);

// swap all the lumps
	mod_base = (byte *)header;

	for (i=0 ; i<sizeof(dheader_t)/4 ; i++)
		((int *)header)[i] = LittleLong ( ((int *)header)[i]);

// load into heap
	
	Mod_LoadVertexes (&header->lumps[LUMP_VERTEXES]);
	Mod_LoadEdges (&header->lumps[LUMP_EDGES]);
	Mod_LoadSurfedges (&header->lumps[LUMP_SURFEDGES]);
	Mod_LoadLighting (&header->lumps[LUMP_LIGHTING]);
	Mod_LoadPlanes (&header->lumps[LUMP_PLANES]);
	Mod_LoadTexinfo (&header->lumps[LUMP_TEXINFO]);
	Mod_LoadFaces (&header->lumps[LUMP_FACES]);
	Mod_LoadMarksurfaces (&header->lumps[LUMP_LEAFFACES]);
	Mod_LoadVisibility (&header->lumps[LUMP_VISIBILITY]);
	Mod_LoadLeafs (&header->lumps[LUMP_LEAFS]);
	Mod_LoadNodes (&header->lumps[LUMP_NODES]);
	Mod_LoadSubmodels (&header->lumps[LUMP_MODELS]);
	mod->numframes = 2;		// regular and alternate animation
	
//
// set up the submodels
//
	for (i=0 ; i<mod->numsubmodels ; i++)
	{
		model_t	*starmod;

		bm = &mod->submodels[i];
		starmod = &mod_inline[i];

		*starmod = *loadmodel;
		
		starmod->firstmodelsurface = bm->firstface;
		starmod->nummodelsurfaces = bm->numfaces;
		starmod->firstnode = bm->headnode;
		if (starmod->firstnode >= loadmodel->numnodes)
			ri.Sys_Error (ERR_DROP, "Inline model %i has bad firstnode", i);

		VectorCopy (bm->maxs, starmod->maxs);
		VectorCopy (bm->mins, starmod->mins);
		starmod->radius = bm->radius;
	
		if (i == 0)
			*loadmodel = *starmod;

		starmod->numleafs = bm->visleafs;
	}
}

/*
==============================================================================

ALIAS MODELS

==============================================================================
*/

/*
=================
Mod_LoadAliasModel
=================
*/
#define NUMVERTEXNORMALS	162

// MD2 files use these for precalculated normals.
// We use those normals if the gl_fixupmodels cvar is 0.
static const float	r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

typedef struct
{
	unsigned int	index;	// vertex index
	unsigned int	submodel_id;	// id of submodel_id
} vertex_submodel_t;

static int compare_submodel_ids(const void* x, const void* y)
{
	const vertex_submodel_t* xs = (const vertex_submodel_t*)x;
	const vertex_submodel_t* ys = (const vertex_submodel_t*)y;

	return (xs->submodel_id - ys->submodel_id);
}

// 'begin' and 'end' have STL semantics:
// - 'end' must be greater or equal to 'begin'.
// - 'end' refers to the address after the last valid element.
//
// You must have at least three points. This is probably not a huge problem,
// because the minimum possible set would be the three vertices of a single
// triangle.
//
// http://nghiaho.com/?page_id=671
static void Mod_FixupAliasSubmodel(
	const char* name,
	alias_model_t* alias,
	vertex_submodel_t* begin,
	vertex_submodel_t* end)
{
	typedef struct {
		vec3_t centroid;
	} frame_info_t;
	frame_info_t* frame_info;
	size_t vtxcount = (end - begin);

	frame_info = (frame_info_t*)malloc(sizeof(frame_info_t)*alias->num_frames);
	memset(frame_info, 0, sizeof(frame_info_t)*alias->num_frames);

	// Compute the centroids (midpoint of all vertices) for each frame
	for (int f = 0; f < alias->num_frames; ++f)
	{
		vec3_t sum = { 0, 0, 0 };
		for (vertex_submodel_t* submodel_itr = begin; submodel_itr != end; ++submodel_itr)
		{
			alias_model_vertex_t* vtx = &(alias->frames[f].verts[submodel_itr->index]);
			VectorAdd(vtx->v, sum, sum);
		}
		VectorScale(sum, 1.0f / (vec_t)vtxcount, frame_info[f].centroid);
	}

	// Accumulate a matrix as per the following:
	// H = sum across all points (A_i - centroid_A)(B_i - centroid_B)^T
	// where A_i and B_i are the same indexed vertex in frames A and B.
	for (int f = 1; f < alias->num_frames; ++f)
	{
		mat3x3_t h = { 0 };
		vec3_t s = { 0 };
		mat3x3_t v = { 0 };
		mat3x3_t rotation = { 0 };
		vec3_t translation = { 0 };
		double err = 0;

		for (vertex_submodel_t* submodel_itr = begin; submodel_itr != end; ++submodel_itr)
		{
			alias_model_vertex_t* src_vtx = &(alias->frames[0].verts[submodel_itr->index]);
			alias_model_vertex_t* dst_vtx = &(alias->frames[f].verts[submodel_itr->index]);
			vec3_t src_v;
			vec3_t dst_v;
			mat3x3_t tmp;

			VectorCopy(src_vtx->v, src_v);
			VectorSubtract(src_v, frame_info[0].centroid, src_v);
			VectorCopy(dst_vtx->v, dst_v);
			VectorSubtract(dst_v, frame_info[f].centroid, dst_v);

			MatrixMultiplyColumnVectorWithTransposedColumnVector(
				src_v,
				dst_v,
				tmp);
			MatrixAdd(h, tmp, h);
		}

		// [ U, S, V ] = SVD(H)
		// (MatrixSingularValueDcomposition() returns U over its original input variable.)
		MatrixSingularValueDcomposition(h, s, v);

		// The rotation matrix can be found from:
		// R = VU^T
		MatrixTranspose(h);
		MatrixMultiply(v, h, rotation);

		// There's a reflection case to deal with here. If the determinant is less
		// than zero, multiply the third column by -1.
		if (MatrixDeterminant(rotation) < 0)
		{
			rotation[0][2] *= -1;
			rotation[1][2] *= -1;
			rotation[2][2] *= -1;
		}

		// The translation is:
		// t = -R * centroid_A + centroid_B
		// Reuse 'v' as temporary space.
		MatrixScale(rotation, -1, v);
		MatrixMultiplyWithColumnVector(v, frame_info[0].centroid, translation);
		VectorAdd(translation, frame_info[f].centroid, translation);

		// Compute the error.
		// err = sum across all points ||R*A_i + t - B_i||^2
		// where A_i and B_i are the same vertex in frames A and B, and || is the Euclidian distance operator
		for (vertex_submodel_t* submodel_itr = begin; submodel_itr != end; ++submodel_itr)
		{
			alias_model_vertex_t* src_vtx = &(alias->frames[0].verts[submodel_itr->index]);
			alias_model_vertex_t* dst_vtx = &(alias->frames[f].verts[submodel_itr->index]);
			vec3_t tmp;

			MatrixMultiplyWithColumnVector(rotation, src_vtx->v, tmp);
			VectorAdd(tmp, translation, tmp);

			// Find the Euclidian distance (squared) between tmp and B_i
			// sqrt((a0-b0)^2 + (a1-b1)^2 + (a2-b2)^2)^2
			// (a0-b0)^2 + (a1-b1)^2 + (a2-b2)^2
			//
			// Because we're summing all of these up, we can add them directly to err.

			for (int k = 0; k < 2; ++k)
				err += (tmp[k] - dst_vtx->v[k]) * (tmp[k] - dst_vtx->v[k]);
		}

		// If the least-squares error is less than 1.0, than the rotation and translation
		// is probably a pretty good fit. Apply them to get new vertex values.
		if (err < gl_fixupmodels_threshold->value)
		{
			for (vertex_submodel_t* submodel_itr = begin; submodel_itr != end; ++submodel_itr)
			{
				alias_model_vertex_t* src_vtx = &(alias->frames[0].verts[submodel_itr->index]);
				alias_model_vertex_t* dst_vtx = &(alias->frames[f].verts[submodel_itr->index]);

				MatrixMultiplyWithColumnVector(rotation, src_vtx->v, dst_vtx->v);
				VectorAdd(dst_vtx->v, translation, dst_vtx->v);
			}
		}
	}

	free(frame_info);
}

static void Mod_RecomputeVertexNormals(
	const char* name,
	alias_model_t* alias)
{
	typedef struct {
		vec3_t normal_sum;
		unsigned int normal_count;
	} computed_normal_t;

	computed_normal_t* normals;
	normals = (computed_normal_t*)malloc(sizeof(computed_normal_t)*alias->frames[0].num_verts);
	memset(normals, 0, sizeof(computed_normal_t)*alias->frames[0].num_verts);

	for (int f = 0; f < alias->num_frames; ++f)
	{
		alias_model_frame_t* frame = &(alias->frames[f]);

		for (int t = 0; t < alias->num_tris; ++t)
		{
			vec3_t vtemp1, vtemp2, normal;
			float ftemp;
			alias_model_triangle_t* tri = &(alias->tris[t]);

			VectorSubtract(
				frame->verts[tri->index_xyz[0]].v,
				frame->verts[tri->index_xyz[1]].v,
				vtemp1);
			VectorSubtract(
				frame->verts[tri->index_xyz[2]].v,
				frame->verts[tri->index_xyz[1]].v,
				vtemp2);
			CrossProduct(vtemp1, vtemp2, normal);
			VectorNormalize(normal);

			// Rotate the normal so that the model faces down the positive x axis.
			ftemp = normal[0];
			normal[0] = -normal[1];
			normal[1] = ftemp;

			for (int i = 0; i < 3; ++i)
			{
				VectorAdd(normals[tri->index_xyz[i]].normal_sum, normal, normals[tri->index_xyz[i]].normal_sum);
				normals[tri->index_xyz[i]].normal_count++;
			}
		}

		// Average the normals for each vertex.
		for (int i = 0; i < frame->num_verts; ++i)
		{
			vec3_t v;
			VectorScale(normals[i].normal_sum, 1.0f / normals[i].normal_count, v);
			VectorNormalize(v);
			VectorCopy(v, frame->verts[i].lightnormal);
		}
	}

	free(normals);
}

static void Mod_FixupAliasModel(const char* name, alias_model_t *alias)
{
	qboolean has_updated_tags = false;
	vertex_submodel_t*	vtx_submodel_ids;
	unsigned int	last_submodel_id = ~0UL;
	vertex_submodel_t* begin;
	vertex_submodel_t* end;

	// This transformation only works if we have multiple frames to compare.
	if (alias->num_frames <= 1)
		return;

	vtx_submodel_ids = (vertex_submodel_t*)malloc(alias->frames[0].num_verts*sizeof(vertex_submodel_t));

	// Initially, each submodel_id id corresponds to each vertex id.
	for (size_t i = 0; i < alias->frames[0].num_verts; ++i)
	{
		vtx_submodel_ids[i].submodel_id = vtx_submodel_ids[i].index = (unsigned int)i;
	}

	// First, we tag all vertices to group them into submodel_ids.
	// The submodel_id for a vertex is the minimum of the submodel_ids for all neighbors,
	// applied transitively.
	do
	{
		has_updated_tags = false;

		for (int i = 0; i < alias->num_tris; ++i)
		{
			short tri_submodel_ids[3];
			short new_submodel_id;
			for (int j = 0; j < 3; ++j)
				tri_submodel_ids[j] = vtx_submodel_ids[alias->tris[i].index_xyz[j]].submodel_id;

			new_submodel_id = min(min(tri_submodel_ids[0], tri_submodel_ids[1]), tri_submodel_ids[2]);

			for (int j = 0; j < 3; ++j)
			{
				if (new_submodel_id < vtx_submodel_ids[alias->tris[i].index_xyz[j]].submodel_id)
				{
					vtx_submodel_ids[alias->tris[i].index_xyz[j]].submodel_id = new_submodel_id;
					has_updated_tags = true;
				}
			}
		}
	} while (has_updated_tags);

	// Sort by submodel_id.
	qsort(vtx_submodel_ids, alias->frames[0].num_verts, sizeof(vertex_submodel_t), compare_submodel_ids);

	// Now, run Mod_FixupAliasSubmodel on each grouping of vertices.
	last_submodel_id = vtx_submodel_ids[0].submodel_id;
	begin = vtx_submodel_ids;
	end = vtx_submodel_ids + 1;
	for (int i = 1; i < alias->frames[0].num_verts; ++i, ++end)
	{
		if (vtx_submodel_ids[i].submodel_id != last_submodel_id)
		{
			last_submodel_id = vtx_submodel_ids[i].submodel_id;
			Mod_FixupAliasSubmodel(name, alias, begin, end);
			begin = end;
		}
	}
	if (begin != end)
	{
		Mod_FixupAliasSubmodel(name, alias, begin, end);
	}

	free(vtx_submodel_ids);

	Mod_RecomputeVertexNormals(name, alias);
}

void Mod_LoadAliasModel(model_t *mod, void *buffer)
{
	int					i, j;
	const byte			*bufferItr = (byte*)buffer;
	const dtriangle_t	*pintri;
	int					*pincmd;
	alias_model_t		*outmodel;
	dmdl_t				header;

	memset(&header, 0, sizeof(dmdl_t));

	// Skip past ident (was already checked)
	header.ident = LittleLong(*(int*)bufferItr);
	bufferItr += sizeof(int);

	header.version = LittleLong(*(int*)bufferItr);
	if (header.version != ALIAS_VERSION)
		ri.Sys_Error(ERR_DROP, "%s has wrong version number (%i should be %i)",
		mod->name, header.version, ALIAS_VERSION);

	bufferItr += sizeof(int);

	header.skinwidth = LittleLong(*(int*)bufferItr);
	bufferItr += sizeof(int);

	header.skinheight = LittleLong(*(int*)bufferItr);
	if (header.skinheight > MAX_LBM_HEIGHT)
		ri.Sys_Error(ERR_DROP, "model %s has a skin taller than %d", mod->name,
		MAX_LBM_HEIGHT);
	bufferItr += sizeof(int);

	header.framesize = LittleLong(*(int*)bufferItr);
	bufferItr += sizeof(int);

	header.num_skins = LittleLong(*(int*)bufferItr);
	bufferItr += sizeof(int);

	header.num_xyz = LittleLong(*(int*)bufferItr);
	if (header.num_xyz <= 0)
		ri.Sys_Error(ERR_DROP, "model %s has no vertices", mod->name);
	if (header.num_xyz > MAX_VERTS)
		ri.Sys_Error(ERR_DROP, "model %s has too many vertices", mod->name);
	bufferItr += sizeof(int);

	header.num_st = LittleLong(*(int*)bufferItr);
	if (header.num_st <= 0)
		ri.Sys_Error(ERR_DROP, "model %s has no st vertices", mod->name);
	bufferItr += sizeof(int);

	header.num_tris = LittleLong(*(int*)bufferItr);
	if (header.num_tris <= 0)
		ri.Sys_Error(ERR_DROP, "model %s has no triangles", mod->name);
	bufferItr += sizeof(int);

	header.num_glcmds = LittleLong(*(int*)bufferItr);
	bufferItr += sizeof(int);

	header.num_frames = LittleLong(*(int*)bufferItr);
	if (header.num_frames <= 0)
		ri.Sys_Error(ERR_DROP, "model %s has no frames", mod->name);
	bufferItr += sizeof(int);

	header.ofs_skins = LittleLong(*(int*)bufferItr);
	bufferItr += sizeof(int);
	header.ofs_st = LittleLong(*(int*)bufferItr);
	bufferItr += sizeof(int);
	header.ofs_tris = LittleLong(*(int*)bufferItr);
	bufferItr += sizeof(int);
	header.ofs_frames = LittleLong(*(int*)bufferItr);
	bufferItr += sizeof(int);
	header.ofs_glcmds = LittleLong(*(int*)bufferItr);
	bufferItr += sizeof(int);
	header.ofs_end = LittleLong(*(int*)bufferItr);
	bufferItr += sizeof(int);

	outmodel = (alias_model_t*)Hunk_Alloc(sizeof(alias_model_t));

	//
	// load triangle lists
	//
	pintri = (dtriangle_t *)((byte *)buffer + header.ofs_tris);
	outmodel->num_tris = header.num_tris;
	outmodel->tris = (alias_model_triangle_t*)Hunk_Alloc(header.num_tris * sizeof(alias_model_triangle_t));

	for (i = 0; i<header.num_tris; i++)
	{
		for (j = 0; j<3; j++)
		{
			outmodel->tris[i].index_xyz[j] = LittleShort(pintri[i].index_xyz[j]);
		}
	}

	//
	// load the frames
	//
	outmodel->num_frames = header.num_frames;
	outmodel->frames = (alias_model_frame_t*)Hunk_Alloc(header.num_frames * sizeof(alias_model_frame_t));
	for (i = 0; i<header.num_frames; i++)
	{
		vec3_t scale;
		vec3_t translate;
		const daliasframe_t* pinframe = (daliasframe_t *)((byte *)buffer + header.ofs_frames + i * header.framesize);
		alias_model_frame_t* frame = &(outmodel->frames[i]);

		for (j = 0; j<3; ++j)
		{
			scale[j] = LittleFloat(pinframe->scale[j]);
			translate[j] = LittleFloat(pinframe->translate[j]);
		}

		frame->num_verts = header.num_xyz;
		frame->verts = (alias_model_vertex_t*)Hunk_Alloc(header.num_xyz * sizeof(alias_model_vertex_t));
		for (j = 0; j < header.num_xyz; ++j)
		{
			const dtrivertx_t* ptrivertx = &(pinframe->verts[j]);
			alias_model_vertex_t* amv = &(outmodel->frames[i].verts[j]);

			amv->v[0] = (scale[0] * ptrivertx->v[0]) + translate[0];
			amv->v[1] = (scale[1] * ptrivertx->v[1]) + translate[1];
			amv->v[2] = (scale[2] * ptrivertx->v[2]) + translate[2];
			amv->lightnormal[0] = r_avertexnormals[ptrivertx->lightnormalindex][0];
			amv->lightnormal[1] = r_avertexnormals[ptrivertx->lightnormalindex][1];
			amv->lightnormal[2] = r_avertexnormals[ptrivertx->lightnormalindex][2];
		}

		for (j = 0; j < 3; ++j)
		{
			frame->mins[j] = translate[j];
			frame->maxs[j] = (scale[j] * 255.0f) + translate[j];
		}
	}

	mod->type = mod_alias;

	//
	// load the glcmds
	//
	pincmd = (int *)((byte *)buffer + header.ofs_glcmds);
	outmodel->num_glcmds = header.num_glcmds;
	outmodel->glcmds = (int*)Hunk_Alloc(header.num_glcmds * sizeof(int));
	for (i = 0; i<header.num_glcmds; i++)
		outmodel->glcmds[i] = LittleLong(pincmd[i]);

	// register all skins
	outmodel->num_skins = header.num_skins;
	outmodel->skins = (char**)Hunk_Alloc(header.num_skins * sizeof(char*));
	for (i = 0; i<header.num_skins; i++)
	{
		const char* skinname = (char *)buffer + header.ofs_skins + i*MAX_SKINNAME;
		size_t skinname_length = strnlen(skinname, MAX_SKINNAME);
		outmodel->skins[i] = (char*)Hunk_Alloc((skinname_length+1)*sizeof(char));
		strncpy(outmodel->skins[i], skinname, MAX_SKINNAME);
		outmodel->skins[i][skinname_length] = '\0';

		mod->skins[i] = GL_FindImage(outmodel->skins[i], it_skin);
	}

	mod->mins[0] = -32;
	mod->mins[1] = -32;
	mod->mins[2] = -32;
	mod->maxs[0] = 32;
	mod->maxs[1] = 32;
	mod->maxs[2] = 32;

	// Fix up vertices.
	if (gl_fixupmodels->value)
	{
		Mod_FixupAliasModel(mod->name, outmodel);
	}
}

/*
==============================================================================

SPRITE MODELS

==============================================================================
*/

/*
=================
Mod_LoadSpriteModel
=================
*/
void Mod_LoadSpriteModel (model_t *mod, void *buffer)
{
	dsprite_t	*sprin, *sprout;
	int			i;

	sprin = (dsprite_t *)buffer;
	sprout = Hunk_Alloc (modfilelen);

	sprout->ident = LittleLong (sprin->ident);
	sprout->version = LittleLong (sprin->version);
	sprout->numframes = LittleLong (sprin->numframes);

	if (sprout->version != SPRITE_VERSION)
		ri.Sys_Error (ERR_DROP, "%s has wrong version number (%i should be %i)",
				 mod->name, sprout->version, SPRITE_VERSION);

	if (sprout->numframes > MAX_MD2SKINS)
		ri.Sys_Error (ERR_DROP, "%s has too many frames (%i > %i)",
				 mod->name, sprout->numframes, MAX_MD2SKINS);

	// byte swap everything
	for (i=0 ; i<sprout->numframes ; i++)
	{
		sprout->frames[i].width = LittleLong (sprin->frames[i].width);
		sprout->frames[i].height = LittleLong (sprin->frames[i].height);
		sprout->frames[i].origin_x = LittleLong (sprin->frames[i].origin_x);
		sprout->frames[i].origin_y = LittleLong (sprin->frames[i].origin_y);
		memcpy (sprout->frames[i].name, sprin->frames[i].name, MAX_SKINNAME);
		mod->skins[i] = GL_FindImage (sprout->frames[i].name,
			it_sprite);
	}

	mod->type = mod_sprite;
}

//=============================================================================

/*
@@@@@@@@@@@@@@@@@@@@@
R_BeginRegistration

Specifies the model that will be used as the world
@@@@@@@@@@@@@@@@@@@@@
*/
void R_BeginRegistration (char *model)
{
	char	fullname[MAX_QPATH];
	cvar_t	*flushmap;

	registration_sequence++;
	r_oldviewcluster = -1;		// force markleafs

	Com_sprintf (fullname, sizeof(fullname), "maps/%s.bsp", model);

	// explicitly free the old map if different
	// this guarantees that mod_known[0] is the world map
	flushmap = ri.Cvar_Get ("flushmap", "0", 0);
	if ( strcmp(mod_known[0].name, fullname) || flushmap->value)
		Mod_Free (&mod_known[0]);
	r_worldmodel = Mod_ForName(fullname, true);

	r_viewcluster = -1;
}


/*
@@@@@@@@@@@@@@@@@@@@@
R_RegisterModel

@@@@@@@@@@@@@@@@@@@@@
*/
struct model_s *R_RegisterModel (char *name)
{
	model_t	*mod;
	int		i;
	dsprite_t	*sprout;

	mod = Mod_ForName (name, false);
	if (mod)
	{
		mod->registration_sequence = registration_sequence;

		// register any images used by the models
		if (mod->type == mod_sprite)
		{
			sprout = (dsprite_t *)mod->extradata;
			for (i=0 ; i<sprout->numframes ; i++)
				mod->skins[i] = GL_FindImage (sprout->frames[i].name, it_sprite);
		}
		else if (mod->type == mod_alias)
		{
			const alias_model_t* alias = (alias_model_t *)mod->extradata;
			for (i = 0; i<alias->num_skins; i++)
			{
				mod->skins[i] = GL_FindImage(alias->skins[i], it_skin);
			}

			mod->numframes = alias->num_frames;
		}
		else if (mod->type == mod_brush)
		{
			for (i=0 ; i<mod->numtexinfo ; i++)
				mod->texinfo[i].image->registration_sequence = registration_sequence;
		}
	}
	return mod;
}


/*
@@@@@@@@@@@@@@@@@@@@@
R_EndRegistration

@@@@@@@@@@@@@@@@@@@@@
*/
void R_EndRegistration (void)
{
	int		i;
	model_t	*mod;

	for (i=0, mod=mod_known ; i<mod_numknown ; i++, mod++)
	{
		if (!mod->name[0])
			continue;
		if (mod->registration_sequence != registration_sequence)
		{	// don't need this model
			Mod_Free (mod);
		}
	}

	GL_FreeUnusedImages ();
}


//=============================================================================


/*
================
Mod_Free
================
*/
void Mod_Free (model_t *mod)
{
	Hunk_Free(mod->extradata);
	memset(mod, 0, sizeof(*mod));
}

/*
================
Mod_FreeAll
================
*/
void Mod_FreeAll (void)
{
	int		i;

	for (i=0 ; i<mod_numknown ; i++)
	{
		if (mod_known[i].extradatasize)
			Mod_Free (&mod_known[i]);
	}
}
