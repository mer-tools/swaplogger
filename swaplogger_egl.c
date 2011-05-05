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
#include "swaplogger_egl.h"

#include <EGL/egl.h>

#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

typedef void (*EGLFunction)();

typedef EGLAPI EGLBoolean EGLAPIENTRY (*eglSwapBuffers_ptr)(EGLDisplay dpy, EGLSurface surface);
typedef EGLAPI EGLBoolean EGLAPIENTRY (*eglSwapBuffersRegion2_ptr)(EGLDisplay dpy, EGLSurface surface,
                                                                   EGLint count, const EGLint* rects);
typedef EGLAPI EGLFunction EGLAPIENTRY (*eglGetProcAddress_ptr)(const char* procName);

static void* eglLibrary = 0;
static eglSwapBuffers_ptr real_eglSwapBuffers = 0;
static eglGetProcAddress_ptr real_eglGetProcAddress = 0;
static eglSwapBuffersRegion2_ptr real_eglSwapBuffersRegion2 = 0;
int count_eglSwapBuffers = 1;

int eglInit(void)
{
    eglLibrary = dlopen("libEGL.so", RTLD_NOW);
    real_eglSwapBuffers = (eglSwapBuffers_ptr)dlsym(eglLibrary, "eglSwapBuffers");
    if (!real_eglSwapBuffers)
    {
        printf("Unable to look up eglSwapBuffers");
        return 0;
    }
    real_eglGetProcAddress = (eglGetProcAddress_ptr)dlsym(eglLibrary, "eglGetProcAddress");
    if (!real_eglGetProcAddress)
    {
        printf("Unable to look up eglGetProcAddress");
        return 0;
    }
    return 1;
}

void eglCleanup(void)
{
    dlclose(eglLibrary);
}

EGLAPI EGLBoolean EGLAPIENTRY eglSwapBuffers(EGLDisplay dpy, EGLSurface surface)
{
    if (!real_eglSwapBuffers)
    {
        initSwapLogger();
    }

    if (count_eglSwapBuffers)
    {
        struct Rect rect = {.x = 0, .y = 0};
        eglQuerySurface(dpy, surface, EGL_WIDTH, &rect.w);
        eglQuerySurface(dpy, surface, EGL_HEIGHT, &rect.h);

        registerSwap("EGL", 1, &rect);
    }

    return real_eglSwapBuffers(dpy, surface);
}

EGLAPI EGLBoolean EGLAPIENTRY eglSwapBuffersRegion2(EGLDisplay dpy, EGLSurface surface,
                                                    EGLint count, const EGLint* rects)
{
    if (count_eglSwapBuffers)
    {
        registerSwap("EGL", count, (const struct Rect*)rects);
    }

    return real_eglSwapBuffersRegion2(dpy, surface, count, rects);
}

EGLAPI EGLFunction EGLAPIENTRY eglGetProcAddress(const char* procName)
{
    EGLFunction f;

    if (!real_eglGetProcAddress)
    {
        initSwapLogger();
    }

    f = real_eglGetProcAddress(procName);

    if (procName && !strcmp(procName, "eglSwapBuffersRegion2NOK"))
    {
        real_eglSwapBuffersRegion2 = (eglSwapBuffersRegion2_ptr)f;
        f = (EGLFunction)eglSwapBuffersRegion2;
    }

    return f;
}
