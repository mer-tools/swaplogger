/**
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
 *
 *  This library can be LD_PRELOADed to log all calls to eglSwapBuffers and
 *  estimate the frame rendering rate.
 *
 *  Sami Kyöstilä <sami.kyostila@nokia.com>
 */
#include <sys/time.h>

#include <time.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <float.h>
#include <termios.h>
#include <signal.h>
#include <pthread.h>
#include <stdint.h>

#include "swaplogger.h"

#define MAX_TIMESTAMPS  4096

#if defined(USE_EGL)
#   include "swaplogger_egl.h"
#endif

#if defined(USE_XSHM)
#   include "swaplogger_xshm.h"
#endif

#if defined(USE_XDAMAGE)
#   include "swaplogger_xdamage.h"
#endif

static int timestampCount = 64;
static int64_t timestamps[MAX_TIMESTAMPS] = {0};
static int64_t baseTime = 0;
static int frameCounter = 0;
static int verbose = 1;
static int interactive = 0;
static int roundResults = 1;
static int showGeometry = 0;
static char processName[256];
static int resetRequested = 0;
static FILE* output = 0;
static struct termios savedTermState;
static pthread_mutex_t swapLock = PTHREAD_MUTEX_INITIALIZER;

/**
 *  Statistics
 *
 *  Note that these are collected since the most recent reset.
 */
static struct
{
    /** Instantaneous FPS */
    float instFps;

    /** Minimum FPS */
    float minFps;

    /** Maximum FPS */
    float maxFps;

    /** Average frame duration */
    float avgDuration;

    /** Moving average FPS */
    float movingAvgFps;
} stats;

static void printStatistics(void);

static int64_t getTime(void)
{
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    return (int64_t)(t.tv_nsec) + (t.tv_sec * 1000ULL * 1000ULL * 1000ULL);
}

int getProcessName(char* name, int length)
{
    int bytes;
    char *s;

    bytes = readlink("/proc/self/exe", name, length);
    if (bytes < 0)
    {
        return 0;
    }
    if (bytes > length)
    {
        bytes = length;
    }

    /* Append a terminating NULL */
    name[bytes] = 0;

    /* Separate the binary name */
    s = name + bytes;

    while (s > name && *s != '/')
    {
        s--;
    }
    if (s != name)
    {
        int i;
        for (i = 0; i < length; i++)
        {
            name[i] = s[i + 1];
            if (!s[i + 1]) break;
        }
    }
    return 1;
}

float milliseconds(uint64_t microseconds)
{
    return microseconds / (1000.0f * 1000.0f);
}

static void printInfo(const char *info)
{
    printf("INFO -- %.2f -- %s -- %s\n",
           milliseconds(getTime() - baseTime), processName, info);
}

static void cleanup(void)
{
    if (interactive)
    {
        tcsetattr(0, TCSANOW, &savedTermState);
    }
    printStatistics();

#if defined(USE_EGL)
    eglCleanup();
#endif /* USE_EGL */

#if defined(USE_XSHM)
    xshmCleanup();
#endif /* USE_XSHM */

#if defined(USE_XDAMAGE)
    damageCleanup();
#endif /* USE_XDAMAGE */
}

static void handleInterrupt(int sig)
{
    signal(sig, SIG_DFL);
    cleanup();
    raise(sig);
}

static void handleReset(int sig)
{
    resetRequested = 1;
    signal(sig, handleReset);
}

static void reset(void)
{
    frameCounter = 0;
    stats.instFps = 0.0f;
    stats.minFps = 1.0f / 0.0f;
    stats.maxFps = 0.0f;
    stats.avgDuration = 0.0f;
    stats.movingAvgFps = 0.0f;
}

