#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>

typedef struct {
    float red;
    float green;
    float blue;
    float alpha;
} Color;

Window get_active_window(Display *display, Window root)
{
    Atom prop = XInternAtom(display, "_NET_ACTIVE_WINDOW", True), type;
    Window active;
    int format;
    unsigned long len, extra;
    unsigned char *result = NULL;

    if(XGetWindowProperty(display, root, prop, 0L, sizeof(active), False, XA_WINDOW,
        &type, &format, &len, &extra, &result) == Success && result) {
            active = *(Window *)result;
            XFree(result);
            return active;
    }
    else
        return None;
}

void query_net_client_list(Display *display, Window root, unsigned long *n_windows)
{
    Atom prop = XInternAtom(display, "_NET_CLIENT_LIST", True), type;
    int format;
    unsigned long extra;
    unsigned char *result = NULL;

    XGetWindowProperty(display, root, prop, 0, 1024, False, XA_WINDOW, &type,
        &format, n_windows, &extra, &result);
}

Window overlay_active(Display *display, Window root, XVisualInfo vinfo, Window active)
{
    Window r;
    int x, y;
    unsigned int w, h, bw, d;
    XGetGeometry(display, active, &r, &x, &y, &w, &h, &bw, &d);

    int width, height;
    width = height = fmin(w/3, h/3);
    
    x = (w - width)/2;
    y = (h - height)/2;


    XMatchVisualInfo(display, DefaultScreen(display), 32, TrueColor, &vinfo);
    XSetWindowAttributes attr;
    attr.colormap = XCreateColormap(display, DefaultRootWindow(display), vinfo.visual, AllocNone);
    attr.border_pixel = 0;
    attr.background_pixel = 0x00FF0000;
    unsigned int border_width = 0;

    Window overlay = XCreateWindow(display, active, x, y, width, height, border_width, vinfo.depth, InputOutput, vinfo.visual, CWColormap | CWBorderPixel | CWBackPixel, &attr);

    XMapWindow(display, overlay);
    XFlush(display);

    return overlay;
}

void die(const char *message, ...)
{
    va_list ap;

    va_start(ap, message);
    fprintf(stderr, "%s: ", "delete this");
    vfprintf(stderr, message, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    bool quit = false;
    /* int exit_code = 0; */
    
    Display *display = XOpenDisplay(NULL); if(!display) exit(EXIT_FAILURE);

    Window root = RootWindow(display, DefaultScreen(display));

    XVisualInfo vinfo;
    if (!XMatchVisualInfo(display, DefaultScreen(display), 32, TrueColor, &vinfo))
        die("32-bit color not supported");
    
    long root_event_mask = PropertyChangeMask;
    XSelectInput(display, root, root_event_mask);

    Atom net_active_window = XInternAtom(display, "_NET_ACTIVE_WINDOW", True);
    
    Window active_window = get_active_window(display, root);

    int n_windows;
    query_net_client_list(display, root, (unsigned long *)&n_windows);

    Window aw_overlay;

    if(active_window)
        aw_overlay = overlay_active(display, root, vinfo, active_window);


    do {
        XEvent event;
        XPropertyEvent property_event;
        XNextEvent(display, &event);

        if(event.type == PropertyNotify) {
            property_event = event.xproperty;
            if(property_event.atom == net_active_window) {
                if(active_window) { // destroy previous aw_overlay window
                    XDestroyWindow(display, aw_overlay);
                }
                active_window = get_active_window(display, root);
                if(active_window)
                    aw_overlay = overlay_active(display, root, vinfo, active_window);

            }
        }
    } while(!quit);

    XDestroyWindow(display, aw_overlay);
    XCloseDisplay(display);

    return EXIT_SUCCESS;
}
