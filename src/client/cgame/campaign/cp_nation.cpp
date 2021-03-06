/**
 * @file
 * @brief Nation code
 * @note Functions with NAT_*
 */

/*
Copyright (C) 2002-2014 UFO: Alien Invasion.

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
#include "../../../shared/parse.h"
#include "cp_campaign.h"
#include "cp_geoscape.h"
#include "cp_ufo.h"
#include "cp_time.h"
#include "save/save_nation.h"
#include "../../ui/ui_main.h" /* ui_node_linechart.h */
#include "../../ui/node/ui_node_linechart.h" /* lineStrip_t */

nation_t* NAT_GetNationByIDX (const int index)
{
	assert(index >= 0);
	assert(index < ccs.numNations);

	return &ccs.nations[index];
}

/**
 * @brief Return a nation-pointer by the nations id (nation_t->id) text.
 * @param[in] nationID nation id as defined in (nation_t->id)
 * @return nation_t pointer or nullptr if nothing found (=error).
 */
nation_t* NAT_GetNationByID (const char* nationID)
{
	if (!nationID) {
		Com_Printf("NAT_GetNationByID: nullptr nationID\n");
		return nullptr;
	}
	for (int i = 0; i < ccs.numNations; i++) {
		nation_t* nation = NAT_GetNationByIDX(i);
		if (Q_streq(nation->id, nationID))
			return nation;
	}

	Com_Printf("NAT_GetNationByID: Could not find nation '%s'\n", nationID);

	/* No matching nation found - ERROR */
	return nullptr;
}


/**
 * @brief Lower happiness of nations depending on alien activity.
 * @note Daily called
 * @sa CP_BuildBaseGovernmentLeave
 */
void NAT_UpdateHappinessForAllNations (const float minhappiness)
{
	MIS_Foreach(mission) {
		nation_t* nation = GEO_GetNation(mission->pos);
		/* Difficulty modifier range is [0, 0.02f] */

		/* Some non-water location have no nation */
		if (nation) {
			float happinessFactor;
			const nationInfo_t* stats = NAT_GetCurrentMonthInfo(nation);
			switch (mission->stage) {
			case STAGE_TERROR_MISSION:
			case STAGE_SUBVERT_GOV:
			case STAGE_RECON_GROUND:
			case STAGE_SPREAD_XVI:
			case STAGE_HARVEST:
				happinessFactor = HAPPINESS_ALIEN_MISSION_LOSS;
				break;
			default:
				/* mission is not active on earth or does not have any influence
				 * on the nation happiness, skip this mission */
				continue;
			}

			NAT_SetHappiness(minhappiness, nation, stats->happiness + happinessFactor);
			Com_DPrintf(DEBUG_CLIENT, "Happiness of nation %s decreased: %.02f\n", nation->name, stats->happiness);
		}
	}
}

/**
 * @brief Get the actual funding of a nation
 * @param[in] nation Pointer to the nation
 * @param[in] month idx of the month -- 0 for current month
 * @return actual funding of a nation
 * @sa CL_NationsMaxFunding
 */
int NAT_GetFunding (const nation_t* const nation, int month)
{
	assert(month >= 0);
	assert(month < MONTHS_PER_YEAR);
	return nation->maxFunding * nation->stats[month].happiness;
}

/**
 * @brief Get the current month nation stats
 * @param[in] nation Pointer to the nation
 * @return The current month nation stats
 */
const nationInfo_t* NAT_GetCurrentMonthInfo (const nation_t* const nation)
{
	return &nation->stats[0];
}

/**
 * @brief Translates the nation happiness float value to a string
 * @param[in] nation
 * @return Translated happiness string
 * @note happiness is between 0 and 1.0
 */
