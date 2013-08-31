/**
 * @file
 * @brief UFOAI web interface management. Authentification as well as
 * uploading/downloading stuff to and from your account is done here.
 */

/*
Copyright (C) 2002-2013 UFO: Alien Invasion.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.m

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "web_team.h"
#include "web_main.h"
#include "../cl_shared.h"
#include "../cgame/cl_game_team.h"
#include "../cl_http.h"
#include "../ui/ui_main.h"
#include "../../shared/parse.h"

cvar_t *web_username;
cvar_t *web_password;
cvar_t *web_userid;
cvar_t *web_teamdownloadurl;
cvar_t *web_teamdeleteurl;
cvar_t *web_teamuploadurl;
cvar_t *web_teamlisturl;

/**
 * @brief Uploads a team onto the ufoai server
 * @note Only teams from the home directory of the user are working. A user may
 * not upload default delivered teams from without the game installation folder.
 */
void WEB_UploadTeam_f (void)
{
	if (!WEB_CheckAuth())
		return;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <slotindex>\n", Cmd_Argv(0));
		return;
	}

	int index = atoi(Cmd_Argv(1));
	if (index < 0)
		return;

	char filename[MAX_OSPATH];
	if (!GAME_GetTeamFileName(index, filename, sizeof(filename)))
		return;

	const char *fullPath = va("%s/%s", FS_Gamedir(), filename);
	/* we ignore this, because this file is not in the users save path,
	 * but part of the game data. Don't upload this. */
	if (!FS_FileExists("%s", fullPath)) {
		Com_Printf("no user data: '%s'\n", fullPath);
		return;
	}

	if (WEB_PutFile("team", fullPath, web_teamuploadurl->string)) {
		UI_ExecuteConfunc("teamlist_uploadsuccessful");
		Com_Printf("uploaded the team '%s'\n", filename);
	} else {
		UI_ExecuteConfunc("teamlist_uploadfailed");
		Com_Printf("failed to upload the team from file '%s'\n", filename);
	}
}

/**
 * @brief Delete one of your own teams from the server
 */
void WEB_DeleteTeam_f (void)
{
	if (!WEB_CheckAuth())
		return;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <teamid>\n", Cmd_Argv(0));
		return;
	}

	const int teamId = atoi(Cmd_Argv(1));
	if (teamId < 0)
		return;

	char url[576];
	Com_sprintf(url, sizeof(url), "%s?teamid=%i", web_teamdeleteurl->string, teamId);
	if (WEB_GetURL(url, nullptr))
		Com_Printf("deleted the team '%i'\n", teamId);
	else
		Com_Printf("failed to delete the team '%i' from the server\n", teamId);
}

/**
 * @brief Downloads a particular team and informs the ui (by calling confuncs) about the result.
 * @note This method does a duplicate check on the downloaded file.
 */
void WEB_DownloadTeam_f (void)
{
	if (Cmd_Argc() != 3) {
		Com_Printf("Usage: %s <userid> <id>\n", Cmd_Argv(0));
		return;
	}
	const char* userId = Cmd_Argv(1);
	const int id = atoi(Cmd_Argv(2));
	char filename[MAX_QPATH];
	if (!GAME_TeamGetFreeFilename(filename, sizeof(filename))) {
		Com_Printf("No free filenames for a new team\n");
		return;
	}
	char urlId[256];
	if (!Q_strreplace(web_teamdownloadurl->string, "$id$", va("%02d", id), urlId, sizeof(urlId))) {
		Com_Printf("$id$ is missing in the url\n");
		return;
	}
	char url[256];
	if (!Q_strreplace(urlId, "$userid$", userId, url, sizeof(url))) {
		Com_Printf("$userid$ is missing in the url\n");
		return;
	}
	qFILE f;
	FS_OpenFile(filename, &f, FILE_WRITE);
	if (!f.f) {
		Com_Printf("Could not open the target file\n");
		FS_CloseFile(&f);
		return;
	}

	/* no login is needed here */
	if (!HTTP_GetToFile(url, f.f)) {
		Com_Printf("team download failed from '%s'\n", url);
		FS_CloseFile(&f);
		return;
	}
	FS_CloseFile(&f);

	if (Com_CheckDuplicateFile(filename, "save/*.mpt")) {
		FS_RemoveFile(va("%s/%s", FS_Gamedir(), filename));
		UI_ExecuteConfunc("teamlist_downloadduplicate");
		return;
	}

	UI_ExecuteConfunc("teamlist_downloadsuccessful %s", filename);
}

/**
 * @brief The http callback for the team list command
 * @param[in] responseBuf The team list in ufo script format
 * @param[in] userdata This is null in this case
 */
static void WEB_ListTeamsCallback (const char *responseBuf, void *userdata)
{
	if (!responseBuf) {
		Com_Printf("Could not load the teamlist\n");
		return;
	}

	struct entry_s {
		int id;
		char name[MAX_VAR];
		int userId;
	};

	const value_t values[] = {
		{"id", V_INT, offsetof(entry_s, id), MEMBER_SIZEOF(entry_s, id)},
		{"userid", V_INT, offsetof(entry_s, userId), MEMBER_SIZEOF(entry_s, userId)},
		{"name", V_STRING, offsetof(entry_s, name), 0},
		{nullptr, V_NULL, 0, 0}
	};

	entry_s entry;

	const char *token = Com_Parse(&responseBuf);
	if (token[0] != '{') {
		Com_Printf("invalid token: '%s' - expected {\n", token);
		return;
	}
	int num = 0;
	int level = 1;
	for (;;) {
		token = Com_Parse(&responseBuf);
		if (!responseBuf)
			break;
		if (token[0] == '{') {
			level++;
			OBJZERO(entry);
			continue;
		}
		if (token[0] == '}') {
			level--;
			if (level == 0) {
				break;
			}
			UI_ExecuteConfunc("teamlist_add %i \"%s\" %i %i", entry.id, entry.name, entry.userId, (entry.userId == web_userid->integer) ? 1 : 0);
			num++;
			continue;
		}

		const value_t *value;
		for (value = values; value->string; value++) {
			if (!Q_streq(token, value->string))
				continue;
			token = Com_Parse(&responseBuf);
			if (!responseBuf)
				break;
			/* found a normal particle value */
			Com_EParseValue(&entry, token, value->type, value->ofs, value->size);
			break;
		}
		if (value->string == nullptr) {
			Com_DPrintf(DEBUG_CLIENT, "invalid value found: '%s'\n", token);
			// skip the invalid value for this and try to go on
			token = Com_Parse(&responseBuf);
		}
	}
	Com_Printf("found %i teams\n", num);
}

/**
 * @brief Shows all downloadable teams on the ufoai server
 * @sa WEB_ListTeamsCallback
 */
void WEB_ListTeams_f (void)
{
	static const char *url = web_teamlisturl->string;
	if (!WEB_GetURL(url, WEB_ListTeamsCallback))
		Com_Printf("failed to query the team list\n");
}
