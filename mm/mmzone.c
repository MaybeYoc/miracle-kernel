// SPDX-License-Identifier: GPL-2.0
/*
 * linux/mm/mmzone.c
 *
 * management codes for pgdats, zones and page flags
 */


#include <linux/stddef.h>
#include <linux/mm.h>
#include <linux/mmzone.h>

struct pglist_data *first_online_pgdat(void)
{
	int nid;

	for (nid = 0; nid < MAX_NUMNODES; nid++)
		if (get_node_online(nid))
			return NODE_DATA(nid);

	return NULL;
}

struct pglist_data *next_online_pgdat(struct pglist_data *pgdat)
{
	int i, nid;

	if (!pgdat)
		return NULL;

	nid = pgdat->node_id + 1;

	if (nid == MAX_NUMNODES)
		return NULL;
	
	for (i = nid; i < MAX_NUMNODES; i++) {
		if (get_node_online(i))
			return NODE_DATA(i);
	}

	return NULL;
}

struct zone *next_zone(struct zone *zone)
{
	struct pglist_data *pgdat = zone->zone_pgdat;

	if (zone < pgdat->node_zones + MAX_NR_ZONES - 1)
		zone++;
	else {
		pgdat = next_online_pgdat(pgdat);
		if (pgdat)
			zone = pgdat->node_zones;
		else
			zone = NULL;
	}

	return zone;
}
