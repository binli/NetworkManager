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
 */

#include <stdlib.h>
#include <string.h>
#include "utils.h"

static gboolean
check_suffix (const char *base, const char *tag)
{
	int len, tag_len;

	g_return_val_if_fail (base != NULL, TRUE);
	g_return_val_if_fail (tag != NULL, TRUE);

	len = strlen (base);
	tag_len = strlen (tag);
	if ((len > tag_len) && !strcasecmp (base + len - tag_len, tag))
		return TRUE;
	return FALSE;
}

gboolean
utils_should_ignore_file (const char *filename, gboolean only_ifcfg)
{
	char *base;
	gboolean ignore = TRUE;
	gboolean is_ifcfg = FALSE;
	gboolean is_other = FALSE;

	g_return_val_if_fail (filename != NULL, TRUE);

	base = g_path_get_basename (filename);
	g_return_val_if_fail (base != NULL, TRUE);

	/* Only handle ifcfg, ifroute and routes files now */
	if (!strncmp (base, IFCFG_TAG, strlen (IFCFG_TAG)))
		is_ifcfg = TRUE;

	if (only_ifcfg == FALSE) {
		if (   !strncmp (base, IFROUTE_TAG, strlen (IFROUTE_TAG))
		    || !strncmp (base, ROUTES_TAG, strlen (ROUTES_TAG)))
				is_other = TRUE;
	}

	/* But not those that have certain suffixes */
	if (   (is_ifcfg || is_other)
	    && !check_suffix (base, BAK_TAG)
	    && !check_suffix (base, TILDE_TAG)
	    && !check_suffix (base, ORIG_TAG)
	    && !check_suffix (base, REJ_TAG))
		ignore = FALSE;

	g_free (base);
	return ignore;
}

/* get the ifcfg's name from the full path, such as eth0, wlan0 or routes*/
const char *
utils_get_ifcfg_name (const char *file, gboolean only_ifcfg)
{
	const char *name = NULL, *start = NULL;
	char *base;

	g_return_val_if_fail (file != NULL, NULL);

	base = g_path_get_basename (file);
	if (!base)
		return NULL;

	/* Find the point in 'file' where 'base' starts.  We use 'file' since it's
	 * const and thus will survive after we free 'base'.
	 */
	start = file + strlen (file) - strlen (base);
	g_assert (strcmp (start, base) == 0);
	g_free (base);

	if (!strncmp (start, IFCFG_TAG, strlen (IFCFG_TAG)))
		name = start + strlen (IFCFG_TAG);
	else if (only_ifcfg == FALSE)  {
		if (!strncmp (start, IFROUTE_TAG, strlen (IFROUTE_TAG)))
			name = start + strlen (IFROUTE_TAG);
		else if (!strncmp (start, ROUTES_TAG, strlen (ROUTES_TAG)))
			name = start + strlen (ROUTES_TAG);
	}

	return name;
}

/* Used to get any ifcfg/extra file path from any other ifcfg/extra path
 * in the form <tag><name>.
 */
static char *
utils_get_extra_path (const char *parent, const char *tag)
{
	char *item_path = NULL, *dirname;
	const char *name;

	g_return_val_if_fail (parent != NULL, NULL);
	g_return_val_if_fail (tag != NULL, NULL);

	dirname = g_path_get_dirname (parent);
	if (!dirname)
		return NULL;

	name = utils_get_ifcfg_name (parent, FALSE);
	if (name) {
		if (!strcmp (dirname, "."))
			item_path = g_strdup_printf ("%s%s", tag, name);
		else
			item_path = g_strdup_printf ("%s/%s%s", dirname, tag, name);
	}
	g_free (dirname);

	return item_path;
}

char *
utils_get_ifcfg_path (const char *parent)
{
	return utils_get_extra_path (parent, IFCFG_TAG);
}

