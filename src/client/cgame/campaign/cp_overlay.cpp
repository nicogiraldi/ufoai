/**
 * @file
 * @brief Functions to generate and render overlay for geoscape
 * @todo update the alpha values for radar like the ones for the xvi map - it's much faster like this
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

#include "../../cl_shared.h"
#include "cp_campaign.h"
#include "cp_overlay.h"

/** Max alpha level - don't set this to 255 or nothing else will be visible below the mask */
static const int MAX_ALPHA_VALUE = 200;
static const int INITIAL_ALPHA_VALUE = 60;

static inline byte* CP_XVIGetAlpha (int x, int y)
{
	return &cgi->r_xviAlpha[y * XVI_WIDTH + x];
}

static inline byte* CP_RadarGet (int x, int y, bool source)
{
	if (source)
		return &cgi->r_radarSourcePic[y * RADAR_WIDTH + x];
	return &cgi->r_radarPic[y * RADAR_WIDTH + x];
}

static inline void CP_UploadXVI (void)
{
	cgi->R_UploadAlpha("***r_xvitexture***", cgi->r_xviAlpha);
}

void CP_GetXVIMapDimensions (int* width, int* height)
{
	*width = XVI_WIDTH;
	*height = XVI_HEIGHT;
}

void CP_SetXVILevel (int x, int y, int value)
{
	assert(x >= 0);
	assert(x < XVI_WIDTH);
	assert(y >= 0);
	assert(y < XVI_HEIGHT);

	if (!value)
		*CP_XVIGetAlpha(x, y) = 0;
	else
		*CP_XVIGetAlpha(x, y) = std::min(MAX_ALPHA_VALUE, value + INITIAL_ALPHA_VALUE);
}

int CP_GetXVILevel (int x, int y)
{
	assert(x >= 0);
	assert(x < XVI_WIDTH);
	assert(y >= 0);
	assert(y < XVI_HEIGHT);

	return std::max(0, *CP_XVIGetAlpha(x, y) - INITIAL_ALPHA_VALUE);
}

/**
 * @brief Set lower and upper value of an overlay (radar, xvi) row that can be modified when tracing a circle.
 * @param[in] pos Position of the center of circle.
 * @param[in] radius Radius of the circle to be drawn.
 * @param[in] height Height of the overlay (in pixel).
 * @param[out] yMin Pointer to the lower row of the overlay that should be changed.
 * @param[out] yMax Pointer to the higher row of the overlay that should be changed.
 * @note circle will be drawn between yMin (included) and yMin (excluded). So yMin and yMax are rounded
 * respectively by lower and upper value.
 */
static void CP_SetMinMaxOverlayRows (const vec2_t pos, float radius, const int height, int* yMin, int* yMax)
{
	const float radarHeightPerDegree = height / 180.0f;

	if (pos[1] + radius > 90) {
		*yMin = 0;
		*yMax = round((90 - pos[1] + radius) * radarHeightPerDegree);
	} else if (pos[1] - radius < -90) {
		*yMin = ceil((90 - pos[1] - radius) * radarHeightPerDegree);
		*yMax = height;
	} else {
		*yMin = ceil((90 - pos[1] - radius) * radarHeightPerDegree);
		*yMax = round((90 - pos[1] + radius) * radarHeightPerDegree);
	}

	/* a few assert to avoid buffer overflow */
	assert(*yMin >= 0);
	assert(*yMin <= *yMax);
	assert(*yMax <= height);			/* the loop will stop just BEFORE yMax, so use <= rather than < */
}

/**
 * @brief Return the half longitude affected when tracing a circle at a given latitude.
 * @param[in] centerPos center of the circle (radar coverage, XVI infection zone).
 * @param[in] radius radius of the circle.
 * @param[in] yLat latitude of current point (in radians).
 * @note This is an implementation of the following facts:
 * - the distance (on a sphere) between the center of the circle and the border of the circle
 *    at current latitude is equal to radius
 * - the border of the circle on a row has the same latitude than current latitude.
 */
static inline float CP_GetCircleDeltaLongitude (const vec2_t centerPos, float radius, const float yLat)
{
	const float angle = (cos(radius * torad) - sin(centerPos[1] * torad) * sin(yLat)) / (cos(centerPos[1] * torad) * cos(yLat));
	return fabs(angle) > 1.0f ? 180.0f : todeg * acos(angle);
}

/**
 * @brief Change the value of 1 pixel in XVI overlay, the new value is higher than old one.
 * @param[in] xMin Minimum column (this volumn will be reached).
 * @param[in] xMax Maximum column (this volumn won't be reached).
 * @param[in] centerPos Position of the center of the circle.
 * @param[in] y current row in XVI overlay.
 * @param[in] yLat Latitude (in degree) of the current Row.
 * @param[in] xviLevel Level of XVI at the center of the circle (ie at @c center ).
 * @param[in] radius Radius of the circle.
 */
