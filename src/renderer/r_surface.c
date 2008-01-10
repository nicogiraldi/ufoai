/**
 * @file r_surface.c
 * @brief surface-related refresh code
 */

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

#include "r_local.h"
#include "r_lightmap.h"
#include "r_error.h"

mBspSurfaces_t r_opaque_surfaces;
mBspSurfaces_t r_opaque_warp_surfaces;
mBspSurfaces_t r_alpha_surfaces;
mBspSurfaces_t r_alpha_warp_surfaces;

/**
 * @brief this is the r_numMapTiles index of the loaded bsp
 * @note Remember that we can have loaded more than one bsp at the same time
 */
static int bufferMapTile = -1;

static void R_SetVertexArrayState (const mBspSurface_t *surf)
{
	model_t* mod;

	if (bufferMapTile == surf->tile)
		return;

	mod = r_mapTiles[surf->tile];

	R_BindArray(GL_VERTEX_ARRAY, GL_FLOAT, mod->bsp.verts);

	R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, mod->bsp.texcoords);

	if (r_state.multitexture_enabled) {  /* lightmap texcoords */
		R_SelectTexture(&r_state.lightmap_texunit);
		R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, mod->bsp.lmtexcoords);
		R_SelectTexture(&r_state.texture_texunit);
	}

	if (r_state.lighting_enabled)  /* normal vectors for lighting */
		R_BindArray(GL_NORMAL_ARRAY, GL_FLOAT, mod->bsp.normals);
}

static void R_SetVertexBufferState (const mBspSurface_t *surf)
{
	model_t *mod;

	if (bufferMapTile == surf->tile)
		return;

	mod = r_mapTiles[surf->tile];

	R_BindBuffer(GL_VERTEX_ARRAY, GL_FLOAT, mod->bsp.vertex_buffer);

	R_BindBuffer(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, mod->bsp.texcoord_buffer);

	if (r_state.multitexture_enabled) {  /* lightmap texcoords */
		R_SelectTexture(&r_state.lightmap_texunit);
		R_BindBuffer(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, mod->bsp.lmtexcoord_buffer);
		R_SelectTexture(&r_state.texture_texunit);
	}

	if (r_state.lighting_enabled)  /* normal vectors for lighting */
		R_BindBuffer(GL_NORMAL_ARRAY, GL_FLOAT, mod->bsp.normal_buffer);
}

static void R_ResetVertexArrayState (void)
{
	R_BindBuffer(0, 0, 0);

	R_BindDefaultArray(GL_VERTEX_ARRAY);

	R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);

	if (r_state.multitexture_enabled) {
		R_SelectTexture(&r_state.lightmap_texunit);
		R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);
		R_SelectTexture(&r_state.texture_texunit);
	}

	if (r_state.lighting_enabled)
		R_BindDefaultArray(GL_NORMAL_ARRAY);

	bufferMapTile = -1;
}

/**
 * @brief Set the surface state according to surface flags and bind the texture
 * @sa R_DrawSurfaces
 */
static void R_SetSurfaceState (const mBspSurface_t *surf)
{
	if (r_state.blend_enabled) {  /* alpha blend */
		float a;
		switch (surf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66)) {
		case SURF_TRANS33:
			a = 0.33;
			break;
		case SURF_TRANS66:
			a = 0.66;
			break;
		default:  /* both flags mean use the texture's alpha channel */
			a = 1.0;
			break;
		}

		qglColor4f(1.0, 1.0, 1.0, a);
	}

	if (surf->texinfo->flags & SURF_ALPHATEST) {  /* alpha test */
		R_EnableAlphaTest(qtrue);
	} else {
		R_EnableAlphaTest(qfalse);
	}

	if (r_vertexbuffers->integer)
		R_SetVertexBufferState(surf);
	else
		R_SetVertexArrayState(surf);

	R_BindTexture(surf->texinfo->image->texnum);  /* texture */

	if (r_state.multitexture_enabled)  /* lightmap */
		R_BindLightmapTexture(surf->lightmaptexturenum);

	R_CheckError();
}

/**
 * @brief Use the vertex, texture and normal arrays to draw a surface
 * @sa R_DrawSurfaces
 */
static inline void R_DrawSurface (const mBspSurface_t *surf)
{
	qglDrawArrays(GL_POLYGON, surf->index, surf->numedges);

	c_brush_polys++;
}

/**
 * @brief General surface drawing function, that draw the surface chains
 * @note The needed states for the surfaces must be set before you call this
 * @sa R_DrawSurface
 * @sa R_SetSurfaceState
 */
static void R_DrawSurfaces (const mBspSurfaces_t *surfs)
{
	int i;

	for (i = 0; i < surfs->count; i++) {
		R_SetSurfaceState(surfs->surfaces[i]);
		R_DrawSurface(surfs->surfaces[i]);
	}

	/* and restore array pointers */
	R_ResetVertexArrayState();

	R_Color(NULL);
}

/**
 * @brief Draw the surface chain with multitexture enabled and light enabled
 * @sa R_DrawAlphaSurfaces
 */
void R_DrawOpaqueSurfaces (const mBspSurfaces_t *surfs)
{
	if (!surfs->count)
		return;

	R_EnableMultitexture(qtrue);

	R_EnableLighting(qtrue);
	R_DrawSurfaces(surfs);
	R_EnableLighting(qfalse);

	R_EnableMultitexture(qfalse);
}

/**
 * @brief Draw the surfaces via warp shader
 * @sa R_DrawAlphaWarpSurfaces
 */
void R_DrawOpaqueWarpSurfaces (mBspSurfaces_t *surfs)
{
	if (!surfs->count)
		return;

	R_EnableWarp(qtrue);
	R_DrawSurfaces(surfs);
	R_EnableWarp(qfalse);
}

/**
 * @brief Draw the surface chain with multitexture enabled and blend enabled
 * @sa R_DrawOpaqueSurfaces
 */
void R_DrawAlphaSurfaces (const mBspSurfaces_t *surfs)
{
	if (!surfs->count)
		return;

	assert(r_state.blend_enabled);
	R_EnableMultitexture(qtrue);
	R_DrawSurfaces(surfs);
	R_EnableMultitexture(qfalse);
}

/**
 * @brief Draw the alpha surfaces via warp shader and with blend enabled
 * @sa R_DrawOpaqueWarpSurfaces
 */
void R_DrawAlphaWarpSurfaces (mBspSurfaces_t *surfs)
{
	if (!surfs->count)
		return;

	assert(r_state.blend_enabled);
	R_EnableWarp(qtrue);
	R_DrawSurfaces(surfs);
	R_EnableWarp(qfalse);
}
