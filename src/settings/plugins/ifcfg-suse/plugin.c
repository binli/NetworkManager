/* -*- Mode: C; tab-width: 5; indent-tabs-mode: t; c-basic-offset: 5 -*- */

/* NetworkManager system settings service
 *
 * Søren Sandmann <sandmann@daimi.au.dk>
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
 * (C) Copyright 2007 - 2009 Red Hat, Inc.
 * (C) Copyright 2007 - 2008 Novell, Inc.
 */

#include <config.h>
#include <string.h>

#include <gmodule.h>
#include <glib-object.h>
#include <gio/gio.h>

#include "plugin.h"
#include "nm-system-config-interface.h"

#include "utils.h"
#include "nm-ifcfg-suse-connection.h"

#define CONF_DHCP IFCFG_DIR "/dhcp"
#define HOSTNAME_FILE "/etc/HOSTNAME"

static void system_config_interface_init (NMSystemConfigInterface *system_config_interface_class);

G_DEFINE_TYPE_EXTENDED (SCPluginIfcfg, sc_plugin_ifcfg, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (NM_TYPE_SYSTEM_CONFIG_INTERFACE,
                                               system_config_interface_init))

#define SC_PLUGIN_IFCFG_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), SC_TYPE_PLUGIN_IFCFG, SCPluginIfcfgPrivate))


#define IFCFG_FILE_PATH_TAG "ifcfg-file-path"

typedef struct {
	GHashTable *connections;

	GFileMonitor *hostname_monitor;
	GFileMonitor *dhcp_monitor;
	GFileMonitor *ifcfg_monitor;

	guint monitor_id;

	char *hostname;
} SCPluginIfcfgPrivate;


typedef void (*FileChangedFn) (gpointer user_data);

typedef struct {
	FileChangedFn callback;
	gpointer user_data;
} FileMonitorInfo;

static void connection_new_or_changed (SCPluginIfcfg *plugin,
                                       const char *path,
                                       NMIfcfgSUSEConnection *existing);

static void
connection_unmanaged_changed (NMIfcfgSUSEConnection *connection,
                              GParamSpec *pspec,
                              gpointer user_data)
{
	g_signal_emit_by_name (SC_PLUGIN_IFCFG (user_data),
			NM_SYSTEM_CONFIG_INTERFACE_UNMANAGED_SPECS_CHANGED);
}

static void
connection_ifcfg_changed (NMIfcfgSUSEConnection *connection, gpointer user_data)
{
	SCPluginIfcfg *plugin = SC_PLUGIN_IFCFG (user_data);
	const char *path;

	path = nm_ifcfg_suse_connection_get_path (connection);
	g_return_if_fail (path != NULL);

	connection_new_or_changed (plugin, path, connection);
}

static NMIfcfgSUSEConnection *
_internal_new_connection (SCPluginIfcfg *self,
                          const char *path,
                          NMConnection *source,
                          GError **error)
{
	SCPluginIfcfgPrivate *priv = SC_PLUGIN_IFCFG_GET_PRIVATE (self);
	NMIfcfgSUSEConnection *connection;
	const char *cid;
	GError *local = NULL;
	gboolean ignore_error = FALSE;

	if (!source) {
		PLUGIN_PRINT (IFCFG_PLUGIN_NAME, "parsing %s ... ", path);
	}

	connection = nm_ifcfg_suse_connection_new (path, source, &local, &ignore_error);
	if (!connection) {
		if (!ignore_error) {
			PLUGIN_PRINT (IFCFG_PLUGIN_NAME, "    error: %s",
			              (local && local->message) ? local->message : "(unknown)");
		}
		g_propagate_error (error, local);
		return NULL;
	}

	cid = nm_connection_get_id (NM_CONNECTION (connection));
	g_debug ("cid is %s", cid);
	g_assert (cid);

	g_debug ("get_path is %s", nm_ifcfg_suse_connection_get_path(connection));
	g_hash_table_insert (priv->connections,
	                     (gpointer) nm_ifcfg_suse_connection_get_path (connection),
	                     connection);
	PLUGIN_PRINT (IFCFG_PLUGIN_NAME, "    read connection '%s'", cid);

	if (nm_ifcfg_suse_connection_get_unmanaged_spec (connection)) {
		PLUGIN_PRINT (IFCFG_PLUGIN_NAME, "Ignoring connection '%s' and its "
		              "device due to BOND/BRIDGE/VLAN.", cid);
	} else {
		/* Wait for the connection to become unmanaged once it knows the
		 * hardware IDs of its device, if/when the device gets plugged in.
		 */
		g_signal_connect (G_OBJECT (connection), "notify::" NM_IFCFG_SUSE_CONNECTION_UNMANAGED,
		                  G_CALLBACK (connection_unmanaged_changed), self);
	}

	/* watch changes of ifcfg hardlinks */
	g_signal_connect (G_OBJECT (connection), "ifcfg-changed",
	                  G_CALLBACK (connection_ifcfg_changed), self);

	return connection;
}