static void CP_DrawXVIOverlayPixel (int xMin, int xMax, const vec2_t centerPos, int y, const float yLat, int xviLevel, float radius)
{
	int x;
	vec2_t currentPos;

	currentPos[1] = yLat;

	for (x = xMin; x < xMax; x++) {
		const int previousLevel = CP_GetXVILevel(x, y);
		float distance;
		int newLevel;

		currentPos[0] = 180.0f - 360.0f * x / ((float) XVI_WIDTH);
		distance = GetDistanceOnGlobe(centerPos, currentPos);
		newLevel = ceil((xviLevel * (radius - distance)) / radius);
		if (newLevel > previousLevel)
			CP_SetXVILevel(x, y, xviLevel);
	}
}

/**
 * @brief Draw XVI overlay for a given latitude between 2 longitudes.
 * @param[in] latMin,latMax Minimum and maximum latitude (in degree).
 * @param[in] center Position of the center of the circle.
 * @param[in] y current row in XVI overlay.
 * @param[in] yLat Latitude (in degree) of the current Row.
 * @param[in] xviLevel Level of XVI at the center of the circle (ie at @c center ).
 * @param[in] radius Radius of the circle.
 * @pre We assume latMax - latMin <= 360 degrees.
 */
static void CP_DrawXVIOverlayRow (float latMin, float latMax, const vec2_t center, int y, float yLat, int xviLevel, float radius)
{
	const float xviWidthPerDegree = XVI_WIDTH / 360.0f;

	assert(latMax - latMin <= 360 + EQUAL_EPSILON);

	/* if the disc we draw cross the left or right edge of the picture, we need to
	 * draw 2 part of circle on each side of the overlay */
	if (latMin < -180.0f) {
		int xMin = 0;
		int xMax = ceil((latMax + 180.0f) * xviWidthPerDegree);
		CP_DrawXVIOverlayPixel(xMin, xMax, center, y, yLat, xviLevel, radius);
		xMin = floor((latMin + 540.0f) * xviWidthPerDegree);
		xMax = RADAR_WIDTH;
		CP_DrawXVIOverlayPixel(xMin, xMax, center, y, yLat, xviLevel, radius);
	} else if (latMax > 180.0f) {
		int xMin = 0;
		int xMax = ceil((latMax - 180.0f) * xviWidthPerDegree);
		CP_DrawXVIOverlayPixel(xMin, xMax, center, y, yLat, xviLevel, radius);
		xMin = floor((latMin + 180.0f) * xviWidthPerDegree);
		xMax = RADAR_WIDTH;
		CP_DrawXVIOverlayPixel(xMin, xMax, center, y, yLat, xviLevel, radius);
	} else {
		const int xMin = floor((latMin + 180.0f) * xviWidthPerDegree);
		const int xMax = ceil((latMax + 180.0f) * xviWidthPerDegree);
		CP_DrawXVIOverlayPixel(xMin, xMax, center, y, yLat, xviLevel, radius);
	}
}

/**
 * @brief Applies spreading on xvi transparency channel centered on a given pos
 * @param pos The x/y-location in the flat XVI map
 * @param[in] xCenter,yCenter Position of the center of XVI spreading (in degrees)
 * @param factor The factor of the XVI raise
 * @sa R_DecreaseXVILevel
 * @note xvi rate is null when alpha = 0, max when alpha = maxAlpha
 * XVI spreads in circle, and the alpha value of one pixel indicates the XVI level of infection.
 * This is necessary to take into account a new event that would spread in the zone where XVI is already spread.
 */
static void CP_IncreaseXVILevel (const vec2_t pos, int xCenter, int yCenter, float factor)
{
	int xviLevel;								/**< XVI level rate at position pos */
	int y;										/**< current position (in pixel) */
	int yMax, yMin;								/**< Bounding box of the XVI zone to be drawn (circle) */
	float radius;								/**< radius of the new XVI circle (in degree) */

	/* Get xvi Level infection at pos */
	xviLevel = CP_GetXVILevel(xCenter, yCenter);
	/* Calculate radius of new spreading */
	if (xviLevel < MAX_ALPHA_VALUE - INITIAL_ALPHA_VALUE)
		xviLevel++;
	radius = sqrt(factor * xviLevel);

	/* Set minimum and maximum rows value we'll have to change */
	CP_SetMinMaxOverlayRows(pos, radius, XVI_HEIGHT, &yMin, &yMax);

	for (y = yMin; y < yMax; y++) {
		const float yLat = 90.0f - 180.0f * y / ((float) XVI_HEIGHT);
		const float deltaLong = CP_GetCircleDeltaLongitude(pos, radius, torad * yLat);

		CP_DrawXVIOverlayRow(-pos[0] - deltaLong, -pos[0] + deltaLong, pos, y, yLat, xviLevel, radius);
	}

	CP_UploadXVI();
}

