/**
 * @file cl_menu.h
 * @brief Header for client menu implementation
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

#ifndef CLIENT_CL_MENU_H
#define CLIENT_CL_MENU_H

#include "menu/m_main.h"

/* prototype */
struct menu_s;

void MN_SetViewRect(const struct menu_s* menu);

void MN_InitStartup(void);

#endif /* CLIENT_CL_MENU_H */
