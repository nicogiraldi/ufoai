/**
 * @file m_actions.c
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

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

#include "../../game/q_shared.h"
#include "../client.h"
#include "../cl_input.h"
#include "m_main.h"
#include "m_internal.h"
#include "m_parse.h"
#include "m_input.h"
#include "node/m_node_abstractnode.h"

menuNode_t *focusNode;

#define MAX_CONFUNC_SIZE 512
/**
 * @brief Executes confunc - just to identify those confuncs in the code - in
 * this frame.
 * @param[in] confunc The confunc id that should be executed
 */
void MN_ExecuteConfunc (const char *fmt, ...)
{
	va_list ap;
	char confunc[MAX_CONFUNC_SIZE];

	va_start(ap, fmt);
	Q_vsnprintf(confunc, sizeof(confunc), fmt, ap);
	Cmd_ExecuteString(confunc);
	va_end(ap);
}

/**
 * @brief read a property name from an input buffer to an output
 * @return last position into the input buffer if we find property, else NULL
 */
inline static const char* MN_GenCommandReadProperty (const char* input, char* output, int outputSize)
{
	assert(*input == '<');
	outputSize--;
	input++;

	while (outputSize && *input != '\0' && *input != ' ' && *input != '>') {
		*output++ = *input++;
		outputSize--;
	}
	if (*input != '>') {
		return NULL;
	}
	*output = '\0';
	return ++input;
}

/**
 * @brief Replace injection identifiers (e.g. <eventParam>) by a value
 * @note The injection identifier can be every node value - e.g. <image> or <width>.
 * It's also possible to do something like
 * @code cmd "set someCvar <min>/<max>"
 */
static const char* MN_GenInjectedString (const menuNode_t* source, qboolean useCmdParam, const char* input, qboolean addNewLine)
{
	static char cmd[256];
	int length = sizeof(cmd) - ((addNewLine)?2:1);
	static char propertyName[64];
	const char *cin = input;
	char *cout = cmd;

	while (length && cin[0] != '\0') {
		if (cin[0] == '<') {
			/* read propertyName between '<' and '>' */
			const char *next = MN_GenCommandReadProperty(cin, propertyName, sizeof(propertyName));
			if (next) {
				/* cvar injection */
				if (!Q_strncmp(propertyName, "cvar:", 5)) {
					cvar_t *cvar = Cvar_Get(propertyName + 5, "", 0, NULL);
					int l = snprintf(cout, length, "%s", cvar->string);
					cout += l;
					cin = next;
					length -= l;
					continue;

				/* source property injection */
				} else if (source) {
					/* find peroperty definition */
					const value_t *property = MN_NodeGetPropertyDefinition(source, propertyName);
					if (property) {
						const char* value;
						int l;
						/* only allow common type or cvar */
						if ((property->type & V_SPECIAL_TYPE) != 0 && (property->type & V_SPECIAL_TYPE) != V_SPECIAL_CVAR) {
							Sys_Error("MN_GenCommand: Unsuported type injection for property '%s', node '%s.%s'", property->string, source->menu->name, source->name);
						}
						/* inject the property value */
						value = Com_ValueToStr((const void*)source, property->type, property->ofs);
						l = snprintf(cout, length, "%s", value);
						cout += l;
						cin = next;
						length -= l;
						continue;
					} else if (useCmdParam) {
						const int arg = atoi(propertyName);
						if (Cmd_Argc() >= arg) {
							const int l = snprintf(cout, length, "%s", Cmd_Argv(arg));
							cout += l;
							cin = next;
							length -= l;
							continue;
						}
					}
				}
			}
		}
		*cout++ = *cin++;
		length--;
	}

	/* is buffer too small? */
	assert(cin[0] == '\0');

	if (addNewLine)
		*cout++ = '\n';

	*cout++ = '\0';

	return cmd;
}

