/**
 *  Swap logger
 *  Copyright (c) 2011 Nokia
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE.
 */
#include "swaplogger.h"
#include "swaplogger_xshm.h"

#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>

#include <stdio.h>
#include <dlfcn.h>

typedef Status (*XShmPutImage_ptr)(Display* display, Drawable d, GC gc, XImage*
        image, int src_x, int src_y, int dest_x, int dest_y, unsigned int
        width, unsigned int height, Bool send_event);

static void* xextLibrary = 0;
static XShmPutImage_ptr real_XShmPutImage = 0;
int count_XSHMPutImage = 1;

int xshmInit(void)
{
    xextLibrary = dlopen("libXext.so.6", RTLD_NOW);
    real_XShmPutImage = (XShmPutImage_ptr)dlsym(xextLibrary, "XShmPutImage");
    if (!real_XShmPutImage)
    {
        printf("Unable to look up XShmPutImage");
        return 0;
    }
    return 1;
}

void xshmCleanup(void)
{
    dlclose(xextLibrary);
}

Status XShmPutImage(Display* display, Drawable d, GC gc, XImage* image,
        int src_x, int src_y, int dest_x, int dest_y,
        unsigned int width, unsigned int height, Bool send_event)
{
    if (!real_XShmPutImage)
    {
        initSwapLogger();
    }

    if (count_XSHMPutImage)
    {
        struct Rect rect =
        {
            .x = dest_x, .y = dest_y,
            .w = width,  .h = height
        };
        registerSwap("XSHM", 1, &rect);
    }

    return real_XShmPutImage(display, d, gc, image, src_x, src_y, dest_x,
                             dest_y, width, height, send_event);
}