static void
read_connections (SCPluginIfcfg *plugin)
{
	GDir *dir;
	GError *err = NULL;

	g_debug ("read_connections...");
	dir = g_dir_open (IFCFG_DIR, 0, &err);
	if (dir) {
		const char *item;

		while ((item = g_dir_read_name (dir))) {
			char *full_path;

			if (utils_should_ignore_file (item, TRUE))
				continue;

			full_path = g_build_filename (IFCFG_DIR, item, NULL);
			if (utils_get_ifcfg_name (full_path, TRUE))
				_internal_new_connection (plugin, full_path, NULL, NULL);
			g_free (full_path);
		}

		g_dir_close (dir);
	} else {
		PLUGIN_WARN (IFCFG_PLUGIN_NAME, "Can not read directory '%s': %s", IFCFG_DIR, err->message);
		g_error_free (err);
	}
}

/* Monitoring */

/* Callback for nm_settings_connection_replace_and_commit. Report any errors
 * encountered when commiting connection settings updates. */
static void
commit_cb (NMSettingsConnection *connection, GError *error, gpointer unused) 
{
	if (error) {
		PLUGIN_WARN (IFCFG_PLUGIN_NAME, "    error updating: %s",
	             	 (error && error->message) ? error->message : "(unknown)");
	}
}

static void
remove_connection (SCPluginIfcfg *self, NMIfcfgSUSEConnection *connection)
{
	SCPluginIfcfgPrivate *priv = SC_PLUGIN_IFCFG_GET_PRIVATE (self);
	gboolean managed = FALSE;
	const char *path;

	g_return_if_fail (self != NULL);
	g_return_if_fail (connection != NULL);

	managed = !nm_ifcfg_suse_connection_get_unmanaged_spec (connection);
	path = nm_ifcfg_suse_connection_get_path (connection);

	g_object_ref (connection);
	g_hash_table_remove (priv->connections, path);
	nm_settings_connection_signal_remove (NM_SETTINGS_CONNECTION (connection));
	g_object_unref (connection);

	/* Emit unmanaged changes _after_ removing the connection */
	if (managed == FALSE)
		g_signal_emit_by_name (self, NM_SYSTEM_CONFIG_INTERFACE_UNMANAGED_SPECS_CHANGED);
}

