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
 * Copyright (C) 2008 - 2011 Red Hat, Inc.
 * Copyright (C) 2011 SUSE.
 */

#include <string.h>
#include <sys/ioctl.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <ctype.h>
#include <sys/inotify.h>
#include <errno.h>
#include <linux/if.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <netinet/ether.h>

#ifndef __user
#define __user
#endif
#include <linux/types.h>
#include <wireless.h>
#undef __user

#include <glib.h>
#include <glib/gi18n.h>
#include <nm-utils.h>
#include "common.h"
#include "shvar.h"
#include "utils.h"
#include "reader.h"


#include <nm-connection.h>

static NMSetting *
make_connection_setting (const char *file,
                         shvarFile *ifcfg,
                         const char *type,
                         const char *suggested)
{
	NMSettingConnection *s_con;
	const char *ifcfg_name = NULL;
	char *new_id = NULL, *uuid = NULL, *value;
	char *start_mode = NULL;
	char *ifcfg_id;

	ifcfg_name = utils_get_ifcfg_name (file, TRUE);
	if (!ifcfg_name)
		return NULL;

	s_con = NM_SETTING_CONNECTION (nm_setting_connection_new ());

	/* Try the ifcfg file's internally defined name if available */
	ifcfg_id = svGetValue (ifcfg, "NAME", FALSE);
	if (ifcfg_id && strlen (ifcfg_id))
		g_object_set (s_con, NM_SETTING_CONNECTION_ID, ifcfg_id, NULL);

	if (!nm_setting_connection_get_id (s_con)) {
		if (suggested) {
			/* For cosmetic reasons, if the suggested name is the same as
			 * the ifcfg files name, don't use it.  Mainly for wifi so that
			 * the SSID is shown in the connection ID instead of just "wlan0".
			 */
			if (strcmp (ifcfg_name, suggested)) {
				new_id = g_strdup_printf ("%s %s (%s)", reader_get_prefix (), suggested, ifcfg_name);
				g_object_set (s_con, NM_SETTING_CONNECTION_ID, new_id, NULL);
			}
		}

		/* Use the ifcfg file's name as a last resort */
		if (!nm_setting_connection_get_id (s_con)) {
			new_id = g_strdup_printf ("%s %s", reader_get_prefix (), ifcfg_name);
			g_object_set (s_con, NM_SETTING_CONNECTION_ID, new_id, NULL);
		}
		g_free (new_id);
	}

	g_free (ifcfg_id);

	/* Try for a UUID key before falling back to hashing the file name */
	uuid = svGetValue (ifcfg, "UUID", FALSE);
	if (!uuid || !strlen (uuid)) {
		g_free (uuid);
		uuid = nm_utils_uuid_generate_from_string (ifcfg->fileName);
	}
	g_object_set (s_con,
	              NM_SETTING_CONNECTION_TYPE, type,
	              NM_SETTING_CONNECTION_UUID, uuid,
	              NULL);
	g_debug ("uuid is %s", uuid);
	g_free (uuid);

	/* STARTMODE */
	start_mode = svGetValue (ifcfg, "STARTMODE", FALSE);
	g_debug ("start_mode is %s", start_mode);
	if (!strcmp (start_mode, "auto") || 
			!strcmp (start_mode, "onboot") ||
			!strcmp (start_mode, "hotplug") ||
			!strcmp (start_mode, "nfsroot")) {
		g_object_set (s_con, NM_SETTING_CONNECTION_AUTOCONNECT,
				TRUE,
				NULL);
	} else {
		g_object_set (s_con, NM_SETTING_CONNECTION_AUTOCONNECT,
				FALSE,
				NULL);
	}

	return NM_SETTING (s_con);
}

static NMConnection *
wireless_connection_from_ifcfg (const char *file,
                                shvarFile *ifcfg,
                                gboolean nm_controlled,
                                char **unmanaged,
                                GError **error)
{
	NMConnection *connection = NULL;

	return connection;
}

static NMConnection *
wired_connection_from_ifcfg (const char *file,
                             shvarFile *ifcfg,
                             gboolean nm_controlled,
                             char **unmanaged,
                             GError **error)
{
	NMConnection *connection = NULL;
	NMSetting *con_setting = NULL;

	g_return_val_if_fail (file != NULL, NULL);
	g_return_val_if_fail (ifcfg != NULL, NULL);

	g_debug ("wired_connection_from_ifcfg...");
	connection = nm_connection_new ();
	if (!connection) {
		g_set_error (error, IFCFG_PLUGIN_ERROR, 0,
		             "Failed to allocate new connection for %s.", file);
		return NULL;
	}

	con_setting = make_connection_setting (file, ifcfg, NM_SETTING_WIRED_SETTING_NAME, NULL);
	if (!con_setting) {
		g_set_error (error, IFCFG_PLUGIN_ERROR, 0,
		             "Failed to create connection setting.");
		g_object_unref (connection);
		return NULL;
	}

	nm_connection_add_setting (connection, con_setting);

	if (!nm_connection_verify (connection, error)) {
		g_object_unref (connection);
		return NULL;
	}
	g_debug ("nm_connection_verify is okay");

	return connection;
}