inline static void MN_ExecuteSetAction (const menuNode_t* source, const menu_t* menu, qboolean useCmdParam, const menuAction_t* action)
{
	const char* path;
	byte *value;
	menuNode_t *node;

	if (!action->data)
		return;

	if (action->type.param1 == EA_CVARNAME) {
		char cvarName[MAX_VAR];
		const char* textValue;
		assert(action->type.param2 == EA_VALUE);
		Q_strncpyz(cvarName, MN_GenInjectedString(source, useCmdParam, action->data, qfalse), MAX_VAR);

		textValue = action->data;
		textValue += ALIGN(strlen(action->data) + 1);
		textValue = MN_GenInjectedString(source, useCmdParam, textValue, qfalse);
		if (textValue[0] == '_') {
			textValue = gettext(textValue + 1);
		}
		Cvar_Set(cvarName, textValue);
		return;
	}

	/* search the node */
	path = MN_GenInjectedString(source, useCmdParam, action->data, qfalse);
	switch (action->type.param1) {
	case EA_THISMENUNODENAMEPROPERTY:
		node = MN_GetNode(menu, path);
		if (!node) {
			Com_Printf("MN_ExecuteSetAction: node \"%s.%s\" doesn't exist\n", menu->name, path);
			return;
		}
		break;
	case EA_PATHPROPERTY:
		node = MN_GetNodeByPath(path);
		if (!node) {
			Com_Printf("MN_ExecuteSetAction: node \"%s\" doesn't exist\n", path);
			return;
		}
		break;
	default:
		node = NULL;
		Sys_Error("MN_ExecuteSetAction: Invalid actiontype");
	}

	value = action->data;
	value += ALIGN(strlen(action->data) + 1);

	/* decode text value */
	if (action->type.param2 == EA_VALUE) {
		const char* v = MN_GenInjectedString(source, useCmdParam, (char*) value, qfalse);
		MN_NodeSetProperty(node, action->scriptValues, v);
		return;
	}

	/* decode RAW value */
	assert(action->type.param2 == EA_RAWVALUE);
	if ((action->scriptValues->type & V_SPECIAL_TYPE) == 0)
		Com_SetValue(node, (char *) value, action->scriptValues->type, action->scriptValues->ofs, action->scriptValues->size);
	else if ((action->scriptValues->type & V_SPECIAL_TYPE) == V_SPECIAL_CVAR) {
		void *mem = ((byte *) node + action->scriptValues->ofs);
		switch (action->scriptValues->type & V_BASETYPEMASK) {
		case V_FLOAT:
			**(float **) mem = *(float*) value;
			break;
		case V_INT:
			**(int **) mem = *(int*) value;
			break;
		default:
			*(byte **) mem = value;
		}
	} else if (action->scriptValues->type == V_SPECIAL_ACTION) {
		void *mem = ((byte *) node + action->scriptValues->ofs);
		*(menuAction_t**) mem = *(menuAction_t**)value;
	} else if (action->scriptValues->type == V_SPECIAL_ICONREF) {
		void *mem = ((byte *) node + action->scriptValues->ofs);
		*(menuIcon_t**) mem = *(menuIcon_t**)value;
	} else {
		assert(qfalse);
	}
}

/**
 * @todo remove 'menu' param when its possible
 */
static void MN_ExecuteInjectedAction (const menuNode_t* source, const menu_t* menu, qboolean useCmdParam, const menuAction_t* action)
{
	switch (action->type.op) {

	case EA_NULL:
		/* do nothing */
		break;

	case EA_CMD:
		/* execute a command */
		if (action->data)
			Cbuf_AddText(MN_GenInjectedString(source, useCmdParam, action->data, qtrue));
		break;

	case EA_CALL:
		/* call another function */
		MN_ExecuteActions(menu, **(menuAction_t ***) action->data);
		break;

	case EA_SET:
		MN_ExecuteSetAction(source, menu, useCmdParam, action);
		break;

	case EA_IF:
		if (MN_CheckCondition((menuDepends_t *) action->data)) {
			MN_ExecuteActions(menu, (const menuAction_t* const) action->scriptValues);
		}
		break;

	default:
		Sys_Error("unknown action type\n");
		break;
	}

}

/**
 * @sa MN_ParseAction
 */
void MN_ExecuteActions (const menu_t* const menu, const menuAction_t* const first)
{
	const menuAction_t *action;
	for (action = first; action; action = action->next) {
		MN_ExecuteInjectedAction(NULL, menu, qfalse, action);
	}
}

static inline void MN_ExecuteInjectedActions (const menuNode_t* source, qboolean useCmdParam, const menuAction_t* firstAction)
{
	static int callnumber = 0;
	const menuAction_t *action;
	if (callnumber++ > 20) {
		Com_Printf("MN_ExecuteInjectedActions: Possible recursion\n");
		return;
	}
	for (action = firstAction; action; action = action->next) {
		menu_t *menu = NULL;
		if (source) {
			menu = source->menu;
		}
		MN_ExecuteInjectedAction(source, menu, useCmdParam, action);
	}
	callnumber--;
}

/**
 * @brief allow to inject command param into cmd of confunc command
 */