/**
 * @sa R_IncreaseXVILevel
 */
void CP_DecreaseXVILevelEverywhere (void)
{
	/** x,y: current position (in pixel) */
	for (int y = 0; y < XVI_HEIGHT; y++) {
		for (int x = 0; x < XVI_WIDTH; x++) {
			const int xviLevel = CP_GetXVILevel(x, y);
			if (xviLevel > 0)
				CP_SetXVILevel(x, y, xviLevel - 1);
		}
	}

	CP_UploadXVI();
}

/**
 * @brief Convert the pos into degrees and increase XVI there.
 * @param pos The x/y-location in the flat XVI map
 * @param factor The factor of the XVI raise
 * @note xvi rate is null when alpha = 0, max when alpha = maxAlpha
 */
void CP_ChangeXVILevel (const vec2_t pos, float factor)
{
	const int xCenter = round((180 - pos[0]) * XVI_WIDTH / 360.0f);
	const int yCenter = round((90 - pos[1]) * XVI_HEIGHT / 180.0f);

	CP_IncreaseXVILevel(pos, xCenter, yCenter, factor);
}

/**
 * @brief Initialize XVI overlay on geoscape.
 * @note xvi rate is null when alpha = 0, max when alpha = maxAlpha
 */
void CP_InitializeXVIOverlay (void)
{
	/* data Pointer to the data containing values to store in XVI map. Can be nullptr for new games.
	 * This is only the alpha channel of the xvi map */
	memset(cgi->r_xviAlpha, 0, XVI_WIDTH * XVI_HEIGHT);
	CP_UploadXVI();
}

/*
 * @brief Radar overlay code description
 * The radar overlay is handled 2 times: bases radar range and aircraft radar range.
 * Bases radar range needs to be updated only when a radar facility is built or destroyed.
 * The base radar overlay is stored in r_radarSourcePic.
 * Aircraft radar overlay needs to be updated every time an aircraft moves, because the position of the radar moves.
 * this overlay is created by duplicating r_radarSourcePic, and then adding any aircraft radar coverage. It is stored in r_radarTexture
 * @sa RADAR_UpdateWholeRadarOverlay
 */

/**
 * @brief Initialize radar overlay on geoscape.
 * @param[in] source Initialize the source texture if true: if you are updating base radar overlay.
 * false if you are updating aircraft radar overlay (base radar overlay will be copied to aircraft radar overlay)
 */
void CP_InitializeRadarOverlay (bool source)
{
	/* Initialize Radar */
	if (source)
		memset(cgi->r_radarSourcePic, INITIAL_ALPHA_VALUE, RADAR_WIDTH * RADAR_HEIGHT);
	else
		memcpy(cgi->r_radarPic, cgi->r_radarSourcePic, RADAR_WIDTH * RADAR_HEIGHT);
}

/**
 * @brief Draw radar overlay for a given latitude between 2 longitudes.
 * @param[in] latMin,latMax Minimum and maximum latitude.
 * @param[in] y current row in radar overlay.
 * @param[in] source True if we must update the source of the radar coverage, false if the copy must be updated.
 * @pre We assume latMax - latMin <= 360 degrees.
 */
static void CP_DrawRadarOverlayRow (float latMin, float latMax, int y, byte alpha, bool source)
{
	const float radarWidthPerDegree = RADAR_WIDTH / 360.0f;
	int x;

	assert(latMax - latMin <= 360 + EQUAL_EPSILON);

	/* if the disc we draw cross the left or right edge of the picture, we need to
	 * draw 2 part of circle on each side of the overlay */
	if (latMin < -180.0f) {
		int xMin = 0;
		int xMax = ceil((latMax + 180.0f) * radarWidthPerDegree);
		for (x = xMin; x < xMax; x++) {
			byte* dest = CP_RadarGet(x, y, source);
			if (alpha < dest[3])
				dest[3] = alpha;
		}
		xMin = floor((latMin + 540.0f) * radarWidthPerDegree);
		xMax = RADAR_WIDTH;
		for (x = xMin; x < xMax; x++) {
			byte* dest = CP_RadarGet(x, y, source);
			if (alpha < dest[3])
				dest[3] = alpha;
		}
	} else if (latMax > 180.0f) {
		int xMin = 0;
		int xMax = ceil((latMax - 180.0f) * radarWidthPerDegree);
		for (x = xMin; x < xMax; x++) {
			byte* dest = CP_RadarGet(x, y, source);
			if (alpha < dest[3])
				dest[3] = alpha;
		}
		xMin = floor((latMin + 180.0f) * radarWidthPerDegree);
		xMax = RADAR_WIDTH;
		for (x = xMin; x < xMax; x++) {
			byte* dest = CP_RadarGet(x, y, source);
			if (alpha < dest[3])
				dest[3] = alpha;
		}
	} else {
		const int xMin = floor((latMin + 180.0f) * radarWidthPerDegree);
		const int xMax = ceil((latMax + 180.0f) * radarWidthPerDegree);
		for (x = xMin; x < xMax; x++) {
			byte* dest = CP_RadarGet(x, y, source);
			if (alpha < dest[3])
				dest[3] = alpha;
		}
	}
}

