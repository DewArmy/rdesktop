/* -*- c-basic-offset: 8 -*-
   rdesktop: A Remote Desktop Protocol client.

   Support functions for Extended Window Manager Hints,
   http://www.freedesktop.org/wiki/Standards_2fwm_2dspec

   Copyright (C) Peter Astrand <astrand@cendio.se> 2005
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <X11/Xlib.h>
#include "rdesktop.h"

#define _NET_WM_STATE_REMOVE        0	/* remove/unset property */
#define _NET_WM_STATE_ADD           1	/* add/set property */
#define _NET_WM_STATE_TOGGLE        2	/* toggle property  */

extern Display *g_display;
static Atom g_net_wm_state_maximized_vert_atom, g_net_wm_state_maximized_horz_atom,
	g_net_wm_state_hidden_atom;
Atom g_net_wm_state_atom;

/* 
   Get window property value (32 bit format) 
   Returns zero on success, -1 on error
*/
static int
get_property_value(Window wnd, char *propname, long max_length,
		   unsigned long *nitems_return, unsigned char **prop_return)
{
	int result;
	Atom property;
	Atom actual_type_return;
	int actual_format_return;
	unsigned long bytes_after_return;

	property = XInternAtom(g_display, propname, True);
	if (property == None)
	{
		fprintf(stderr, "Atom %s does not exist\n", propname);
		return (-1);
	}

	result = XGetWindowProperty(g_display, wnd, property, 0,	/* long_offset */
				    max_length,	/* long_length */
				    False,	/* delete */
				    AnyPropertyType,	/* req_type */
				    &actual_type_return,
				    &actual_format_return,
				    nitems_return, &bytes_after_return, prop_return);

	if (result != Success)
	{
		fprintf(stderr, "XGetWindowProperty failed\n");
		return (-1);
	}

	if (actual_type_return == None || actual_format_return == 0)
	{
		fprintf(stderr, "Window is missing property %s\n", propname);
		return (-1);
	}

	if (bytes_after_return)
	{
		fprintf(stderr, "%s is too big for me\n", propname);
		return (-1);
	}

	if (actual_format_return != 32)
	{
		fprintf(stderr, "%s has bad format\n", propname);
		return (-1);
	}

	return (0);
}

/* 
   Get current desktop number
   Returns -1 on error
*/
static int
get_current_desktop(void)
{
	unsigned long nitems_return;
	unsigned char *prop_return;
	int current_desktop;

	if (get_property_value
	    (DefaultRootWindow(g_display), "_NET_CURRENT_DESKTOP", 1, &nitems_return,
	     &prop_return) < 0)
		return (-1);

	if (nitems_return != 1)
	{
		fprintf(stderr, "_NET_CURRENT_DESKTOP has bad length\n");
		return (-1);
	}

	current_desktop = *prop_return;
	XFree(prop_return);
	return current_desktop;
}

/*
  Get workarea geometry
  Returns zero on success, -1 on error
 */

int
get_current_workarea(uint32 * x, uint32 * y, uint32 * width, uint32 * height)
{
	int current_desktop;
	unsigned long nitems_return;
	unsigned char *prop_return;
	uint32 *return_words;
	const uint32 net_workarea_x_offset = 0;
	const uint32 net_workarea_y_offset = 1;
	const uint32 net_workarea_width_offset = 2;
	const uint32 net_workarea_height_offset = 3;
	const uint32 max_prop_length = 32 * 4;	/* Max 32 desktops */

	if (get_property_value
	    (DefaultRootWindow(g_display), "_NET_WORKAREA", max_prop_length, &nitems_return,
	     &prop_return) < 0)
		return (-1);

	if (nitems_return % 4)
	{
		fprintf(stderr, "_NET_WORKAREA has odd length\n");
		return (-1);
	}

	current_desktop = get_current_desktop();

	if (current_desktop < 0)
		return -1;

	return_words = (uint32 *) prop_return;

	*x = return_words[current_desktop * 4 + net_workarea_x_offset];
	*y = return_words[current_desktop * 4 + net_workarea_y_offset];
	*width = return_words[current_desktop * 4 + net_workarea_width_offset];
	*height = return_words[current_desktop * 4 + net_workarea_height_offset];

	XFree(prop_return);

	return (0);

}



