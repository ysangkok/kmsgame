/*
 * Copyright (c) 2011-2012 David Herrmann <dh.herrmann@googlemail.com>
 * Copyright (c) 2011 University of Tuebingen
 */

void print_help() {};

#include <dlfcn.h>
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
#include "shl_log.h"
#include "uterm_video.h"
#include "uterm_input.h"
#include "uterm_monitor.h"
#include "test_include.h"

typedef pixman_image_t* (*game_getframe_function_type)(void);
typedef void (*game_putinput_function_type)(int, char*, bool, int);

game_getframe_function_type game_getframe = NULL;
game_putinput_function_type game_putinput = NULL;

static struct uterm_input *input;
struct uterm_video *video;
static void sig_quit(struct ev_eloop *p,
struct signalfd_siginfo *info,
void *data)
{
	if (!input)
		return;

	if (uterm_input_is_awake(input))
	{
		uterm_input_sleep(input);
		log_info("Went to sleep\n");
	}
	else
	{
		uterm_input_wake_up(input);
		log_info("Woke Up\n");
	}
}


struct
{
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
	if (mods & UTERM_ALT_MASK)
		printf("ALT ");
	if (mods & UTERM_LOGO_MASK)
		printf("LOGO ");
	printf("\n");
}


static void input_arrived(struct uterm_input *input,
struct uterm_input_event *ev,
void *data)
{
	char s[32];

	xkb_keysym_get_name(ev->keysyms[0], s, sizeof(s));
	printf("sym %s ", s);
	if (ev->codepoints[0] != UTERM_INPUT_INVALID)
	{
		/*
		 * Just a proof-of-concept hack. This works because glibc uses
		 * UTF-32 (= UCS-4) as the internal wchar_t encoding.
		 */
		printf("unicode %lc ", ev->codepoints[0]);
	}
	print_modifiers(ev->mods);

	game_putinput(ev->keysyms[0], s, ev->codepoints[0] != UTERM_INPUT_INVALID, ev->codepoints[0]);
}


struct
{
	char *xkb_model;
	char *xkb_layout;
	char *xkb_variant;
	char *xkb_options;
	char *xkb_keymap;
} input_conf;
static void monitor_event(struct uterm_monitor *mon,
struct uterm_monitor_event *ev,
void *data)
{
	int ret;
	char *keymap;

	if (ev->type == UTERM_MONITOR_NEW_SEAT)
	{
		if (strcmp(ev->seat_name, "seat0"))
			return;

		keymap = NULL;
		if (input_conf.xkb_keymap && *input_conf.xkb_keymap)
		{
			ret = shl_read_file(input_conf.xkb_keymap, &keymap,
				NULL);
			if (ret)
				log_error("cannot read keymap file %s: %d",
					input_conf.xkb_keymap, ret);
		}

		ret = uterm_input_new(&input, eloop,
			input_conf.xkb_model,
			input_conf.xkb_layout,
			input_conf.xkb_variant,
			input_conf.xkb_options,
			keymap,
			0, 0);
		if (ret)
			return;
		ret = uterm_input_register_cb(input, input_arrived, NULL);
		if (ret)
			return;
		uterm_input_wake_up(input);
	}
	else if (ev->type == UTERM_MONITOR_FREE_SEAT)
	{
		uterm_input_unregister_cb(input, input_arrived, NULL);
		uterm_input_unref(input);
	}
	else if (ev->type == UTERM_MONITOR_NEW_DEV)
	{
		if (ev->dev_type == UTERM_MONITOR_INPUT)
			uterm_input_add_dev(input, ev->dev_node);
	}
	else if (ev->type == UTERM_MONITOR_FREE_DEV)
	{
		if (ev->dev_type == UTERM_MONITOR_INPUT)
			uterm_input_remove_dev(input, ev->dev_node);
	}
}


static void redraw();

struct uterm_display *iter;

static int blit_outputs(struct uterm_video *video)
{
	int j, ret;

	j = 0;
	iter = uterm_video_get_displays(video);
	for ( ; iter; iter = uterm_display_next(iter))
	{
		log_notice("Activating display %d %p...", j, iter);
		ret = uterm_display_activate(iter, NULL);
		if (ret)
			log_err("Cannot activate display %d: %d", j, ret);
		else
			log_notice("Successfully activated display %d", j);

		ret = uterm_display_set_dpms(iter, UTERM_DPMS_ON);
		if (ret)
			log_err("Cannot set DPMS to ON: %d", ret);

		break;

		++j;
	}

	redraw();

	log_notice("Looping...");
	ev_eloop_run(eloop, 5000);
	log_notice("Exiting...");

	return 0;
}


static void redraw()
{
	int ret;

	printf("calling!\n");
	pixman_image_t* filimg = game_getframe();

	struct uterm_video_buffer buf = { .width = 1280, .height = 800, .stride = pixman_image_get_stride(filimg), .format = UTERM_FORMAT_XRGB32, .data = (uint8_t*) pixman_image_get_data(filimg)};

	uterm_display_blit(iter, &buf, 0, 0);
	ret = uterm_display_swap(iter, true);
	if (ret)
	{
		log_err("Cannot swap screen: %d", ret);
	}

}


struct conf_option options[] =
{
	TEST_OPTIONS,
	CONF_OPTION_STRING(0, "dev", &output_conf.dev, NULL),
	CONF_OPTION_STRING(0, "xkb-layout", &output_conf.xkb_layout, "us"),
	CONF_OPTION_STRING(0, "xkb-variant", &output_conf.xkb_variant, ""),
	CONF_OPTION_STRING(0, "xkb-options", &output_conf.xkb_options, ""),

};
struct ev_timer *redraw_timer;
static void redraw_timer_event(struct ev_timer *timer, uint64_t num, void *data)
{
	printf("redraw event!\n");
	redraw();
}


int main(int argc, char **argv)
{

	void* game_library = dlopen("game.so", RTLD_LAZY);
	if (game_library == NULL)
	{
		abort();
	}

	game_putinput = dlsym(game_library, "game_putinput");
	game_getframe = dlsym(game_library, "game_getframe");

	if (game_putinput == NULL) abort();
	if (game_getframe == NULL) abort();

	struct itimerspec spec;
	memset(&spec, 0, sizeof(spec));

	struct uterm_video *video;
	int ret;
	const char *node;
	const struct uterm_video_module *mode;
	size_t onum;

	onum = sizeof(options) / sizeof(*options);
	ret = test_prepare(options, onum, argc, argv, &eloop);
	if (ret)
		goto err_fail;

	// input
	if (!setlocale(LC_ALL, ""))
	{
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

	mode = UTERM_VIDEO_DRM2D;
	node = "/dev/dri/card0";

	if (output_conf.dev)
		node = output_conf.dev;

	log_notice("Creating video object using %s...", node);

	ret = uterm_video_new(&video, eloop, node, mode);
	if (ret)
	{
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
	if (ret)
	{
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