/**
 * @brief Add a radar coverage (base or aircraft) to radar overlay
 * @param[in] pos Position of the center of radar
 * @param[in] innerRadius Radius of the radar coverage
 * @param[in] outerRadius Radius of the outer radar coverage
 * @param[in] source True if we must update the source of the radar coverage, false if the copy must be updated.
 * @pre We assume outerRadius is smaller than 180 degrees
 */
void CP_AddRadarCoverage (const vec2_t pos, float innerRadius, float outerRadius, bool source)
{
	const byte innerAlpha = 0;					/**< Alpha of the inner radar range */
	const byte outerAlpha = 60;					/**< Alpha of the outer radar range */
	const float radarHeightPerDegree = RADAR_HEIGHT / 180.0f;
	int y;										/**< current position (in pixel) */
	int yMax, yMin;								/**< Bounding box of the inner radar zone */
	int outeryMax, outeryMin;					/**< Bounding box of the outer radar zone */

	/** @todo add EQUAL_EPSILON here? */
	assert(outerRadius < 180.0f);
	assert(outerRadius > innerRadius);

	/* Set minimum and maximum rows value we'll have to change */
	CP_SetMinMaxOverlayRows(pos, innerRadius, RADAR_HEIGHT, &yMin, &yMax);
	CP_SetMinMaxOverlayRows(pos, outerRadius, RADAR_HEIGHT, &outeryMin, &outeryMax);

	/* Draw upper part of the radar coverage */
	for (y = outeryMin; y < yMin; y++) {
		/* latitude of current point, in radian */
		const float yLat = torad * (90.0f - y / radarHeightPerDegree);
		float outerDeltaLong = CP_GetCircleDeltaLongitude(pos, outerRadius, yLat);

		/* Only the outer radar coverage is drawn at this latitude */
		CP_DrawRadarOverlayRow(-pos[0] - outerDeltaLong, -pos[0] + outerDeltaLong, y, outerAlpha, source);
	}

	/* Draw middle part of the radar coverage */
	for (y = yMin; y < yMax; y++) {
		/* latitude of current point, in radian */
		const float yLat = torad * (90.0f - y / radarHeightPerDegree);
		const float deltaLong = CP_GetCircleDeltaLongitude(pos, innerRadius, yLat);
		const float outerDeltaLong = CP_GetCircleDeltaLongitude(pos, outerRadius, yLat);

		/* At this latitude, there are 3 parts to draw: left outer radar, inner radar, and right outer radar */
		CP_DrawRadarOverlayRow(-pos[0] - outerDeltaLong, -pos[0] - deltaLong, y, outerAlpha, source);
		CP_DrawRadarOverlayRow(-pos[0] - deltaLong, -pos[0] + deltaLong, y, innerAlpha, source);
		CP_DrawRadarOverlayRow(-pos[0] + deltaLong, -pos[0] + outerDeltaLong, y, outerAlpha, source);
	}

	/* Draw lower part of the radar coverage */
	for (y = yMax; y < outeryMax; y++) {
		/* latitude of current point, in radian */
		const float yLat = torad * (90.0f - y / radarHeightPerDegree);
		const float outerDeltaLong = CP_GetCircleDeltaLongitude(pos, outerRadius, yLat);

		/* Only the outer radar coverage is drawn at this latitude */
		CP_DrawRadarOverlayRow(-pos[0] - outerDeltaLong, -pos[0] + outerDeltaLong, y, outerAlpha, source);
	}
}

/**
 * @brief Smooth radar coverage
 * @note allows to make texture pixels less visible.
 */
void CP_UploadRadarCoverage (void)
{
	cgi->R_SoftenTexture(cgi->r_radarPic, RADAR_WIDTH, RADAR_HEIGHT, 1);
	cgi->R_UploadAlpha("***r_radarTexture***", cgi->r_radarPic);
}
