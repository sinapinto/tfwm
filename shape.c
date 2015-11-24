/* See LICENSE file for copyright and license details. */
#include <xcb/xcb_image.h>
#include <xcb/shape.h>
#include "types.h"
#include "shape.h"
#include "tfwm.h"
#include "corner"

void
check_shape_extension() {
	xcb_query_extension_reply_t const* ereply = xcb_get_extension_data(conn, &xcb_shape_id);
	if (!ereply)
		err("can't get shape extension data.");
	if (!ereply->present)
		warn("SHAPE extension isn't available");
	xcb_shape_query_version_cookie_t vcookie = xcb_shape_query_version_unchecked(conn);
	xcb_shape_query_version_reply_t* vreply = xcb_shape_query_version_reply(conn, vcookie, 0);
	if (!vreply)
		err("can't get shape extension version.");
	free(vreply);
}


void
roundcorners(Client *c) {
	xcb_pixmap_t pmap = xcb_create_pixmap_from_bitmap_data(conn,
			screen->root,
			corner_bits,
			corner_width, corner_height,
			1, // depth
			0, // fg
			1, // bg
			NULL); // gc
	if (pmap == XCB_NONE)
		err("xcb_create_pixmap_from_bitmap_data() failed.");

	xcb_shape_mask(conn,
			XCB_SHAPE_SO_SUBTRACT,
			XCB_SHAPE_SK_BOUNDING,
			c->win,
			0, 0,
			pmap);
	xcb_free_pixmap(conn, pmap);
}

