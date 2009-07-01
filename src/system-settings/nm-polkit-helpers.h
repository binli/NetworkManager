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
 * (C) Copyright 2008 Novell, Inc.
 * (C) Copyright 2008 Red Hat, Inc.
 */

#ifndef NM_POLKIT_HELPERS_H
#define NM_POLKIT_HELPERS_H

#include <polkit-dbus/polkit-dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#define NM_SYSCONFIG_POLICY_ACTION "org.freedesktop.network-manager-settings.system.modify"

PolKitContext *create_polkit_context   (GError **error);
gboolean       check_polkit_privileges (DBusGConnection *dbus_connection,
								PolKitContext *pol_ctx,
								DBusGMethodInvocation *context,
								GError **err);

#endif /* NM_POLKIT_HELPERS_H */