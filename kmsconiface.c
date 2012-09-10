/*
 * Copyright (c) 2011-2012 David Herrmann <dh.herrmann@googlemail.com>
 * Copyright (c) 2011 University of Tuebingen
 */

void print_help() {};

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

#include <errno.h>
#include <linux/input.h>
#include <locale.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signalfd.h>
#include <unistd.h>
#include <X11/keysym.h>

#include "pixman.h"


#include "eloop.h"
#include "log.h"
#include "uterm.h"
#ifndef LONG_BIT
#define LONG_BIT (sizeof(long)*8)
#endif
#define UTERM_HAVE_DUMB
//#include "uterm_internal.h"
#include "log.c"
#include "conf.c"
#include "test_include.h"

#include "game.h"

static struct uterm_input *input;
struct uterm_video *video;
static void sig_quit(struct ev_eloop *p,
			struct signalfd_siginfo *info,
			void *data)
{
	if (!input)
		return;

	if (uterm_input_is_awake(input)) {
		uterm_input_sleep(input);
		log_info("Went to sleep\n");
	} else {
		uterm_input_wake_up(input);
		log_info("Woke Up\n");
	}
}

struct {
	char *dev;
	char *xkb_layout;
	char *xkb_variant;
	char *xkb_options;

} output_conf;
static struct ev_eloop *eloop;

static void print_modifiers(unsigned int mods)
{
	if (mods & UTERM_SHIFT_MASK)
		printf("SHIFT ");
	if (mods & UTERM_LOCK_MASK)
		printf("LOCK ");
	if (mods & UTERM_CONTROL_MASK)
		printf("CONTROL ");
	if (mods & UTERM_MOD1_MASK)
		printf("MOD1 ");
	if (mods & UTERM_MOD2_MASK)
		printf("MOD2 ");
	if (mods & UTERM_MOD3_MASK)
		printf("MOD3 ");
	if (mods & UTERM_MOD4_MASK)
		printf("MOD4 ");
	if (mods & UTERM_MOD5_MASK)
		printf("MOD5 ");
	printf("\n");
}

static void input_arrived(struct uterm_input *input,
				struct uterm_input_event *ev,
				void *data)
{
	char s[32];


	uterm_input_keysym_to_string(input, ev->keysym, s, sizeof(s));
	printf("sym %s ", s);
	if (ev->unicode != UTERM_INPUT_INVALID) {
		/*
		 * Just a proof-of-concept hack. This works because glibc uses
		 * UTF-32 (= UCS-4) as the internal wchar_t encoding.
		 */
		printf("unicode %lc ", (int) ev->unicode);
	}
	print_modifiers(ev->mods);
	game_putinput(ev->keysym, s, ev->unicode != UTERM_INPUT_INVALID, ev->unicode);
}

static void monitor_event(struct uterm_monitor *mon,
				struct uterm_monitor_event *ev,
				void *data)
{
	int ret;

	if (ev->type == UTERM_MONITOR_NEW_SEAT) {
		if (strcmp(ev->seat_name, "seat0"))
			return;

		ret = uterm_input_new(&input, eloop,
				      output_conf.xkb_layout,
				      output_conf.xkb_variant,
				      output_conf.xkb_options);
		if (ret)
			return;
		ret = uterm_input_register_cb(input, input_arrived, NULL);
		if (ret)
			return;
		uterm_input_wake_up(input);
	} else if (ev->type == UTERM_MONITOR_FREE_SEAT) {
		uterm_input_unregister_cb(input, input_arrived, NULL);
		uterm_input_unref(input);
	} else if (ev->type == UTERM_MONITOR_NEW_DEV) {
		if (ev->dev_type == UTERM_MONITOR_INPUT)
			uterm_input_add_dev(input, ev->dev_node);
	} else if (ev->type == UTERM_MONITOR_FREE_DEV) {
		if (ev->dev_type == UTERM_MONITOR_INPUT)
			uterm_input_remove_dev(input, ev->dev_node);
	}
}



#if 0
static int my_display_blit(struct uterm_display *disp,
                        const struct uterm_video_buffer *buf,
                        unsigned int x, unsigned int y)
{
        unsigned int tmp;
        uint8_t *dst, *src;
        struct dumb_rb *rb;
        unsigned int width, height;
        unsigned int sw, sh;

        if (!disp->video || !display_is_online(disp))
                return -EINVAL;
        if (!buf || !video_is_awake(disp->video))
                return -EINVAL;
        if (buf->format != UTERM_FORMAT_XRGB32)
                return -EINVAL;

        rb = &disp->dumb.rb[disp->dumb.current_rb ^ 1];
        sw = disp->current_mode->dumb.info.hdisplay;
        sh = disp->current_mode->dumb.info.vdisplay;

        tmp = x + buf->width;
        if (tmp < x || x >= sw)
                return -EINVAL;
        if (tmp > sw)
                width = sw - x;
        else
                width = buf->width;

        tmp = y + buf->height;
        if (tmp < y || y >= sh)
                return -EINVAL;
        if (tmp > sh)
                height = sh - y;
        else
                height = buf->height;

        dst = rb->map;
        dst = &dst[y * rb->stride + x * 4];
        src = buf->data;

        while (height--) {
                memcpy(dst, src, 4 * width);
                dst += rb->stride;
                src += buf->stride;
        }

        return 0;
}
#endif
static void redraw();

