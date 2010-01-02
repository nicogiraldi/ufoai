/**
 * @file cl_renderer.h
 */

/*
All original material Copyright (C) 2002-2009 UFO: Alien Invasion.

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

#ifndef CLIENT_REF_H
#define CLIENT_REF_H

#include "../common/common.h"
#include "renderer/r_material.h"
#include "renderer/r_image.h"
#include "renderer/r_model.h"

#include <SDL.h>

/* e.g. used for sequences and particle editor */
#define RDF_NOWORLDMODEL    1
#define RDF_IRGOGGLES       2	/**< actor is using ir goggles and everything with RF_IRGOGGLES is visible for him */

/** entity->flags (render flags) */
#define RF_TRANSLUCENT      0x00000001
#define RF_BOX              0x00000002	/**< actor selection box */
#define RF_PATH             0x01000000	/**< pathing marker, debugging only */
#define RF_ARROW            0x02000000	/**< arrow, debugging only */

/** the following ent flags also draw entity effects */
#define RF_SHADOW           0x00000004	/**< shadow (when living) for this entity */
#define RF_BLOOD            0x00000008	/**< blood (when dead) for this entity */
#define RF_SELECTED         0x00000010	/**< selected actor */
#define RF_MEMBER           0x00000020	/**< actor in the same team */
#define RF_ALLIED           0x00000040	/**< actor in an allied team (controlled by another player) */
#define RF_ACTOR            0x00000080	/**< this is an actor */
#define RF_PULSE            0x00000100	/**< glowing entity */
#define RF_IRGOGGLES        0x00000200	/**< this is visible if the actor uses ir goggles */

#define EARTH_RADIUS 8192.0f
#define MOON_RADIUS 1024.0f

#define WEATHER_NONE	0
#define WEATHER_FOG 	1

#define VID_NORM_WIDTH		1024
#define VID_NORM_HEIGHT		768

#define MAX_PTL_ART		512
#define MAX_PTLS		2048

#define MAX_GL_LIGHTS 8

typedef struct {
	vec3_t origin;
	vec4_t color;
	vec4_t ambient;
	float radius;
} light_t;

/** @brief sustains are light flashes which slowly decay */
typedef struct sustain_s {
	light_t light;
	float time;
	float sustain;
} sustain_t;

/**
 * @note if @c scale is @c NULL and @c center is not, the @c center vector is used to autoscale the
 * model and the value of @c center is used as size.
 * @todo The above is more a hack - fix this.
 */
typedef struct {
	model_t *model;			/**< the loaded model */
	const char *name;		/**< model path (resolved in the renderer on model loading time) */

	float *origin;			/**< pointer to node/menumodel origin */
	float *angles;			/**< pointer to node/menumodel angles */
	float *scale;			/**< pointer to node/menumodel scale */
	float *center;			/**< pointer to node/menumodel center */

	int frame, oldframe;	/**< animation frames */
	float backlerp;			/**< linear interpolation from previous frame */

	int skin;				/**< skin number */
	int mesh;				/**< which mesh? @note md2 models only have one mesh */
	float *color;
} modelInfo_t;

typedef struct ptlCmd_s {
	byte cmd;
	byte type;
	int ref;
} ptlCmd_t;

typedef struct ptlDef_s {
	char name[MAX_VAR];
	ptlCmd_t *init, *run, *think, *round, *physics;
} ptlDef_t;

typedef struct ptlArt_s {
	byte type;
	byte frame;
	char name[MAX_VAR];
	int skin;
	union {
		const image_t *image;
		model_t *model;
	} art;
} ptlArt_t;