void initSwapLogger(void)
{
#if defined(USE_EGL)
    if (!eglInit())
    {
        printInfo("Unable to initialize EGL hook");
        exit(1);
    }
#endif /* USE_EGL */

#if defined(USE_XSHM)
    if (!xshmInit())
    {
        printInfo("Unable to initialize XSHM hook");
        exit(1);
    }
#endif /* USE_XSHM */

#if defined(USE_XDAMAGE)
    if (!damageInit())
    {
        printInfo("Unable to initialize X damage tracking");
    }
#endif /* USE_XDAMAGE */

    getProcessName(processName, sizeof(processName));

    output = stdout;

    if (getenv("SL_VERBOSE"))
    {
        verbose = atoi(getenv("SL_VERBOSE"));
    }
    if (getenv("SL_INTERACTIVE"))
    {
        interactive = atoi(getenv("SL_INTERACTIVE"));
    }
    if (getenv("SL_PERIOD"))
    {
        timestampCount = atoi(getenv("SL_PERIOD"));
    }
    if (getenv("SL_OUTPUT"))
    {
        output = fopen(getenv("SL_OUTPUT"), "w");
        if (!output)
        {
            perror("fopen");
            output = stdout;
        }
    }
    if (getenv("SL_ROUND"))
    {
        roundResults = atoi(getenv("SL_ROUND"));
    }
    if (getenv("SL_SHOW_GEOMETRY"))
    {
        showGeometry = atoi(getenv("SL_SHOW_GEOMETRY"));
    }
#if defined(USE_XSHM)
    if (getenv("SL_COUNT_X"))
    {
        count_XSHMPutImage = atoi(getenv("SL_COUNT_X"));
    }
#endif /* USE_XSHM */

#if defined(USE_EGL)
    if (getenv("SL_COUNT_EGL"))
    {
        count_eglSwapBuffers = atoi(getenv("SL_COUNT_EGL"));
    }
#endif /* USE_EGL */

#if defined(USE_XDAMAGE)
    if (getenv("SL_COUNT_XDAMAGE"))
    {
        count_XDamage = atoi(getenv("SL_COUNT_XDAMAGE"));
    }
#endif /* USE_XDAMAGE */

    if (interactive)
    {
        int flags = fcntl(0, F_GETFL);
        struct termios term;

        signal(SIGINT, handleInterrupt);

        flags |= O_NONBLOCK;
        fcntl(0, F_SETFL, flags);

        tcgetattr(0, &term);
        savedTermState = term;
        term.c_lflag &= ~ICANON;
        term.c_lflag &= ~ECHO;
        tcsetattr(0, TCSANOW, &term);
        setbuf(stdin, NULL);
    }

    signal(SIGUSR1, handleReset);
    atexit(cleanup);

    baseTime = getTime();
    reset();
    printInfo("Swap logger initialized");
}

static float estimateMovingAverageFps(void)
{
    int i;
    int n = 0;
    uint64_t duration = 0;

    if (frameCounter < timestampCount)
    {
        return 0.0f;
    }
    for (i = frameCounter - timestampCount + 1; i <= frameCounter; i++)
    {
        duration += timestamps[i % MAX_TIMESTAMPS] - timestamps[(i - 1) % MAX_TIMESTAMPS];
        n++;
    }
    assert(n == timestampCount);
    if (duration == 0)
    {
        return 0.0f;
    }
    return (1000.0f * 1000.0f * 1000.0f * timestampCount) / duration;
}

static float instantaneousFps(uint64_t duration)
{
    if (duration == 0)
    {
        return 0.0f;
    }
    return (1000.0f * 1000.0f * 1000.0f / duration);
}

static void printStatistics(void)
{
    int64_t time = timestamps[(MAX_TIMESTAMPS + frameCounter - 1) % MAX_TIMESTAMPS];
    fprintf(output,
            "STAT -- %.2f -- %s -- frame:%d ifps:%.2f min:%.2f max:%.2f apfs_%d:%.2f afps:%.2f\n",
            milliseconds(time - baseTime), processName, frameCounter,
            stats.instFps, stats.minFps, stats.maxFps, timestampCount,
            stats.movingAvgFps, instantaneousFps(stats.avgDuration));
}