static gboolean
is_wireless_device (const char *iface)
{
	int fd;
	struct iw_range range;
	struct iwreq wrq;
	gboolean is_wireless = FALSE;

	g_return_val_if_fail (iface != NULL, FALSE);
	g_debug ("is_wireless_device...");

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == -1)
		return FALSE;

	memset (&wrq, 0, sizeof (struct iwreq));
	memset (&range, 0, sizeof (struct iw_range));
	strncpy (wrq.ifr_name, iface, IFNAMSIZ);
	wrq.u.data.pointer = (caddr_t) &range;
	wrq.u.data.length = sizeof (struct iw_range);

	if (ioctl (fd, SIOCGIWRANGE, &wrq) == 0)
		is_wireless = TRUE;
	else {
		if (errno == EOPNOTSUPP)
			is_wireless = FALSE;
		else {
			/* Sigh... some wired devices (kvm/qemu) return EINVAL when the
			 * device is down even though it's not a wireless device.  So try
			 * IWNAME as a fallback.
			 */
			memset (&wrq, 0, sizeof (struct iwreq));
			strncpy (wrq.ifr_name, iface, IFNAMSIZ);
			if (ioctl (fd, SIOCGIWNAME, &wrq) == 0)
				is_wireless = TRUE;
		}
	}

	close (fd);
	return is_wireless;
}

static gboolean
is_bond_device (shvarFile *parsed)
{
	g_return_val_if_fail (parsed != NULL, FALSE);
	g_debug ("is_bond_device...");

	/* BONDING_MASTER set to 'yes' to identify this interface as a bonding interface */
	if (svTrueValue (parsed, "BONDING_MASTER", FALSE))
		return TRUE;

	return FALSE;
}

NMConnection *
connection_from_file (const char *filename,
                      const char *network_file,  /* for unit tests only */
                      const char *test_type,     /* for unit tests only */
                      char **unmanaged,
                      char **ifroutefile,
                      char **routesfile,
                      GError **out_error,
                      gboolean *ignore_error)
{
	NMConnection *connection = NULL;
	shvarFile *parsed = NULL;
	const char *ifcfg_name = NULL;
	char *type = NULL; /* interface type */
	char *name = NULL; /* description of the device */
	gboolean nm_controlled = TRUE;
	GError *error = NULL;

	g_debug ("connection_from_file(%s)...", filename);
	ifcfg_name = utils_get_ifcfg_name (filename, TRUE);
	if (!ifcfg_name || (strlen(ifcfg_name) == 0)) {
		g_set_error (out_error, IFCFG_PLUGIN_ERROR, 0,
		             "Ignoring connection '%s' because it's not a valid ifcfg file.",
					 filename);
		return NULL;
	}

	g_debug ("ifcfg_name is %s", ifcfg_name);

	if (!strcmp (ifcfg_name, "lo")) {
		if (ignore_error)
			*ignore_error = TRUE;
		g_set_error (&error, IFCFG_PLUGIN_ERROR, 0,
				"Ignoring loopback device config.");
		return NULL;
	}

	parsed = svNewFile (filename);
	if (!parsed) {
		g_set_error (out_error, IFCFG_PLUGIN_ERROR, 0,
		             "Couldn't parse file '%s'", filename);
		return NULL;
	}

	type = svGetValue (parsed, "INTERFACETYPE", FALSE);
	if (!type) {
		if (is_bond_device (parsed))
			type = g_strdup (TYPE_BOND);
		else if (is_wireless_device (ifcfg_name))
			type = g_strdup (TYPE_WIRELESS);
		else
			type = g_strdup (TYPE_ETHERNET);
	} else {
	}
	g_debug ("type is %s", type);

	name = svGetValue (parsed, "NAME", FALSE);
	g_debug ("name is %s", name);

	/* Construct the connection */
	if (!strcasecmp (type, TYPE_ETHERNET))
		connection = wired_connection_from_ifcfg (filename, parsed, nm_controlled, unmanaged, &error);
	else if (!strcasecmp (type, TYPE_WIRELESS))
		connection = wireless_connection_from_ifcfg (filename, parsed, nm_controlled, unmanaged, &error);
	else {
		g_set_error (&error, IFCFG_PLUGIN_ERROR, 0,
		             "Unknown connection type '%s'", type);
	}

done:
	svCloseFile (parsed);

	if (error && out_error)
		*out_error = error;
	else
		g_clear_error (&error);

	return connection;
}

const char *
reader_get_prefix (void)
{
	return _("System");
}