static void
connection_new_or_changed (SCPluginIfcfg *self,
                           const char *path,
                           NMIfcfgSUSEConnection *existing)
{
	NMIfcfgSUSEConnection *new;
	GError *error = NULL;
	gboolean ignore_error = FALSE;
	const char *new_unmanaged = NULL, *old_unmanaged = NULL;

	g_return_if_fail (self != NULL);
	g_return_if_fail (path != NULL);

	if (!existing) {
		/* Completely new connection */
		new = _internal_new_connection (self, path, NULL, NULL);
		if (new) {
			if (nm_ifcfg_suse_connection_get_unmanaged_spec (new)) {
				g_signal_emit_by_name (self, NM_SYSTEM_CONFIG_INTERFACE_UNMANAGED_SPECS_CHANGED);
			} else {
				/* Only managed connections are announced to the settings service */
				g_signal_emit_by_name (self, NM_SYSTEM_CONFIG_INTERFACE_CONNECTION_ADDED, new);
			}
		}
		return;
	}

	new = (NMIfcfgSUSEConnection *) nm_ifcfg_suse_connection_new (path, NULL, &error, &ignore_error);
	if (!new) {
		/* errors reading connection; remove it */
		if (!ignore_error) {
			PLUGIN_WARN (IFCFG_PLUGIN_NAME, "    error: %s",
			             (error && error->message) ? error->message : "(unknown)");
		}
		g_clear_error (&error);

		PLUGIN_PRINT (IFCFG_PLUGIN_NAME, "removed %s.", path);
		remove_connection (self, existing);
		return;
	}

	/* Successfully read connection changes */
	old_unmanaged = nm_ifcfg_suse_connection_get_unmanaged_spec (NM_IFCFG_SUSE_CONNECTION (existing));
	new_unmanaged = nm_ifcfg_suse_connection_get_unmanaged_spec (NM_IFCFG_SUSE_CONNECTION (new));

	/* When interface is unmanaged or the connections and unmanaged specs are the same
	 * there's nothing to do */
	if (   (g_strcmp0 (old_unmanaged, new_unmanaged) == 0 && new_unmanaged != NULL)
	    || (   nm_connection_compare (NM_CONNECTION (existing),
	                                  NM_CONNECTION (new),
	                                  NM_SETTING_COMPARE_FLAG_IGNORE_AGENT_OWNED_SECRETS |
	                                    NM_SETTING_COMPARE_FLAG_IGNORE_NOT_SAVED_SECRETS)
	        && g_strcmp0 (old_unmanaged, new_unmanaged) == 0)) {

		g_object_unref (new);
		return;
	}

	PLUGIN_PRINT (IFCFG_PLUGIN_NAME, "updating %s", path);

	if (new_unmanaged) {
		if (!old_unmanaged) {
			/* Unexport the connection by telling the settings service it's
			 * been removed, and notify the settings service by signalling that
			 * unmanaged specs have changed.
			 */
			nm_settings_connection_signal_remove (NM_SETTINGS_CONNECTION (existing));
			/* Remove the path so that claim_connection() doesn't complain later when
			 * interface gets managed and connection is re-added. */
			nm_connection_set_path (NM_CONNECTION (existing), NULL);

			g_object_set (existing, NM_IFCFG_SUSE_CONNECTION_UNMANAGED, new_unmanaged, NULL);
			g_signal_emit_by_name (self, NM_SYSTEM_CONFIG_INTERFACE_UNMANAGED_SPECS_CHANGED);
		}
	} else {
		if (old_unmanaged) {  /* now managed */
			const char *cid;

			cid = nm_connection_get_id (NM_CONNECTION (new));
			g_assert (cid);

			PLUGIN_PRINT (IFCFG_PLUGIN_NAME, "Managing connection '%s' and its "
			              "device because NM_CONTROLLED was true.", cid);
			g_signal_emit_by_name (self, NM_SYSTEM_CONFIG_INTERFACE_CONNECTION_ADDED, existing);
		}

		nm_settings_connection_replace_and_commit (NM_SETTINGS_CONNECTION (existing),
		                                           NM_CONNECTION (new),
		                                           commit_cb, NULL);

		/* Update unmanaged status */
		g_object_set (existing, NM_IFCFG_SUSE_CONNECTION_UNMANAGED, new_unmanaged, NULL);
		g_signal_emit_by_name (self, NM_SYSTEM_CONFIG_INTERFACE_UNMANAGED_SPECS_CHANGED);
	}
	g_object_unref (new);
}

static void
dir_changed (GFileMonitor *monitor,
		   GFile *file,
		   GFile *other_file,
		   GFileMonitorEvent event_type,
		   gpointer user_data)
{
	SCPluginIfcfg *plugin = SC_PLUGIN_IFCFG (user_data);
	SCPluginIfcfgPrivate *priv = SC_PLUGIN_IFCFG_GET_PRIVATE (plugin);
	char *path, *name;
	NMIfcfgSUSEConnection *connection;

	path = g_file_get_path (file);
	if (utils_should_ignore_file (path, FALSE)) {
		g_free (path);
		return;
	}

	/* Given any ifcfg, ifroute, or routes file, get the ifcfg file path */
	name = utils_get_ifcfg_path (path);
	g_free (path);
	if (name) {
		connection = g_hash_table_lookup (priv->connections, name);
		switch (event_type) {
		case G_FILE_MONITOR_EVENT_DELETED:
			PLUGIN_PRINT (IFCFG_PLUGIN_NAME, "removed %s.", name);
			if (connection)
				remove_connection (plugin, connection);
			break;
		case G_FILE_MONITOR_EVENT_CREATED:
		case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
			/* Update or new */
			connection_new_or_changed (plugin, name, connection);
			break;
		default:
			break;
		}
		g_free (name);
	}
}

/* monitoring the /etc/sysconfig/network/ directory */
static void
setup_ifcfg_monitoring (SCPluginIfcfg *plugin)
{
	SCPluginIfcfgPrivate *priv = SC_PLUGIN_IFCFG_GET_PRIVATE (plugin);
	GFile *file;
	GFileMonitor *monitor;

	priv->connections = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);

	file = g_file_new_for_path (IFCFG_DIR "/");
	monitor = g_file_monitor_directory (file, G_FILE_MONITOR_NONE, NULL, NULL);
	g_object_unref (file);

	if (monitor) {
		priv->monitor_id = g_signal_connect (monitor, "changed", G_CALLBACK (dir_changed), plugin);
		priv->ifcfg_monitor = monitor;
	}
}