static int blit_outputs(struct uterm_video *video)
{
	int j, ret;
	struct uterm_display *iter;

	j = 0;
	iter = uterm_video_get_displays(video);
	for ( ; iter; iter = uterm_display_next(iter)) {
		log_notice("Activating display %d %p...", j, iter);
		ret = uterm_display_activate(iter, NULL);
		if (ret)
			log_err("Cannot activate display %d: %d", j, ret);
		else
			log_notice("Successfully activated display %d", j);

		ret = uterm_display_set_dpms(iter, UTERM_DPMS_ON);
		if (ret)
			log_err("Cannot set DPMS to ON: %d", ret);

		++j;
	}

	redraw();

	log_notice("Looping...");
	ev_eloop_run(eloop, 5000);
	log_notice("Exiting...");

	return 0;
}

static void redraw() {
	struct uterm_display *iter;
	struct uterm_screen *screen;
	int ret;

	iter = uterm_video_get_displays(video);
	for ( ; iter; iter = uterm_display_next(iter)) {
		if (uterm_display_get_state(iter) != UTERM_DISPLAY_ACTIVE)
			continue;

		ret = uterm_screen_new_single(&screen, iter);
		if (ret) {
			log_err("Cannot create temp-screen object: %d", ret);
			continue;
		}

pixman_image_t* filimg = game_getframe();

struct uterm_video_buffer buf = { .width = 1280, .height = 800, .stride = pixman_image_get_stride(filimg), .format = UTERM_FORMAT_XRGB32, .data = (uint8_t*) pixman_image_get_data(filimg)};

/*
uint32_t prev = -1;

// check changed alpha
for (int i=0; i<1280*800; i++) {
	uint8_t a = (buf.data[i] & 0xff000000) >> 24;
	if (a != prev) printf("%08x\n", prev=a);
}
*/

		//my_display_blit(screen->disp, &buf, 0, 0);
		uterm_screen_blit(screen, &buf, 0, 0);
		ret = uterm_screen_swap(screen);
		if (ret) {
			log_err("Cannot swap screen: %d", ret);
			uterm_screen_unref(screen);
			continue;
		}

		//log_notice("Successfully set screen on display %p", iter);
		uterm_screen_unref(screen);
	}

}

struct conf_option options[] = {
	TEST_OPTIONS,
	CONF_OPTION_STRING(0, "dev", NULL, &output_conf.dev, NULL),
	CONF_OPTION_STRING(0, "xkb-layout", NULL, &output_conf.xkb_layout, "us"),
	CONF_OPTION_STRING(0, "xkb-variant", NULL, &output_conf.xkb_variant, ""),
	CONF_OPTION_STRING(0, "xkb-options", NULL, &output_conf.xkb_options, ""),

};
struct ev_timer *redraw_timer;
static void redraw_timer_event(struct ev_timer *timer, uint64_t num, void *data)
{
	redraw();
}
int main(int argc, char **argv)
{
	struct itimerspec spec;
	memset(&spec, 0, sizeof(spec));
	int ret;
	unsigned int mode;
	const char *node;
	size_t onum;

	onum = sizeof(options) / sizeof(*options);
	ret = test_prepare(options, onum, argc, argv, &eloop);
	if (ret)
		goto err_fail;

// input
	if (!setlocale(LC_ALL, "")) {
		log_err("Cannot set locale: %m");
		ret = -EFAULT;
		goto err_exit;
	}
	struct uterm_monitor *mon;

	ret = uterm_monitor_new(&mon, eloop, monitor_event, NULL);
	if (ret)
		goto err_exit;

	ret = ev_eloop_register_signal_cb(eloop, SIGQUIT, sig_quit, NULL);
	if (ret)
		goto err_mon;

	uterm_monitor_scan(mon);

// /input


	mode = UTERM_VIDEO_DUMB;
	node = "/dev/dri/card0";

	if (output_conf.dev)
		node = output_conf.dev;

	log_notice("Creating video object using %s...", node);

	ret = uterm_video_new(&video, eloop, mode, node);
	if (ret) {
			goto err_exit;
	}

	log_notice("Waking up video object...");
	ret = uterm_video_wake_up(video);
	if (ret < 0)
		goto err_unref;

{
        memset(&spec, 0, sizeof(spec));
        spec.it_value.tv_nsec = 1;
        spec.it_interval.tv_nsec = 100000000;

	ev_eloop_new_timer(eloop, &redraw_timer, &spec, redraw_timer_event, 0);
}

	ret = blit_outputs(video);
	if (ret) {
		log_err("Cannot set outputs: %d", ret);
		goto err_unref;
	}
	ev_eloop_unregister_signal_cb(eloop, SIGQUIT, sig_quit, NULL);

err_mon:
	uterm_monitor_unref(mon);
err_unref:
	uterm_video_unref(video);
err_exit:
	test_exit(options, onum, eloop);
err_fail:
	if (ret != -ECANCELED)
		test_fail(ret);
	return abs(ret);
}