const char* NAT_GetHappinessString (const nation_t* nation)
{
	const nationInfo_t* stats = NAT_GetCurrentMonthInfo(nation);
	if (stats->happiness < 0.015)
		return _("Giving up");
	else if (stats->happiness < 0.025)
		return _("Furious");
	else if (stats->happiness < 0.04)
		return _("Angry");
	else if (stats->happiness < 0.06)
		return _("Mad");
	else if (stats->happiness < 0.10)
		return _("Upset");
	else if (stats->happiness < 0.20)
		return _("Tolerant");
	else if (stats->happiness < 0.30)
		return _("Neutral");
	else if (stats->happiness < 0.50)
		return _("Content");
	else if (stats->happiness < 0.70)
		return _("Pleased");
	else if (stats->happiness < 0.95)
		return _("Happy");
	else
		return _("Exuberant");
}

/**
 * @brief Updates the nation happiness
 * @param[in] minhappiness Minimum value of mean happiness before the game is lost
 * @param[in] nation The nation to update the happiness for
 * @param[in] happiness The new happiness value to set for the given nation
 */
void NAT_SetHappiness (const float minhappiness, nation_t* nation, const float happiness)
{
	const char* oldString = NAT_GetHappinessString(nation);
	const char* newString;
	nationInfo_t* stats = &nation->stats[0];
	const float oldHappiness = stats->happiness;
	const float middleHappiness = (minhappiness + 1.0) / 2;
	notify_t notifyType = NT_NUM_NOTIFYTYPE;

	stats->happiness = happiness;
	if (stats->happiness < 0.0f)
		stats->happiness = 0.0f;
	else if (stats->happiness > 1.0f)
		stats->happiness = 1.0f;

	newString = NAT_GetHappinessString(nation);

	if (oldString != newString) {
		Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer),
			_("Nation %s changed happiness from %s to %s"), _(nation->name), oldString, newString);
		notifyType = NT_HAPPINESS_CHANGED;
	} else if (oldHappiness > middleHappiness && happiness < middleHappiness) {
		Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer),
			_("Nation %s changed happiness to %s"), _(nation->name), newString);
		notifyType = NT_HAPPINESS_PLEASED;
	} else if (happiness < minhappiness && oldHappiness > minhappiness) {
		Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), /** @todo need to more specific message */
			_("Happiness of nation %s is %s and less than minimal happiness allowed to the campaign"), _(nation->name), newString);
		notifyType = NT_HAPPINESS_MIN;
	} else {
		return;
	}

	MSO_CheckAddNewMessage(notifyType, _("Nation changed happiness"), cp_messageBuffer);
}

/**
 * @brief Nation saving callback
 * @param[out] p XML Node structure, where we write the information to
 */
bool NAT_SaveXML (xmlNode_t* p)
{
	xmlNode_t* n = cgi->XML_AddNode(p, SAVE_NATION_NATIONS);

	for (int i = 0; i < ccs.numNations; i++) {
		nation_t* nation = NAT_GetNationByIDX(i);

		if (!nation)
			continue;

		xmlNode_t* s = cgi->XML_AddNode(n, SAVE_NATION_NATION);
		cgi->XML_AddString(s, SAVE_NATION_ID, nation->id);
		for (int j = 0; j < MONTHS_PER_YEAR; j++) {
			const nationInfo_t* stats = &nation->stats[j];

			if (!stats->inuse)
				continue;

			xmlNode_t* ss = cgi->XML_AddNode(s, SAVE_NATION_MONTH);
			cgi->XML_AddInt(ss, SAVE_NATION_MONTH_IDX, j);
			cgi->XML_AddFloat(ss, SAVE_NATION_HAPPINESS, stats->happiness);
			cgi->XML_AddInt(ss, SAVE_NATION_XVI, stats->xviInfection);
		}
	}
	return true;
}

/**
 * @brief Nation loading xml callback
 * @param[in] p XML Node structure, where we get the information from
 */
