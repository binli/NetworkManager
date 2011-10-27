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

#include <nm-setting-connection.h>
#include "nm-ifcfg-suse-connection.h"
#include "reader.h"

G_DEFINE_TYPE (NMIfcfgSUSEConnection, nm_ifcfg_suse_connection, NM_TYPE_SETTINGS_CONNECTION)

#define NM_IFCFG_SUSE_CONNECTION_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NM_TYPE_IFCFG_SUSE_CONNECTION, NMIfcfgSUSEConnectionPrivate))

typedef struct {
	gulong ih_event_id;

	char *path; /*  */
	int file_wd;

	char *keyfile;
	int keyfile_wd;

	char *routefile;
	int routefile_wd;

	char *route6file;
	int route6file_wd;

	char *unmanaged; /* when it's vlan, bridge or bond, it should be unmanaged */
} NMIfcfgSUSEConnectionPrivate;

enum {
	PROP_0,
	PROP_UNMANAGED,
	LAST_PROP
};

NMIfcfgSUSEConnection *
nm_ifcfg_suse_connection_new (const char *full_path,
                              NMConnection *source,
                              GError **error,
                              gboolean *ignore_error)
{
	GObject *object;
	//NMIfcfgSUSEConnectionPrivate *priv;
	NMConnection *tmp;
	char *unmanaged = NULL;
	char *ifroutefile = NULL;
	char *routesfile = NULL;

	g_return_val_if_fail (full_path != NULL, NULL);
	g_debug ("nm_ifcfg_suse_connection_new...");

	/* If we're given a connection already, prefer that instead of re-reading */
	if (source)
		tmp = g_object_ref (source);
	else {
		tmp = connection_from_file (full_path, NULL, NULL,
		                            &unmanaged,
		                            &ifroutefile,
		                            &routesfile,
		                            error,
		                            ignore_error);
		if (!tmp)
			return NULL;
	}

	object = (GObject *) g_object_new (NM_TYPE_IFCFG_SUSE_CONNECTION,
	                                   NM_IFCFG_SUSE_CONNECTION_UNMANAGED, unmanaged,
	                                   NULL);
	if (!object)
		goto out;

out:
	g_object_unref (tmp);
	return (NMIfcfgSUSEConnection *) object;
}

const char *
nm_ifcfg_suse_connection_get_path (NMIfcfgSUSEConnection *self)
{
	g_return_val_if_fail (NM_IS_IFCFG_SUSE_CONNECTION (self), NULL);

	return NM_IFCFG_SUSE_CONNECTION_GET_PRIVATE (self)->path;
}

const char *
nm_ifcfg_suse_connection_get_unmanaged_spec (NMIfcfgSUSEConnection *self)
{
	g_return_val_if_fail (NM_IS_IFCFG_SUSE_CONNECTION (self), NULL);

	return NM_IFCFG_SUSE_CONNECTION_GET_PRIVATE (self)->unmanaged;
}


static void
commit_changes (NMSettingsConnection *connection,
                NMSettingsConnectionCommitFunc callback,
                gpointer user_data)
{
}

static void
do_delete (NMSettingsConnection *connection,
	       NMSettingsConnectionDeleteFunc callback,
	       gpointer user_data)
{
	//NMIfcfgSUSEConnectionPrivate *priv = NM_IFCFG_SUSE_CONNECTION_GET_PRIVATE (connection);

	NM_SETTINGS_CONNECTION_CLASS (nm_ifcfg_suse_connection_parent_class)->delete (connection, callback, user_data);
}

static void
nm_ifcfg_suse_connection_init (NMIfcfgSUSEConnection *connection)
{
	g_debug ("nm_ifcfg_suse_connection_init...");
}

static void
finalize (GObject *object)
{
	g_debug ("finalize...");
	/*NMIfcfgSUSEConnectionPrivate *priv = NM_IFCFG_SUSE_CONNECTION_GET_PRIVATE (object);
	NMInotifyHelper *ih;

	nm_connection_clear_secrets (NM_CONNECTION (object));

	ih = nm_inotify_helper_get ();

	if (priv->ih_event_id)
		g_signal_handler_disconnect (ih, priv->ih_event_id);

	g_free (priv->path);
	if (priv->file_wd >= 0)
		nm_inotify_helper_remove_watch (ih, priv->file_wd);

	g_free (priv->keyfile);
	if (priv->keyfile_wd >= 0)
		nm_inotify_helper_remove_watch (ih, priv->keyfile_wd);

	g_free (priv->routefile);
	if (priv->routefile_wd >= 0)
		nm_inotify_helper_remove_watch (ih, priv->routefile_wd);

	g_free (priv->route6file);
	if (priv->route6file_wd >= 0)
		nm_inotify_helper_remove_watch (ih, priv->route6file_wd);
*/
	G_OBJECT_CLASS (nm_ifcfg_suse_connection_parent_class)->finalize (object);
}

static void
set_property (GObject *object, guint prop_id,
		    const GValue *value, GParamSpec *pspec)
{
	NMIfcfgSUSEConnectionPrivate *priv = NM_IFCFG_SUSE_CONNECTION_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_UNMANAGED:
		priv->unmanaged = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
get_property (GObject *object, guint prop_id,
		    GValue *value, GParamSpec *pspec)
{
	NMIfcfgSUSEConnectionPrivate *priv = NM_IFCFG_SUSE_CONNECTION_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_UNMANAGED:
		g_value_set_string (value, priv->unmanaged);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}


static void
nm_ifcfg_suse_connection_class_init (NMIfcfgSUSEConnectionClass *ifcfg_suse_connection_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (ifcfg_suse_connection_class);
	NMSettingsConnectionClass *settings_class = NM_SETTINGS_CONNECTION_CLASS (ifcfg_suse_connection_class);

	g_debug ("nm_ifcfg_suse_connection_class_init...");

	g_type_class_add_private (ifcfg_suse_connection_class, sizeof (NMIfcfgSUSEConnectionPrivate));

	/* Virtual methods */
	object_class->set_property = set_property;
	object_class->get_property = get_property;
	object_class->finalize     = finalize;
	settings_class->delete = do_delete;
	settings_class->commit_changes = commit_changes;

	/* Properties */
	g_object_class_install_property
		(object_class, PROP_UNMANAGED,
		 g_param_spec_string (NM_IFCFG_SUSE_CONNECTION_UNMANAGED,
						  "Unmanaged",
						  "Unmanaged",
						  NULL,
						  G_PARAM_READWRITE));

/* 	signals[IFCFG_CHANGED] =
		g_signal_new ("ifcfg-changed",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              0, NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);*/
}