typedef struct ptl_s {
	qboolean inuse;			/**< particle active? */
	qboolean invis;			/**< is this particle invisible */

	ptlArt_t *pic;			/**< Picture link. */
	ptlArt_t *model;		/**< Model link. */

	byte blend;				/**< blend mode */
	byte style;				/**< style mode */
	vec2_t size;
	vec3_t scale;
	vec4_t color;
	vec3_t s;			/**< current position */
	vec3_t origin;		/**< start position - set initial s position to get this value */
	vec3_t offset;
	vec3_t angles;
	vec3_t lightColor;
	float lightIntensity;
	float lightSustain;
	int levelFlags;

	int skin;		/**< model skin to use for this particle */

	struct ptl_s* children;	/**< list of children */
	struct ptl_s* next;		/**< next peer in list */
	struct ptl_s* parent;   /**< pointer to parent */

	/* private */
	ptlDef_t *ctrl;
	int startTime;
	int frame, endFrame;
	float fps;	/**< how many frames per second (animate) */
	float lastFrame;	/**< time (in seconds) when the think function was last executed (perhaps this can be used to delay or speed up particle actions). */
	float tps; /**< think per second - call the think function tps times each second, the first call at 1/tps seconds */
	float lastThink;
	byte thinkFade, frameFade;
	float t;	/**< time that the particle has been active already */
	float dt;	/**< time increment for rendering this particle (delta time) */
	float life;	/**< specifies how long a particle will be active (seconds) */
	int rounds;	/**< specifies how many rounds a particle will be active */
	int roundsCnt;
	float scrollS;
	float scrollT;
	vec3_t a;	/**< acceleration vector */
	vec3_t v;	/**< velocity vector */
	vec3_t omega;	/**< the rotation vector for the particle (newAngles = oldAngles + frametime * omega) */
	qboolean physics;	/**< basic physics */
	qboolean autohide;	/**< only draw the particle if the current position is
						 * not higher than the current level (useful for weather
						 * particles) */
	qboolean stayalive;	/**< used for physics particles that hit the ground */
	qboolean weather;	/**< used to identify weather particles (can be switched
						 * off via cvar cl_particleweather) */
} ptl_t;

typedef struct {
	qboolean ready;	/**< false if on new level or vid restart - if this is true the map can be rendered */

	float fieldOfViewX, fieldOfViewY;
	vec3_t viewOrigin;
	vec3_t viewAngles;
	float time;					/**< time is used to auto animate */
	int rendererFlags;				/**< RDF_NOWORLDMODEL, etc */
	int worldlevel;
	int brushCount, aliasCount;

	int weather;				/**< weather effects */
	vec4_t fogColor;

	vec3_t ambientLightColor;		/**< from static lighting */

	trace_t trace;				/**< occlusion testing */
	struct entity_s *traceEntity;

	const char *mapZone;		/**< used to replace textures in base assembly */
} rendererData_t;

extern rendererData_t refdef;

/* threading state */
typedef enum {
	THREAD_DEAD,
	THREAD_IDLE,
	THREAD_CLIENT,
	THREAD_BSP,
	THREAD_RENDERER
} threadstate_t;

typedef struct renderer_threadstate_s {
	SDL_Thread *thread;
	threadstate_t state;
} renderer_threadstate_t;

extern renderer_threadstate_t r_threadstate;

struct model_s *R_RegisterModelShort(const char *name);
const image_t *R_RegisterImage(const char *name);

void R_Color(const vec4_t rgba);

void R_ModBeginLoading(const char *tiles, qboolean day, const char *pos, const char *mapName);
void R_SwitchModelMemPoolTag(void);

void R_LoadImage(const char *name, byte **pic, int *width, int *height);

void R_FontShutdown(void);
void R_FontInit(void);
void R_FontRegister(const char *name, int size, const char *path, const char *style);
void R_FontSetTruncationMarker(const char *marker);

void R_FontTextSize(const char *fontId, const char *text, int maxWidth, longlines_t method, int *width, int *height, int *lines, qboolean *isTruncated);
int R_FontDrawString(const char *fontId, int align, int x, int y, int absX, int absY, int maxWidth, int lineHeight, const char *c, int boxHeight, int scrollPos, int *curLine, longlines_t method);

#endif /* CLIENT_REF_H */