bool NAT_LoadXML (xmlNode_t* p)
{
	xmlNode_t* n;
	xmlNode_t* s;

	n = cgi->XML_GetNode(p, SAVE_NATION_NATIONS);
	if (!n)
		return false;

	/* nations loop */
	for (s = cgi->XML_GetNode(n, SAVE_NATION_NATION); s; s = cgi->XML_GetNextNode(s, n, SAVE_NATION_NATION)) {
		xmlNode_t* ss;
		nation_t* nation = NAT_GetNationByID(cgi->XML_GetString(s, SAVE_NATION_ID));

		if (!nation)
			return false;

		/* month loop */
		for (ss = cgi->XML_GetNode(s, SAVE_NATION_MONTH); ss; ss = cgi->XML_GetNextNode(ss, s, SAVE_NATION_MONTH)) {
			int monthIDX = cgi->XML_GetInt(ss, SAVE_NATION_MONTH_IDX, MONTHS_PER_YEAR);
			nationInfo_t* stats = &nation->stats[monthIDX];

			if (monthIDX < 0 || monthIDX >= MONTHS_PER_YEAR)
				return false;

			stats->inuse = true;
			stats->happiness = cgi->XML_GetFloat(ss, SAVE_NATION_HAPPINESS, 0.0);
			stats->xviInfection = cgi->XML_GetInt(ss, SAVE_NATION_XVI, 0);
		}
	}
	return true;
}

/*==========================================
Parsing
==========================================*/

static const value_t nation_vals[] = {
	{"name", V_TRANSLATION_STRING, offsetof(nation_t, name), 0},
	{"pos", V_POS, offsetof(nation_t, pos), MEMBER_SIZEOF(nation_t, pos)},
	{"color", V_COLOR, offsetof(nation_t, color), MEMBER_SIZEOF(nation_t, color)},
	{"funding", V_INT, offsetof(nation_t, maxFunding), MEMBER_SIZEOF(nation_t, maxFunding)},
	{"happiness", V_FLOAT, offsetof(nation_t, stats[0].happiness), MEMBER_SIZEOF(nation_t, stats[0].happiness)},
	{"soldiers", V_INT, offsetof(nation_t, maxSoldiers), MEMBER_SIZEOF(nation_t, maxSoldiers)},
	{"scientists", V_INT, offsetof(nation_t, maxScientists), MEMBER_SIZEOF(nation_t, maxScientists)},
	{"workers", V_INT, offsetof(nation_t, maxWorkers), MEMBER_SIZEOF(nation_t, maxWorkers)},
	{"pilots", V_INT, offsetof(nation_t, maxPilots), MEMBER_SIZEOF(nation_t, maxPilots)},

	{nullptr, V_NULL, 0, 0}
};

/**
 * @brief Parse the nation data from script file
 * @param[in] name Name or ID of the found nation
 * @param[in] text The text of the nation node
 * @sa nation_vals
 * @sa CL_ParseScriptFirst
 * @note write into cp_campaignPool - free on every game restart and reparse
 */
