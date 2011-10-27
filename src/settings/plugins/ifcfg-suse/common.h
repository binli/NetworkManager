
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager system settings service
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * (C) Copyright 2008 - 2010 Red Hat, Inc.
 * (C) Copyright 2011 SUSE.
 */

#ifndef __COMMON_H__
#define __COMMON_H__

#include <glib.h>

#define IFCFG_TAG "ifcfg-"
#define IFROUTE_TAG "ifroute-" /* routes used only for specific interface */
#define ROUTES_TAG "routes" /* routes used for every interface */

/* sysconfig's script also ignore below file*/
#define BAK_TAG ".bak"
#define TILDE_TAG "~"
#define ORIG_TAG ".orig"
#define REJ_TAG ".rej"
#define RPMNEW_TAG ".rpmnew"
#define RPMSAVE_TAG ".rpmsave"
#define SCPMBACKUP_TAG ".scpmbackup"

#define TYPE_ETHERNET "Ethernet"
#define TYPE_WIRELESS "Wireless"
#define TYPE_BRIDGE   "Bridge"
#define TYPE_BOND     "Bond"

#define IFCFG_DIR SYSCONFDIR"/sysconfig/network"

#define IFCFG_PLUGIN_ERROR (ifcfg_plugin_error_quark ())
GQuark ifcfg_plugin_error_quark (void);

#endif  /* __COMMON_H__ */