void MN_ExecuteConFuncActions (const menuNode_t* source, const menuAction_t* firstAction)
{
	MN_ExecuteInjectedActions(source, qtrue, firstAction);
}

void MN_ExecuteEventActions (const menuNode_t* source, const menuAction_t* firstAction)
{
	MN_ExecuteInjectedActions(source, qfalse, firstAction);
}

/**
 * @sa MN_FocusExecuteActionNode
 * @note node must not be in menu
 */
static menuNode_t *MN_GetNextActionNode (menuNode_t* node)
{
	if (node)
		node = node->next;
	while (node) {
		if (MN_CheckVisibility(node) && !node->invis
		 && ((node->onClick && node->onMouseIn) || node->onMouseIn))
			return node;
		node = node->next;
	}
	return NULL;
}

/**
 * @sa MN_FocusExecuteActionNode
 * @sa MN_FocusNextActionNode
 * @sa IN_Frame
 * @sa Key_Event
 */
void MN_FocusRemove (void)
{
	if (mouseSpace != MS_MENU)
		return;

	if (MN_GetMouseCapture())
		return;

	if (focusNode)
		MN_ExecuteActions(focusNode->menu, focusNode->onMouseOut);
	focusNode = NULL;
}

/**
 * @brief Execute the current focused action node
 * @note Action nodes are nodes with click defined
 * @sa Key_Event
 * @sa MN_FocusNextActionNode
 */
qboolean MN_FocusExecuteActionNode (void)
{
	if (mouseSpace != MS_MENU)
		return qfalse;

	if (MN_GetMouseCapture())
		return qfalse;

	if (focusNode) {
		if (focusNode->onClick) {
			MN_ExecuteActions(focusNode->menu, focusNode->onClick);
		}
		MN_ExecuteActions(focusNode->menu, focusNode->onMouseOut);
		focusNode = NULL;
		return qtrue;
	}

	return qfalse;
}

/**
 * @sa MN_FocusRemove
 */
static qboolean MN_FocusSetNode (menuNode_t* node)
{
	if (!node) {
		MN_FocusRemove();
		return qfalse;
	}

	if (MN_GetMouseCapture())
		return qfalse;

	/* reset already focused node */
	MN_FocusRemove();

	focusNode = node;
	MN_ExecuteActions(node->menu, node->onMouseIn);

	return qtrue;
}

/**
 * @brief Set the focus to the next action node
 * @note Action nodes are nodes with click defined
 * @sa Key_Event
 * @sa MN_FocusExecuteActionNode
 */
qboolean MN_FocusNextActionNode (void)
{
	menu_t* menu;
	static int i = MAX_MENUSTACK + 1;	/* to cycle between all menus */

	if (mouseSpace != MS_MENU)
		return qfalse;

	if (MN_GetMouseCapture())
		return qfalse;

	if (i >= mn.menuStackPos)
		i = MN_GetVisibleMenuCount();

	assert(i >= 0);

	if (focusNode) {
		menuNode_t *node = MN_GetNextActionNode(focusNode);
		if (node)
			return MN_FocusSetNode(node);
	}

	while (i < mn.menuStackPos) {
		menu = mn.menuStack[i++];
		if (MN_FocusSetNode(MN_GetNextActionNode(menu->firstChild)))
			return qtrue;
	}
	i = MN_GetVisibleMenuCount();

	/* no node to focus */
	MN_FocusRemove();

	return qfalse;
}

/**
 * @brief Test if a string use an injection syntaxe
 * @todo improve the test
 */
qboolean MN_IsInjectedString(const char *string)
{
	assert(string);
	return string[0] == '<';
}

/**
 * @brief Set a new action to a node->menuAction_t pointer
 * @param[in] type EA_CMD
 * @param[in] data The data for this action - in case of EA_CMD this is the commandline
 * @note You first have to free existing node actions - only free those that are
 * not static in mn.menuActions array
 */
menuAction_t *MN_SetMenuAction (menuAction_t** action, int type, const void *data)
{
	if (*action)
		Sys_Error("There is already an action assigned\n");
	*action = (menuAction_t *)Mem_PoolAlloc(sizeof(**action), cl_menuSysPool, CL_TAG_MENU);
	(*action)->type.op = type;
	switch (type) {
	case EA_CMD:
		(*action)->data = Mem_PoolStrDup((const char *)data, cl_menuSysPool, CL_TAG_MENU);
		break;
	default:
		Sys_Error("Action type %i is not yet implemented", type);
	}

	return *action;
}
