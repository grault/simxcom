#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

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

Window *get_inactive_windows(Display *display, Window root, Window active_window,
                            unsigned long *n_windows)
{
    Atom prop = XInternAtom(display, "_NET_CLIENT_LIST", True), type;
    Window *client_windows, *inactive_windows;
    int format;
    unsigned long extra;
    unsigned char *result = NULL;

    if(XGetWindowProperty(display, root, prop, 0, 1024, False, XA_WINDOW, &type,
        &format, n_windows, &extra, &result) == Success && result) {
            client_windows = (Window *)result;
            if(active_window)
                (*n_windows)--;
            inactive_windows = (Window *)malloc(*n_windows * sizeof(Window));

            for(int i = 0, j = 0; i < *n_windows + 1; i++) {
                if(client_windows[i] != active_window) {
                    inactive_windows[j] = client_windows[i];
                    j++;
                }
            }
            XFree(client_windows);
            return inactive_windows;
    }
    else
        return NULL;
}

char *get_window_name(Display *display, Window window)
{
    Atom prop = XInternAtom(display, "WM_NAME", False), type;
    int format;
    unsigned long extra, len;
    unsigned char *result;

    if(window == None)
        return NULL;

    if(XGetWindowProperty(display, window, prop, 0, 1024, False, AnyPropertyType,
        &type, &format, &len, &extra, &result) == Success && result)
            return (char*)result;
    else
        return NULL;
}

void draw_rectangle(cairo_t *cr, int x, int y, int w, int h, Color c)
{
    cairo_set_source_rgba(cr, c.red, c.green, c.blue, c.alpha);
    cairo_rectangle(cr, x, y, w, h);
    cairo_fill(cr);
}

Window overlay_active(Display *display, Window root, XVisualInfo vinfo,
    Window active, cairo_surface_t* surf, cairo_t* cr,
    Color color)
{
    Window r;
    int x, y;
    unsigned int w, h, bw, d;
    XGetGeometry(display, active, &r, &x, &y, &w, &h, &bw, &d);

    int width, height;
    width = height = fmin(w/3, h/3);
    
    x = (w - width)/2;
    y = (h - height)/2;

    Window overlay = XCreateSimpleWindow(display, active, x, y, width, height, 0, 0, 0);
    XMapWindow(display, overlay);

    surf = cairo_xlib_surface_create(display, overlay, vinfo.visual, width, height);
    cr = cairo_create(surf);

    draw_rectangle(cr, 0, 0, width, height, color);
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
    
    /* Parse command line arguments */
    Color ac;

    Display *display = XOpenDisplay(NULL); if(!display) exit(EXIT_FAILURE);

    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);

    XVisualInfo vinfo;
    if (!XMatchVisualInfo(display, screen, 32, TrueColor, &vinfo))
        die("32-bit color not supported");
    
    long root_event_mask = PropertyChangeMask;
    XSelectInput(display, root, root_event_mask);

    Atom net_active_window = XInternAtom(display, "_NET_ACTIVE_WINDOW", True);
    
    Window active_window = get_active_window(display, root);

    int n_windows;
    Window *inactive_windows = get_inactive_windows(display, root,
        active_window, (unsigned long *)&n_windows);

    ac.alpha = 0.5; ac.red   = 1.0;
    ac.green = ac.blue  = 0.0;


    Window aw_overlay;
    cairo_surface_t *aw_surf = NULL;
    cairo_t *aw_cr = NULL;

    if(active_window)
        aw_overlay = overlay_active(display, root, vinfo, active_window,
            aw_surf, aw_cr, ac);


    do {
        XEvent event;
        XPropertyEvent property_event;
        XNextEvent(display, &event);

        if(event.type == PropertyNotify) {
            property_event = event.xproperty;
            if(property_event.atom == net_active_window) {
                if(active_window) { // destroy previous aw_overlay window
                    cairo_destroy(aw_cr);
                    cairo_surface_destroy(aw_surf);
                    XDestroyWindow(display, aw_overlay);
                }
                active_window = get_active_window(display, root);
                if(active_window)
                    aw_overlay = overlay_active(display, root, vinfo,
                        active_window, aw_surf, aw_cr, ac);

            }
        }
    } while(!quit);

    cairo_destroy(aw_cr);
    cairo_surface_destroy(aw_surf);
    XDestroyWindow(display, aw_overlay);

    free(inactive_windows);
    XCloseDisplay(display);
    return EXIT_SUCCESS;
}