static GSList *
get_connections (NMSystemConfigInterface *config)
{
	SCPluginIfcfg *plugin = SC_PLUGIN_IFCFG (config);
	SCPluginIfcfgPrivate *priv = SC_PLUGIN_IFCFG_GET_PRIVATE (plugin);
	GSList *list = NULL;
	GHashTableIter iter;
	gpointer value;

	g_debug ("get_connections...");
	if (!priv->connections) {
		setup_ifcfg_monitoring (plugin);
		read_connections (plugin);
	}

	g_hash_table_iter_init (&iter, priv->connections);
	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		NMIfcfgSUSEConnection *exported = NM_IFCFG_SUSE_CONNECTION (value);

		if (!nm_ifcfg_suse_connection_get_unmanaged_spec (exported))
			list = g_slist_prepend (list, value);
	}

	return list;
}

static void
file_changed (GFileMonitor *monitor,
		    GFile *file,
		    GFile *other_file,
		    GFileMonitorEvent event_type,
		    gpointer user_data)
{
	FileMonitorInfo *info;
	g_debug ("file_changed(%d)...", event_type);

	switch (event_type) {
	case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
	case G_FILE_MONITOR_EVENT_DELETED:
		info = (FileMonitorInfo *) user_data;
		info->callback (info->user_data);
		break;
	default:
		break;
	}
}

static GFileMonitor *
monitor_file_changes (const char *filename,
				  FileChangedFn callback,
				  gpointer user_data)
{
	GFile *file;
	GFileMonitor *monitor;
	FileMonitorInfo *info;

	g_debug ("monitor_file_changes...");
	file = g_file_new_for_path (filename);
	monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, NULL);
	g_object_unref (file);

	if (monitor) {
		info = g_new0 (FileMonitorInfo, 1);
		info->callback = callback;
		info->user_data = user_data;
		g_object_weak_ref (G_OBJECT (monitor), (GWeakNotify) g_free, info);
		g_signal_connect (monitor, "changed", G_CALLBACK (file_changed), info);
	}

	return monitor;
}

static gboolean
hostname_is_dynamic (void)
{
	GIOChannel *channel;
	const char *pattern = "DHCLIENT_SET_HOSTNAME=";
	char *str = NULL;
	int pattern_len;
	gboolean dynamic = FALSE;

	channel = g_io_channel_new_file (CONF_DHCP, "r", NULL);
	if (!channel)
		return dynamic;

	pattern_len = strlen (pattern);

	while (g_io_channel_read_line (channel, &str, NULL, NULL, NULL) != G_IO_STATUS_EOF) {
		if (!strncmp (str, pattern, pattern_len)) {
			if (!strncmp (str + pattern_len, "\"yes\"", 5))
				dynamic = TRUE;
			break;
		}
		g_free (str);
	}

	g_io_channel_shutdown (channel, FALSE, NULL);
	g_io_channel_unref (channel);

	return dynamic;
}

static char *
hostname_read ()
{
	GIOChannel *channel;
	char *hostname = NULL;

	channel = g_io_channel_new_file (HOSTNAME_FILE, "r", NULL);
	if (channel) {
		g_io_channel_read_line (channel, &hostname, NULL, NULL, NULL);
		g_io_channel_shutdown (channel, FALSE, NULL);
		g_io_channel_unref (channel);

		if (hostname)
			hostname = g_strchomp (hostname);
	}

	return hostname;
}

static void
hostname_changed (gpointer data)
{
	SCPluginIfcfgPrivate *priv = SC_PLUGIN_IFCFG_GET_PRIVATE (data);

	g_debug ("hostname_changed...");

	g_free (priv->hostname);
	if (hostname_is_dynamic ())
		priv->hostname = NULL;
	else
		priv->hostname = hostname_read ();

	g_debug ("hostname is %s", priv->hostname);

	g_object_notify (G_OBJECT (data), NM_SYSTEM_CONFIG_INTERFACE_HOSTNAME);
}

static void
plugin_set_hostname (SCPluginIfcfg *plugin, const char *hostname)
{
	SCPluginIfcfgPrivate *priv = SC_PLUGIN_IFCFG_GET_PRIVATE (plugin);
	GIOChannel *channel;

	g_debug ("plugin_set_hostname(%s)...", hostname);

	channel = g_io_channel_new_file (HOSTNAME_FILE, "w", NULL);
	if (channel) {
		g_io_channel_write_chars (channel, hostname, -1, NULL, NULL);
		g_io_channel_write_chars (channel, "\n", -1, NULL, NULL);
		g_io_channel_shutdown (channel, TRUE, NULL);
		g_io_channel_unref (channel);
	}

	g_free (priv->hostname);
	priv->hostname = g_strdup (hostname);
}

