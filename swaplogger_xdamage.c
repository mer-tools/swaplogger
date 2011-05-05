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
#include "swaplogger_xdamage.h"

#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xdamage.h>

static void *eventThread(void *data);

int count_XDamage = 1;

struct {
    Display* dpy;
    Damage damage;
    int eventBase;
    int done;

    pthread_t eventThread;
} damage;

int damageInit(void)
{
    int errBase;

    memset(&damage, 0, sizeof(damage));

    damage.dpy = XOpenDisplay(NULL);

    if (!damage.dpy)
    {
        printf("Unable to open display\n");
        goto out;
    }

    if (!XDamageQueryExtension(damage.dpy, &damage.eventBase, &errBase))
    {
        printf("Damage extension not supported\n");
        goto out;
    }

    damage.damage = XDamageCreate(damage.dpy, XDefaultRootWindow(damage.dpy),
                                  XDamageReportNonEmpty);
    if (!damage.damage)
    {
        printf("Unable to create damage handle\n");
        goto out;
    }

    pthread_create(&damage.eventThread, NULL, eventThread, NULL);
    return 1;

out:
    damageCleanup();
    return 0;
}

void damageCleanup(void)
{
    damage.done = 1;
    pthread_join(damage.eventThread, NULL);

    if (damage.damage)
    {
        XDamageDestroy(damage.dpy, damage.damage);
    }

    if (damage.dpy)
    {
        XCloseDisplay(damage.dpy);
    }
}

void *eventThread(void* data)
{
    (void)data;
    XEvent event;

    while (!damage.done)
    {
        if (XNextEvent(damage.dpy, &event) == 0)
        {
            if (event.type == damage.eventBase + XDamageNotify)
            {
                const XDamageNotifyEvent* e = (const XDamageNotifyEvent*)&event;
                if (count_XDamage)
                {
                    struct Rect rect =
                    {
                        .x = e->geometry.x,     .y = e->geometry.y,
                        .w = e->geometry.width, .h = e->geometry.height
                    };
                    registerSwap("XDMG", 1, &rect);
                }
                XDamageSubtract(damage.dpy, e->damage, None, None);
            }
        }
    }

    return NULL;
}
