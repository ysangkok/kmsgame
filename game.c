#include "pixman.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <X11/keysym.h>

int posx = 50, posy = 50;

void game_putinput(int keysym, char* str, bool validunicode, int ucs4char) {
    const int step = 5;
    switch (keysym) {
    case XK_Left:
        posx-=step;
        break;
    case XK_Right:
        posx+=step;
        break;
    case XK_Up:
        posy-=step;
        break;
    case XK_Down:
        posy+=step;
        break;
    }

}

pixman_image_t* game_getframe() {
    printf("asked for frame!\n");
    pixman_image_t* filimg;
    uint32_t *dest3 = malloc (1280*800*4);

    int i;

    for (i=0; i<1280*800; ++i) {
        dest3[i] = 0x00ffbb00;
        //dest4[i] = 0x00ffbb00;
    }
    filimg  = pixman_image_create_bits (PIXMAN_a8r8g8b8, 1280, 800, dest3, 4*1280);
    pixman_image_t *fill;
    pixman_color_t white = {
        0xffff,
        0xffff,
        0xffff,
        0xffff
    };
    pixman_color_t *c = &white;
    fill = pixman_image_create_solid_fill (c);
    pixman_image_composite (PIXMAN_OP_SRC, fill, NULL, filimg, 0, 0, 0, 0, 0, 0, 1280, 800);

    pixman_color_t color = { 0x4444, 0x4444, 0xffff, 0xffff };

#define POINT(x,y)                                                      \
    { pixman_double_to_fixed ((x)), pixman_double_to_fixed ((y)) }

    pixman_triangle_t tris[4] = {
        { POINT (100, 100), POINT (10, 50), POINT (110, 10) },
        { POINT (100, 100), POINT (150, 10), POINT (200, 50) },
        { POINT (100, 100), POINT (10, 170), POINT (90, 175) },
        { POINT (100, 100), POINT (170, 150), POINT (120, 190) },
    };
    pixman_image_t* src_img = pixman_image_create_solid_fill (&color);
    pixman_composite_triangles (PIXMAN_OP_ATOP,
                                src_img,
                                filimg,
                                PIXMAN_a8,
                                200, 200,
                                posx, posy,
                                sizeof(tris) / sizeof(tris[0]), tris);
    return filimg;
}