static void
init (NMSystemConfigInterface *config)
{
	SCPluginIfcfgPrivate *priv = SC_PLUGIN_IFCFG_GET_PRIVATE (config);
	g_debug ("init...");

	priv->hostname_monitor = monitor_file_changes (HOSTNAME_FILE, hostname_changed, config);
	priv->dhcp_monitor = monitor_file_changes (CONF_DHCP, hostname_changed, config);

	if (!hostname_is_dynamic ())
		priv->hostname = hostname_read ();
}

static void
sc_plugin_ifcfg_init (SCPluginIfcfg *self)
{
	g_debug ("sc_plugin_ifcfg_init...");
}

static void
dispose (GObject *object)
{
	SCPluginIfcfgPrivate *priv = SC_PLUGIN_IFCFG_GET_PRIVATE (object);

	if (priv->dhcp_monitor)
		g_object_unref (priv->dhcp_monitor);

	if (priv->hostname_monitor)
		g_object_unref (priv->hostname_monitor);

	g_free (priv->hostname);

	G_OBJECT_CLASS (sc_plugin_ifcfg_parent_class)->dispose (object);
}

static void
get_property (GObject *object, guint prop_id,
		    GValue *value, GParamSpec *pspec)
{
	switch (prop_id) {
	case NM_SYSTEM_CONFIG_INTERFACE_PROP_NAME:
		g_value_set_string (value, IFCFG_PLUGIN_NAME);
		break;
	case NM_SYSTEM_CONFIG_INTERFACE_PROP_INFO:
		g_value_set_string (value, IFCFG_PLUGIN_INFO);
		break;
	case NM_SYSTEM_CONFIG_INTERFACE_PROP_CAPABILITIES:
		g_value_set_uint (value, NM_SYSTEM_CONFIG_INTERFACE_CAP_MODIFY_HOSTNAME);
		break;
	case NM_SYSTEM_CONFIG_INTERFACE_PROP_HOSTNAME:
		g_value_set_string (value, SC_PLUGIN_IFCFG_GET_PRIVATE (object)->hostname);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
set_property (GObject *object, guint prop_id,
		    const GValue *value, GParamSpec *pspec)
{
	const char *hostname;

	switch (prop_id) {
	case NM_SYSTEM_CONFIG_INTERFACE_PROP_HOSTNAME:
		hostname = g_value_get_string (value);
		if (hostname && strlen (hostname) < 1)
			hostname = NULL;
		plugin_set_hostname (SC_PLUGIN_IFCFG (object), hostname);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
sc_plugin_ifcfg_class_init (SCPluginIfcfgClass *req_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (req_class);

	g_debug ("sc_plugin_ifcfg_class_init...");

	g_type_class_add_private (req_class, sizeof (SCPluginIfcfgPrivate));

	object_class->get_property = get_property;
	object_class->set_property = set_property;
	object_class->dispose = dispose;

	g_object_class_override_property (object_class,
							    NM_SYSTEM_CONFIG_INTERFACE_PROP_NAME,
							    NM_SYSTEM_CONFIG_INTERFACE_NAME);

	g_object_class_override_property (object_class,
							    NM_SYSTEM_CONFIG_INTERFACE_PROP_INFO,
							    NM_SYSTEM_CONFIG_INTERFACE_INFO);

	g_object_class_override_property (object_class,
							    NM_SYSTEM_CONFIG_INTERFACE_PROP_CAPABILITIES,
							    NM_SYSTEM_CONFIG_INTERFACE_CAPABILITIES);

	g_object_class_override_property (object_class,
							    NM_SYSTEM_CONFIG_INTERFACE_PROP_HOSTNAME,
							    NM_SYSTEM_CONFIG_INTERFACE_HOSTNAME);
}

static void
system_config_interface_init (NMSystemConfigInterface *system_config_interface_class)
{
	g_debug ("system_config_interface_init...");
	/* interface implementation */
	system_config_interface_class->get_connections = get_connections;
	system_config_interface_class->init = init;
}

G_MODULE_EXPORT GObject *
nm_system_config_factory (void)
{
	static SCPluginIfcfg *singleton = NULL;

	g_debug ("nm_system_config_factory...");

	if (!singleton)
		singleton = SC_PLUGIN_IFCFG (g_object_new (SC_TYPE_PLUGIN_IFCFG, NULL));
	else
		g_object_ref (singleton);

	return G_OBJECT (singleton);
}
