/* See LICENSE file for copyright and license details. */
#include <xcb/shape.h>
#include <xcb/xcb_image.h>
#include <xcb/xcb_pixel.h>
#include "tfwm.h"
#include "shape.h"
#include "util.h"

enum {
	TOP_LEFT,
	TOP_RIGHT,
	BOTTOM_LEFT,
	BOTTOM_RIGHT
};

static void draw_corner(Client *c, uint8_t corner, unsigned char bits[]);

bool
check_shape_extension(void) {
	xcb_query_extension_reply_t const *ereply;
	xcb_shape_query_version_cookie_t   vcookie;
	xcb_shape_query_version_reply_t   *vreply;

	ereply = xcb_get_extension_data(conn, &xcb_shape_id);

	if (!ereply) {
		warn("can't get shape extension data.");
		return false;
	}

	if (!ereply->present)
		warn("SHAPE extension isn't available");

	vcookie = xcb_shape_query_version_unchecked(conn);
	vreply = xcb_shape_query_version_reply(conn, vcookie, 0);

	if (!vreply) {
		warn("can't get shape extension version.");
		return false;
	}

	PRINTF("Info: using shape extension version %d.%d\n",
	       vreply->major_version, vreply->minor_version);
	FREE(vreply);
	return true;
}

void
draw_corner(Client *c, uint8_t corner, unsigned char bits[]) {
	xcb_pixmap_t pmap = XCB_NONE;

	PRINTF("len bits: %d\n", LENGTH(bits));
	pmap = xcb_create_pixmap_from_bitmap_data(conn, screen->root, bits,
						  8, 8, 1, 0, 1, NULL);
	if (pmap == XCB_NONE)
		err("xcb_create_pixmap_from_bitmap_data() failed.");

	switch (corner) {
	case TOP_LEFT:
		xcb_shape_mask(conn, XCB_SHAPE_SO_SUBTRACT,
			       XCB_SHAPE_SK_BOUNDING, c->frame, 0, 0, pmap);
		break;
	case TOP_RIGHT:
		xcb_shape_mask(conn, XCB_SHAPE_SO_SUBTRACT,
			       XCB_SHAPE_SK_BOUNDING, c->frame,
			       c->geom.width - 8, 0, pmap);
		break;
	case BOTTOM_LEFT:
		xcb_shape_mask(conn, XCB_SHAPE_SO_SUBTRACT,
			       XCB_SHAPE_SK_BOUNDING, c->frame,
			       0, c->geom.height - 8, pmap);
		break;
	case BOTTOM_RIGHT:
		xcb_shape_mask(conn, XCB_SHAPE_SO_SUBTRACT,
			       XCB_SHAPE_SK_BOUNDING, c->frame,
			       c->geom.width - 8, c->geom.height - 8, pmap);
		break;
	}

	xcb_free_pixmap(conn, pmap);
}

void
roundcorners(Client *c) {
	static unsigned char top_left_bits[] = {
		0xc0, 0xf8, 0xfc, 0xfe, 0xfe, 0xfe, 0xff, 0xff};

	static unsigned char top_right_bits[] = {
		0x03, 0x1f, 0x3f, 0x7f, 0x7f, 0x7f, 0xff, 0xff};

	static unsigned char bottom_left_bits[] = {
		0xff, 0xff, 0xfe, 0xfe, 0xfe, 0xfc, 0xf8, 0xc0};

	static unsigned char bottom_right_bits[] = {
		0xff, 0xff, 0x7f, 0x7f, 0x7f, 0x3f, 0x1f, 0x03};

	draw_corner(c, TOP_LEFT, top_left_bits);
	draw_corner(c, TOP_RIGHT, top_right_bits);
	draw_corner(c, BOTTOM_LEFT, bottom_left_bits);
	draw_corner(c, BOTTOM_RIGHT, bottom_right_bits);
}