static void handleInput(void)
{
    char cmd;
    ssize_t len;

    len = read(0, &cmd, sizeof(cmd));

    if (len <= 0 || len > sizeof(cmd))
    {
        return;
    }

    if (cmd == 'r')
    {
        resetRequested = 1;
    }
    else if (cmd == 'h')
    {
        printInfo("Commands:");
        printInfo("    r: Reset fps calculation");
        printInfo("    q: Toggle quiet mode");
        printInfo("    w: Toggle result rounding");
        printInfo("    h: Show help");
    }
    else if (cmd == 'q')
    {
        verbose = 1 - verbose;
        printInfo(verbose ? "Quiet mode disabled" : "Quiet mode enabled");
    }
    else if (cmd == 'w')
    {
        roundResults = 1 - roundResults;
        printInfo(roundResults ? "Result rounding enabled" : "Result rounding disabled");
    }
}

static void updateStatistics(int64_t duration)
{
    float fps = instantaneousFps(duration);

    stats.instFps = fps;
    if (frameCounter == 1)
    {
        stats.avgDuration = (float)duration;
    }
    else if (frameCounter > 1)
    {
        stats.avgDuration += (1.0f / frameCounter) * ((float)duration - stats.avgDuration);
    }

    if (frameCounter > 0)
    {
        if (fps < stats.minFps)
        {
            stats.minFps = fps;
        }
        if (fps > stats.maxFps)
        {
            stats.maxFps = fps;
        }
    }
    stats.movingAvgFps = estimateMovingAverageFps();
}

static void printGeometry(const char* source, int numRects,
                          const struct Rect* rects)
{
    int64_t time = getTime();
    static const char* format = "%-04s -- %.2f -- %s -- ";
    int i;

    fprintf(output, format, source,
            milliseconds(time - baseTime), processName);

    for (i = 0; i < numRects; i++)
    {
        fprintf(output, "x:%d y:%d w:%d h:%d%s",
                rects[i].x, rects[i].y, rects[i].w, rects[i].h,
                (i == numRects - 1) ? "\n" : "  ");
    }
}

void registerSwap(const char* source, int numRects,
                  const struct Rect* rects)
{
    int64_t time;
    int64_t duration = 0;
    int ignoreSwap = 0;

    pthread_mutex_lock(&swapLock);

#if defined(USE_EGL) && defined(USE_XDAMAGE)
    /* When both EGL or XSHM and X damage events are enabled, only count the
     * damage events as actual frames
     */
    if ((count_XSHMPutImage || count_eglSwapBuffers) &&
        count_XDamage && strncmp(source, "XDMG", 4))
    {
        ignoreSwap = 1;
    }
#endif

    if (!ignoreSwap)
    {
        time = timestamps[frameCounter % MAX_TIMESTAMPS] = getTime();
        if (frameCounter > 0)
        {
            duration = time - timestamps[(frameCounter - 1) % MAX_TIMESTAMPS];
        }
        updateStatistics(duration);
    }
    else
    {
        time = getTime();
    }

    if (verbose || (frameCounter % timestampCount) == 0)
    {
        if (!ignoreSwap)
        {
            static const char* formatRounded =
                "%-04s -- %.2f -- %s -- frame:%d dur:%.2f ifps:%.2f min:%.2f max:%.2f apfs_%d:%.2f afps:%.2f\n";
            static const char* formatUnrounded =
                "%-04s -- %.2f -- %s -- frame:%d dur:%f ifps:%f min:%f max:%f apfs_%d:%f afps:%f\n";
            fprintf(output,
                    roundResults ? formatRounded : formatUnrounded, source,
                    milliseconds(time - baseTime), processName, frameCounter, milliseconds(duration),
                    stats.instFps, stats.minFps, stats.maxFps, timestampCount,
                    stats.movingAvgFps, instantaneousFps(stats.avgDuration));
            if (showGeometry && numRects > 0 && rects)
            {
                printGeometry(source, numRects, rects);
            }
        }
        else
        {
            if (showGeometry && numRects > 0 && rects)
            {
                printGeometry(source, numRects, rects);
            }
            else
            {
                static const char* format = "%-04s -- %.2f -- %s\n";
                fprintf(output, format, source,
                        milliseconds(time - baseTime), processName);
            }
        }
    }

    if (!ignoreSwap)
    {
        frameCounter++;
    }

    if (interactive)
    {
        handleInput();
    }

    if (resetRequested)
    {
        resetRequested = 0;
        printStatistics();
        reset();
        printInfo("Swap logger reset");
    }

    pthread_mutex_unlock(&swapLock);
}