void
ewmh_init()
{
	g_net_wm_state_maximized_vert_atom =
		XInternAtom(g_display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
	g_net_wm_state_maximized_horz_atom =
		XInternAtom(g_display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
	g_net_wm_state_hidden_atom = XInternAtom(g_display, "_NET_WM_STATE_HIDDEN", False);
	g_net_wm_state_atom = XInternAtom(g_display, "_NET_WM_STATE", False);
}


/* 
   Get the window state: normal/minimized/maximized. 
*/
#ifndef MAKE_PROTO
int
ewmh_get_window_state(Window w)
{
	unsigned long nitems_return;
	unsigned char *prop_return;
	uint32 *return_words;
	unsigned long item;
	BOOL maximized_vert, maximized_horz, hidden;

	maximized_vert = maximized_horz = hidden = False;

	if (get_property_value(w, "_NET_WM_STATE", 64, &nitems_return, &prop_return) < 0)
		return SEAMLESSRDP_NORMAL;

	return_words = (uint32 *) prop_return;

	for (item = 0; item < nitems_return; item++)
	{
		if (return_words[item] == g_net_wm_state_maximized_vert_atom)
			maximized_vert = True;
		if (return_words[item] == g_net_wm_state_maximized_horz_atom)
			maximized_horz = True;
		if (return_words[item] == g_net_wm_state_hidden_atom)
			hidden = True;
	}

	XFree(prop_return);

	if (maximized_vert && maximized_horz)
		return SEAMLESSRDP_MAXIMIZED;
	else if (hidden)
		return SEAMLESSRDP_MINIMIZED;
	else
		return SEAMLESSRDP_NORMAL;
}

/* 
   Set the window state: normal/minimized/maximized. 
   Returns -1 on failure. 
*/
int
ewmh_change_state(Window wnd, int state)
{
	Status status;
	XEvent xevent;

	/*
	 * Deal with the hidden atom
	 */
	xevent.type = ClientMessage;
	xevent.xclient.window = wnd;
	xevent.xclient.message_type = g_net_wm_state_atom;
	xevent.xclient.format = 32;
	if (state == SEAMLESSRDP_MINIMIZED)
		xevent.xclient.data.l[0] = _NET_WM_STATE_ADD;
	else
		xevent.xclient.data.l[0] = _NET_WM_STATE_REMOVE;
	xevent.xclient.data.l[1] = g_net_wm_state_hidden_atom;
	xevent.xclient.data.l[2] = 0;
	xevent.xclient.data.l[3] = 0;
	xevent.xclient.data.l[4] = 0;
	status = XSendEvent(g_display, DefaultRootWindow(g_display), False,
			    SubstructureNotifyMask | SubstructureRedirectMask, &xevent);
	if (!status)
		return -1;


	/*
	 * Deal with the max atoms
	 */
	xevent.type = ClientMessage;
	xevent.xclient.window = wnd;
	xevent.xclient.message_type = g_net_wm_state_atom;
	xevent.xclient.format = 32;
	if (state == SEAMLESSRDP_MAXIMIZED)
		xevent.xclient.data.l[0] = _NET_WM_STATE_ADD;
	else
		xevent.xclient.data.l[0] = _NET_WM_STATE_REMOVE;
	xevent.xclient.data.l[1] = g_net_wm_state_maximized_vert_atom;
	xevent.xclient.data.l[2] = g_net_wm_state_maximized_horz_atom;
	xevent.xclient.data.l[3] = 0;
	xevent.xclient.data.l[4] = 0;
	status = XSendEvent(g_display, DefaultRootWindow(g_display), False,
			    SubstructureNotifyMask | SubstructureRedirectMask, &xevent);
	if (!status)
		return -1;

	return 0;
}

#endif /* MAKE_PROTO */


#if 0

/* FIXME: _NET_MOVERESIZE_WINDOW is for pagers, not for
   applications. We should implement _NET_WM_MOVERESIZE instead */

int
ewmh_net_moveresize_window(Window wnd, int x, int y, int width, int height)
{
	Status status;
	XEvent xevent;
	Atom moveresize;

	moveresize = XInternAtom(g_display, "_NET_MOVERESIZE_WINDOW", False);
	if (!moveresize)
	{
		return -1;
	}

	xevent.type = ClientMessage;
	xevent.xclient.window = wnd;
	xevent.xclient.message_type = moveresize;
	xevent.xclient.format = 32;
	xevent.xclient.data.l[0] = StaticGravity | (1 << 8) | (1 << 9) | (1 << 10) | (1 << 11);
	xevent.xclient.data.l[1] = x;
	xevent.xclient.data.l[2] = y;
	xevent.xclient.data.l[3] = width;
	xevent.xclient.data.l[4] = height;

	status = XSendEvent(g_display, DefaultRootWindow(g_display), False,
			    SubstructureNotifyMask | SubstructureRedirectMask, &xevent);
	if (!status)
		return -1;
	return 0;
}

#endif