void CL_ParseNations (const char* name, const char** text)
{
	if (ccs.numNations >= MAX_NATIONS) {
		Com_Printf("CL_ParseNations: nation number exceeding maximum number of nations: %i\n", MAX_NATIONS);
		return;
	}

	/* search for nations with same name */
	int i;
	for (i = 0; i < ccs.numNations; i++) {
		const nation_t* n = NAT_GetNationByIDX(i);
		if (Q_streq(name, n->id))
			break;
	}
	if (i < ccs.numNations) {
		Com_Printf("CL_ParseNations: nation def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	/* initialize the nation */
	nation_t* nation = &ccs.nations[ccs.numNations];
	OBJZERO(*nation);
	nation->idx = ccs.numNations;
	nation->stats[0].inuse = true;

	if (Com_ParseBlock(name, text, nation, nation_vals, cp_campaignPool)) {
		ccs.numNations++;

		Com_DPrintf(DEBUG_CLIENT, "...found nation %s\n", name);
		nation->id = Mem_PoolStrDup(name, cp_campaignPool, 0);
	}
}

/**
 * @brief Finds a city by it's scripted identifier
 * @param[in] cityId Scripted ID of the city
 */
city_t* CITY_GetById (const char* cityId)
{
	LIST_Foreach(ccs.cities, city_t, city) {
		if (Q_streq(cityId, city->id))
			return city;
	}
	return nullptr;
}

/**
 * @brief Finds a city by it's geoscape coordinates
 * @param[in] pos Position of the city
 */
city_t* CITY_GetByPos (vec2_t pos)
{
	LIST_Foreach(ccs.cities, city_t, city) {
		if (Vector2Equal(pos, city->pos))
			return city;
	}
	return nullptr;
}

static const value_t city_vals[] = {
	{"name", V_TRANSLATION_STRING, offsetof(city_t, name), 0},
	{"pos", V_POS, offsetof(city_t, pos), MEMBER_SIZEOF(city_t, pos)},

	{nullptr, V_NULL, 0, 0}
};

/**
 * @brief Parse the city data from script file
 * @param[in] name ID of the found nation
 * @param[in] text The text of the nation node
 */
void CITY_Parse (const char* name, const char** text)
{
	city_t newCity;

	/* search for cities with same name */
	if (CITY_GetById(name)) {
		Com_Printf("CITY_Parse: city def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	OBJZERO(newCity);
	newCity.idx = ccs.numCities;

	if (Com_ParseBlock(name, text, &newCity, city_vals, cp_campaignPool)) {
		ccs.numCities++;
		newCity.id = Mem_PoolStrDup(name, cp_campaignPool, 0);
		/* Add city to the list */
		LIST_Add(&ccs.cities, newCity);
	}
}

/**
 * @brief Checks the parsed nations and cities for errors
 * @return false if there are errors - true otherwise
 */
bool NAT_ScriptSanityCheck (void)
{
	int error = 0;

	/* Check if there is at least one map fitting city parameter for terror mission */
	LIST_Foreach(ccs.cities, city_t, city) {
		bool cityCanBeUsed = false;
		bool parametersFit = false;
		ufoType_t ufoTypes[UFO_MAX];
		int numTypes;
		const mapDef_t* md;

		if (!city->name) {
			error++;
			Com_Printf("...... city '%s' has no name\n", city->id);
		}

		if (MapIsWater(GEO_GetColor(city->pos, MAPTYPE_TERRAIN, nullptr))) {
			error++;
			Com_Printf("...... city '%s' has a position in the water\n", city->id);
		}

		numTypes = CP_TerrorMissionAvailableUFOs(nullptr, ufoTypes);

		MapDef_ForeachSingleplayerCampaign(md) {
			if (md->storyRelated)
				continue;

			if (GEO_PositionFitsTCPNTypes(city->pos, md->terrains, md->cultures, md->populations, nullptr)) {
				/* this map fits city parameter, check if we have some terror mission UFOs available for this map */
				parametersFit = true;

				/* no UFO on this map (LIST_ContainsString doesn't like empty string) */
				if (!md->ufos) {
					continue;
				}

				/* loop must be backward, as we remove items */
				for (int i = numTypes - 1 ; i >= 0; i--) {
					if (cgi->LIST_ContainsString(md->ufos, cgi->Com_UFOTypeToShortName(ufoTypes[i]))) {
						REMOVE_ELEM(ufoTypes, i, numTypes);
					}
				}
			}
			if (numTypes == 0) {
				cityCanBeUsed = true;
				break;
			}
		}

		if (!cityCanBeUsed) {
			error++;
			Com_Printf("...... city '%s' can't be used in game: it has no map fitting parameters\n", city->id);
			if (parametersFit) {
				Com_Printf("      (No map fitting");
				for (int i = 0 ; i < numTypes; i++)
					Com_Printf(" %s", cgi->Com_UFOTypeToShortName(ufoTypes[i]));
				Com_Printf(")\n");
			}
			GEO_PrintParameterStringByPos(city->pos);
		}
	}

	return !error;
}

/*=====================================
Menu functions
=====================================*/


static void CP_NationStatsClick_f (void)
{
	int num;

	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", cgi->Cmd_Argv(0));
		return;
	}

	/* Which entry in the list? */
	num = atoi(cgi->Cmd_Argv(1));
	if (num < 0 || num >= ccs.numNations)
		return;

	cgi->UI_PushWindow("nations");
	cgi->Cbuf_AddText("nation_select %i\n", num);
}

/** Space for month-lines with 12 points for each nation. */
static screenPoint_t fundingPts[MAX_NATIONS][MONTHS_PER_YEAR];
static int usedFundPtslist = 0;
/** Space for 1 line (2 points) for each nation. */
static screenPoint_t colorLinePts[MAX_NATIONS][2];
static int usedColPtslists = 0;

static const vec4_t graphColors[MAX_NATIONS] = {
	{1.0, 0.5, 0.5, 1.0},
	{0.5, 1.0, 0.5, 1.0},
	{0.5, 0.5, 1.0, 1.0},
	{1.0, 0.0, 0.0, 1.0},
	{0.0, 1.0, 0.0, 1.0},
	{0.0, 0.0, 1.0, 1.0},
	{1.0, 1.0, 0.0, 1.0},
	{0.0, 1.0, 1.0, 1.0}
};
static const vec4_t graphColorSelected = {1, 1, 1, 1};

/**
 * @brief Search the maximum (current) funding from all the nations (in all logged months).
 * @note nation->maxFunding is _not_ the real funding value.
 * @return The maximum funding value.
 * @todo Extend to other values?
 * @sa NAT_GetFunding
 */
static int CL_NationsMaxFunding (void)
{
	int max = 0;

	for (int n = 0; n < ccs.numNations; n++) {
		const nation_t* nation = NAT_GetNationByIDX(n);
		for (int m = 0; m < MONTHS_PER_YEAR; m++) {
			if (nation->stats[m].inuse) {
				const int funding = NAT_GetFunding(nation, m);
				if (max < funding)
					max = funding;
			} else {
				/* Abort this months-loops */
				break;
			}
		}
	}
	return max;
}

static int selectedNation = 0;

static lineStrip_t fundingLineStrip[MAX_NATIONS];

/**
 * @brief Draws a graph for the funding values over time.
 * @param[in] nation The nation to draw the graph for.
 * @param[in] node A pointer to a "linestrip" node that we want to draw in.
 * @param[out] funding Funding graph data in a lineStrip_t
 * @param[in] maxFunding The upper limit of the graph - all values will be scaled to this one.
 * @param[in] color If this is -1 draw the line for the current selected nation
 * @todo Somehow the display of the months isn't really correct right now (straight line :-/)
 */
static void CL_NationDrawStats (const nation_t* nation, uiNode_t* node, lineStrip_t* funding, int maxFunding, int color)
{
	int minFunding = 0;
	int ptsNumber = 0;
	float dy;

	if (!nation || !node)
		return;

	/** @todo should be into the chart node code */
	int width	= (int)node->box.size[0];
	int height	= (int)node->box.size[1];
	int dx = (int)(width / MONTHS_PER_YEAR);

	if (minFunding != maxFunding)
		dy = (float) height / (maxFunding - minFunding);
	else
		dy = 0;

	/* Generate pointlist. */
	/** @todo Sort this in reverse? -> Having current month on the right side? */
	for (int m = 0; m < MONTHS_PER_YEAR; m++) {
		if (nation->stats[m].inuse) {
			const int fund = NAT_GetFunding(nation, m);
			fundingPts[usedFundPtslist][m].x = (m * dx);
			fundingPts[usedFundPtslist][m].y = height - dy * (fund - minFunding);
			ptsNumber++;
		} else {
			break;
		}
	}

	/* Guarantee displayable data even for only one month */
	if (ptsNumber == 1) {
		/* Set the second point half the distance to the next month to the right - small horizontal line. */
		fundingPts[usedFundPtslist][1].x = fundingPts[usedFundPtslist][0].x + (int)(0.5 * width / MONTHS_PER_YEAR);
		fundingPts[usedFundPtslist][1].y = fundingPts[usedFundPtslist][0].y;
		ptsNumber++;
	}

	/* Link graph to node */
	funding->pointList = (int*)fundingPts[usedFundPtslist];
	funding->numPoints = ptsNumber;
	if (color < 0) {
		const nation_t* nation = NAT_GetNationByIDX(selectedNation);
		cgi->Cvar_Set("mn_nat_symbol", "nations/%s", nation->id);
		Vector4Copy(graphColorSelected, funding->color);
	} else {
		Vector4Copy(graphColors[color], funding->color);
	}

	usedFundPtslist++;
}

static lineStrip_t colorLineStrip[MAX_NATIONS];

/**
 * @brief Shows the current nation list + statistics.
 * @note See menu_stats.ufo
 */
static void CL_NationStatsUpdate_f (void)
{
	int i;
	int dy = 10;

	usedColPtslists = 0;

	uiNode_t* colorNode = cgi->UI_GetNodeByPath("nations.nation_graph_colors");
	if (colorNode) {
		dy = (int)(colorNode->box.size[1] / MAX_NATIONS);
	}

	for (i = 0; i < ccs.numNations; i++) {
		const nation_t* nation = NAT_GetNationByIDX(i);
		lineStrip_t* color = &colorLineStrip[i];
		const int funding = NAT_GetFunding(nation, 0);

		OBJZERO(*color);

		if (i > 0)
			colorLineStrip[i - 1].next = color;

		if (selectedNation == i) {
			cgi->UI_ExecuteConfunc("nation_marksel %i", i);
		} else {
			cgi->UI_ExecuteConfunc("nation_markdesel %i", i);
		}
		cgi->Cvar_Set(va("mn_nat_name%i",i), "%s", _(nation->name));
		cgi->Cvar_Set(va("mn_nat_fund%i",i), _("%i c"), funding);

		if (colorNode) {
			colorLinePts[usedColPtslists][0].x = 0;
			colorLinePts[usedColPtslists][0].y = (int)colorNode->box.size[1] - (int)colorNode->box.size[1] + dy * i;
			colorLinePts[usedColPtslists][1].x = (int)colorNode->box.size[0];
			colorLinePts[usedColPtslists][1].y = colorLinePts[usedColPtslists][0].y;

			color->pointList = (int*)colorLinePts[usedColPtslists];
			color->numPoints = 2;

			if (i == selectedNation) {
				Vector4Copy(graphColorSelected, color->color);
			} else {
				Vector4Copy(graphColors[i], color->color);
			}

			usedColPtslists++;
		}
	}

	cgi->UI_RegisterLineStrip(LINESTRIP_COLOR, &colorLineStrip[0]);

	/* Hide unused nation-entries. */
	for (i = ccs.numNations; i < MAX_NATIONS; i++) {
		cgi->UI_ExecuteConfunc("nation_hide %i", i);
	}

	/** @todo Display summary of nation info */

	/* Display graph of nations-values so far. */
	uiNode_t* graphNode = cgi->UI_GetNodeByPath("nations.nation_graph_funding");
	if (graphNode) {
		const int maxFunding = CL_NationsMaxFunding();
		usedFundPtslist = 0;
		for (i = 0; i < ccs.numNations; i++) {
			const nation_t* nation = NAT_GetNationByIDX(i);
			lineStrip_t* funding = &fundingLineStrip[i];

			/* init the structure */
			OBJZERO(*funding);

			if (i > 0)
				fundingLineStrip[i - 1].next = funding;

			if (i == selectedNation) {
				CL_NationDrawStats(nation, graphNode, funding, maxFunding, -1);
			} else {
				CL_NationDrawStats(nation, graphNode, funding, maxFunding, i);
			}
		}

		cgi->UI_RegisterLineStrip(LINESTRIP_FUNDING, &fundingLineStrip[0]);
	}
}

/**
 * @brief Select nation and display all relevant information for it.
 */
static void CL_NationSelect_f (void)
{
	int nat;

	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <nat_idx>\n", cgi->Cmd_Argv(0));
		return;
	}

	nat = atoi(cgi->Cmd_Argv(1));
	if (nat < 0 || nat >= ccs.numNations) {
		Com_Printf("Invalid nation index: %is\n",nat);
		return;
	}

	selectedNation = nat;
	CL_NationStatsUpdate_f();
}

#ifdef DEBUG
/**
 * @brief Debug function to list all cities in game.
 * @note Called with debug_listcities.
 */
static void NAT_ListCities_f (void)
{
	LIST_Foreach(ccs.cities, city_t, city) {
		Com_Printf("City '%s' -- position (%0.1f, %0.1f)\n", city->id, city->pos[0], city->pos[1]);
		GEO_PrintParameterStringByPos(city->pos);
	}
}

/**
 * @brief Scriptfunction to list all parsed nations with their current values
 * @note called with debug_listnations
 */
static void NAT_NationList_f (void)
{
	for (int i = 0; i < ccs.numNations; i++) {
		const nation_t* nation = &ccs.nations[i];
		Com_Printf("Nation ID: %s\n", nation->id);
		Com_Printf("...max-funding %i c\n", nation->maxFunding);
		Com_Printf("...happiness %0.2f\n", nation->stats[0].happiness);
		Com_Printf("...xviInfection %i\n", nation->stats[0].xviInfection);
		Com_Printf("...max-soldiers %i\n", nation->maxSoldiers);
		Com_Printf("...max-scientists %i\n", nation->maxScientists);
		Com_Printf("...max-workers %i\n", nation->maxWorkers);
		Com_Printf("...max-pilots %i\n", nation->maxPilots);
		Com_Printf("...color r:%.2f g:%.2f b:%.2f a:%.2f\n", nation->color[0], nation->color[1], nation->color[2], nation->color[3]);
		Com_Printf("...pos x:%.0f y:%.0f\n", nation->pos[0], nation->pos[1]);
	}
}
#endif

/**
 * @brief Update the nation data from all parsed nation each month
 * @note give us nation support by:
 * * credits
 * * new soldiers
 * * new scientists
 * * new pilots
 * @note Called from CP_CampaignRun
 * @sa CP_CampaignRun
 * @sa B_CreateEmployee
 * @todo This is very redundant with CP_StatsUpdate_f. Investigate and clean up.
 */
void NAT_HandleBudget (const campaign_t* campaign)
{
	char message[1024];
	int cost;
	int totalIncome = 0;
	int totalExpenditure = 0;
	int initialCredits = ccs.credits;
	const salary_t* salary = &campaign->salaries;

	/* Refreshes the pilot global list.  Pilots who are already hired are unchanged, but all other
	 * pilots are replaced.  The new pilots is evenly distributed between the nations that are happy (happiness > 0). */
	E_RefreshUnhiredEmployeeGlobalList(EMPL_PILOT, true);

	for (int i = 0; i < ccs.numNations; i++) {
		const nation_t* nation = NAT_GetNationByIDX(i);
		const nationInfo_t* stats = NAT_GetCurrentMonthInfo(nation);
		const int funding = NAT_GetFunding(nation, 0);
		int newScientists = 0, newSoldiers = 0, newPilots = 0, newWorkers = 0;

		totalIncome += funding;

		for (int j = 0; 0.25 + j < (float) nation->maxScientists * stats->happiness * ccs.curCampaign->employeeRate; j++) {
			/* Create a scientist, but don't auto-hire her. */
			E_CreateEmployee(EMPL_SCIENTIST, nation, nullptr);
			newScientists++;
		}

		if (stats->happiness > 0) {
			for (int j = 0; 0.25 + j < (float) nation->maxSoldiers * stats->happiness * ccs.curCampaign->employeeRate; j++) {
				/* Create a soldier. */
				E_CreateEmployee(EMPL_SOLDIER, nation, nullptr);
				newSoldiers++;
			}
		}
		/* pilots */
		if (stats->happiness > 0) {
			for (int j = 0; 0.25 + j < (float) nation->maxPilots * stats->happiness * ccs.curCampaign->employeeRate; j++) {
				/* Create a pilot. */
				E_CreateEmployee(EMPL_PILOT, nation, nullptr);
				newPilots++;
			}
		}

		for (int j = 0; 0.25 + j * 2 < (float) nation->maxWorkers * stats->happiness * ccs.curCampaign->employeeRate; j++) {
			/* Create a worker. */
			E_CreateEmployee(EMPL_WORKER, nation, nullptr);
			newWorkers++;
		}

		Com_sprintf(message, sizeof(message), _("Gained %i %s, %i %s, %i %s, %i %s, and %i %s from nation %s (%s)"),
					funding, ngettext("credit", "credits", funding),
					newScientists, ngettext("scientist", "scientists", newScientists),
					newSoldiers, ngettext("soldier", "soldiers", newSoldiers),
					newPilots, ngettext("pilot", "pilots", newPilots),
					newWorkers, ngettext("worker", "workers", newWorkers),
					_(nation->name), NAT_GetHappinessString(nation));
		MS_AddNewMessage(_("Notice"), message);
	}

	for (int i = 0; i < MAX_EMPL; i++) {
		cost = 0;
		E_Foreach(i, employee) {
			if (!employee->isHired())
				continue;
			const rank_t* rank = CL_GetRankByIdx(employee->chr.score.rank);
			cost += CP_GetSalaryBaseEmployee(salary, employee->getType())
					+ rank->level * CP_GetSalaryRankBonusEmployee(salary, employee->getType());
		}
		totalExpenditure += cost;

		if (cost == 0)
			continue;

		Com_sprintf(message, sizeof(message), _("Paid %i credits to: %s"), cost, E_GetEmployeeString((employeeType_t)i, 1));
		MS_AddNewMessage(_("Notice"), message);
	}

	cost = 0;
	AIR_Foreach(aircraft) {
		if (aircraft->status == AIR_CRASHED)
			continue;
		cost += aircraft->price * salary->aircraftFactor / salary->aircraftDivisor;
	}
	totalExpenditure += cost;

	if (cost != 0) {
		Com_sprintf(message, sizeof(message), _("Paid %i credits for aircraft"), cost);
		MS_AddNewMessage(_("Notice"), message);
	}

	base_t* base = nullptr;
	while ((base = B_GetNext(base)) != nullptr) {
		cost = CP_GetSalaryUpKeepBase(salary, base);
		totalExpenditure += cost;

		Com_sprintf(message, sizeof(message), _("Paid %i credits for upkeep of %s"), cost, base->name);
		MS_AddNewMessage(_("Notice"), message);
	}

	cost = CP_GetSalaryAdministrative(salary);
	Com_sprintf(message, sizeof(message), _("Paid %i credits for administrative overhead."), cost);
	totalExpenditure += cost;
	MS_AddNewMessage(_("Notice"), message);

	if (initialCredits < 0) {
		const float interest = initialCredits * campaign->salaries.debtInterest;

		cost = (int)ceil(interest);
		Com_sprintf(message, sizeof(message), _("Paid %i credits in interest on your debt."), cost);
		totalExpenditure += cost;
		MS_AddNewMessage(_("Notice"), message);
	}
	CP_UpdateCredits(ccs.credits - totalExpenditure + totalIncome);
	CP_GameTimeStop();
}

/**
 * @brief Backs up each nation's relationship values.
 * @note Right after the copy the stats for the current month are the same as the ones from the (end of the) previous month.
 * They will change while the curent month is running of course :)
 * @todo other stuff to back up?
 */
void NAT_BackupMonthlyData (void)
{
	/**
	 * Back up nation relationship .
	 * "inuse" is copied as well so we do not need to set it anywhere.
	 */
	for (int nat = 0; nat < ccs.numNations; nat++) {
		nation_t* nation = NAT_GetNationByIDX(nat);

		for (int i = MONTHS_PER_YEAR - 1; i > 0; i--) {	/* Reverse copy to not overwrite with wrong data */
			nation->stats[i] = nation->stats[i - 1];
		}
	}
}

static const cmdList_t nationCmds[] = {
	{"nation_stats_click", CP_NationStatsClick_f, nullptr},
	{"nation_update", CL_NationStatsUpdate_f, "Shows the current nation list + statistics."},
	{"nation_select", CL_NationSelect_f, "Select nation and display all relevant information for it."},
#ifdef DEBUG
	{"debug_listcities", NAT_ListCities_f, "Debug function to list all cities in game."},
	{"debug_listnations", NAT_NationList_f, "List all nations on the game console"},
#endif
	{nullptr, nullptr, nullptr}
};
/**
 * @brief Init actions for nation-subsystem
 */
void NAT_InitStartup (void)
{
	Cmd_TableAddList(nationCmds);
}

/**
 * @brief Closing actions for nation-subsystem
 */
void NAT_Shutdown (void)
{
	cgi->LIST_Delete(&ccs.cities);

	Cmd_TableRemoveList(nationCmds);
}
