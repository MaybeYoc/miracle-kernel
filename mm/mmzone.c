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
	return NODE_DATA(first_online_node);
}

struct pglist_data *next_online_pgdat(struct pglist_data *pgdat)
{
	int nid = next_online_node(pgdat->node_id);

	if (nid == MAX_NUMNODES)
		return NULL;
	return NODE_DATA(nid);
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

struct zone *first_populated_zoneidx(enum zone_type idx)
{
	struct zone *zone;

	for_each_zone(zone)
		if (populated_zone(zone) && zone_idx(zone) == idx)
			return zone;

	return NULL;
}

struct zone *next_populated_zoneidx(struct zone *zone)
{
	enum zone_type idx = zone_idx(zone);
	struct pglist_data *pgdat = zone->zone_pgdat;

	for (pgdat = next_online_pgdat(pgdat); pgdat; pgdat = next_online_pgdat(pgdat))
		if (populated_zone(&pgdat->node_zones[idx]))
			return &pgdat->node_zones[idx];
	
	return NULL;
}
