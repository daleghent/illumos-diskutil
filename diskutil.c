/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * Copyright 2017 Joyent, Inc.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <time.h>
#include <err.h>
#include <fm/libtopo.h>
#include <scsi/plugins/ses/framework/ses2.h>

#include <topo_error.h>

#define	FMT	"%25s %-25s %-8s %-8s\n"
#define	FMT1A	"%25s %-25s "
#define	FMT1B	"%-8s"
#define	HDGS	"DISK", "BAY", "SERVICE?", "LOCATE?"

typedef enum diskutil_action {
	DISKUTIL_LIST = 1,
	DISKUTIL_LED_CONTROL
} diskutil_action_t;

typedef enum diskutil_led_mode {
	DIKSUTIL_LED_DONTSET = 0,
	DISKUTIL_LED_ON = 1,
	DISKUTIL_LED_OFF
} diskutil_led_mode_t;

typedef struct diskutil_args {
	diskutil_action_t dua_action;
	uint32_t dua_led_type;
	char *dua_disk_name;
	diskutil_led_mode_t dua_led_mode;

	boolean_t dua_led_found;
} diskutil_args_t;

static void
print_led_column(int led_status)
{
	switch (led_status) {
	case -1:
		printf(FMT1B, "-");
		break;
	case 1:
		printf("\x1b[7m" FMT1B "\x1b[0m ", "ON ");
		break;
	case 0:
		printf("\x1b[0m" FMT1B "\x1b[0m ", "OFF");
		break;
	default:
		abort();
	}
}

static int
finder(topo_hdl_t *h, tnode_t *n, void *p)
{
	int err = 0, i = 0;
	char *logical_disk = NULL;
	char *label = NULL;
	tnode_t *pn = n->tn_parent;
	int is_bay = 0;
	/*char *fail = "-", *locate = "-";*/
	diskutil_args_t *dua = p;
	int fail = -1, locate = -1;

	if (strcmp(topo_node_name(n), "disk") != 0)
		return (TOPO_WALK_NEXT);

	topo_prop_get_string(n, "storage", "logical-disk", &logical_disk,
	    &err);

	/*
	 * If we're doing LED control, and we're not interested in this disk,
	 * skip it.  Otherwise, plough on!
	 */
	if (dua->dua_action == DISKUTIL_LED_CONTROL &&
	    strcmp(dua->dua_disk_name, logical_disk) != 0)
		goto out;

	is_bay = (strcmp(topo_node_name(pn), "bay") == 0);
	if (is_bay) {
		tnode_t *c = NULL;

		/* parent is a bay, look for "label" */
		topo_prop_get_string(n->tn_parent, "protocol", "label",
		    &label, &err);

		/* list children of parent, to see if there are indicators */
		while ((c = topo_child_next(pn, c)) != NULL) {
			int err;
			topo_led_state_t mode;
			topo_led_type_t type;

			if (topo_node_flags(c) != TOPO_NODE_FACILITY)
				continue;

			if (topo_prop_get_uint32(c, "facility", "mode", &mode,
			    &err) != 0 ||
			    topo_prop_get_uint32(c, "facility", "type", &type,
			    &err) != 0)
				continue;

			/*
			 * If we're doing LED control and this facility node
			 * has a LED of the correct type, then adjust it
			 * accordingly.
			 */
			if (dua->dua_action == DISKUTIL_LED_CONTROL &&
			    dua->dua_led_type == type) {
				uint32_t val;
				switch (dua->dua_led_mode) {
				case DISKUTIL_LED_ON:
					topo_prop_set_uint32(c, "facility",
					    "mode", TOPO_PROP_MUTABLE, 1, &err);
					if (err != 0)
						printf("could not set "
						    "facility.mode -- err %s\n",
						    topo_strerror(err));
					break;
				case DISKUTIL_LED_OFF:
					topo_prop_set_uint32(c, "facility",
					    "mode", TOPO_PROP_MUTABLE, 0, &err);
					if (err != 0)
						printf("could not set "
						    "facility.mode -- err %s\n",
						    topo_strerror(err));
					break;
				}
				dua->dua_led_found = B_TRUE;
				topo_prop_get_uint32(c, "facility", "mode",
				    &mode, &err);
			}

			switch (type) {
				case TOPO_LED_TYPE_SERVICE:
					fail = mode ? 1 : 0;
					break;
				case TOPO_LED_TYPE_LOCATE:
					locate = mode ? 1 : 0;
					break;
			}
		}
	}

	printf(FMT1A, logical_disk, label == NULL ? "-" : label);
	print_led_column(fail);
	print_led_column(locate);
	printf("\n");

out:
	if (logical_disk != NULL)
		topo_hdl_strfree(h, logical_disk);
	if (label != NULL)
		topo_hdl_strfree(h, label);
	return (TOPO_WALK_NEXT);
}

int
main(int argc, char **argv)
{
	int rc = 0;
	int e = 0;
	topo_hdl_t *h;
	topo_walk_t *w;
	diskutil_args_t dua;

	bzero(&dua, sizeof (dua));

	if (argc == 1) {
		printf("usage:\n");
		printf("\t%s\tlist\n", argv[0]);
		printf("\t%s\tDISK_DEVICE <locate|service> <on|off>\n", argv[0]);
		printf("\n");
		return (1);
	}

	if (argc == 2 && strcmp(argv[1], "list") == 0) {
		dua.dua_action = DISKUTIL_LIST;
		printf("list mode:\n");
	} else if (argc == 4) {
		dua.dua_action = DISKUTIL_LED_CONTROL;
		printf("set LED mode:\n");
		if (strcmp(argv[3], "on") == 0)
			dua.dua_led_mode = DISKUTIL_LED_ON;
		else if (strcmp(argv[3], "off") == 0)
			dua.dua_led_mode = DISKUTIL_LED_OFF;
		else {
			printf("unknown LED mode '%s'\n", argv[3]);
			exit(1);
		}
		if (strcmp(argv[2], "locate") == 0) {
			dua.dua_led_type = TOPO_LED_TYPE_LOCATE;
		} else if (strcmp(argv[2], "service") == 0) {
			dua.dua_led_type = TOPO_LED_TYPE_SERVICE;
		} else {
			printf("unknown LED name '%s'\n", argv[2]);
		}
		dua.dua_disk_name = strdup(argv[1]);
	} else {
		printf("unknown args.\n");
		exit(1);
	}

	h = topo_open(TOPO_VERSION, NULL, &e);
	if (e != 0)
		errx(1, "could not topo_open(), %d", e);

	(void) topo_snap_hold(h, NULL, &e);
	if (e != 0)
		errx(1, "could not topo_snap_hold(), %d", e);

	w = topo_walk_init(h, "hc", finder, &dua, &e);
	if (e == ETOPO_WALK_NOTFOUND)
		errx(1, "ETOPO_WALK_NOTFOUND");
	if (e == ETOPO_WALK_EMPTY)
		errx(1, "ETOPO_WALK_EMPTY");
	if (e != 0)
		errx(1, "could not topo_walk_init(), %d", e);

	printf(FMT, HDGS);
	while ((e = topo_walk_step(w, TOPO_WALK_CHILD)) == TOPO_WALK_NEXT);

	if (dua.dua_action == DISKUTIL_LED_CONTROL &&
	    dua.dua_led_found != B_TRUE) {
		printf("could not find led %d on disk %s\n", dua.dua_led_type,
		    dua.dua_disk_name);
		rc = 1;
	}

	topo_walk_fini(w);
	topo_snap_release(h);
	topo_close(h);

	exit(rc);
}
