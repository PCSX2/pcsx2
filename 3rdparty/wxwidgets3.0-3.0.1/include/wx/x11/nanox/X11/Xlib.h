/*
 * Xlib compatibility
 */

#ifndef _DUMMY_XLIBH_
#define _DUMMY_XLIBH_

/* Move away the typedef in XtoNX.h */
#define XFontStruct XFontStruct1
#include <XtoNX.h>
#undef XFontStruct
#undef XCharStruct

/* Data types */

typedef GR_PALETTE* Colormap;
typedef GR_DRAW_ID Drawable ;
typedef int Status;
typedef unsigned long VisualID;
typedef int Bool;
typedef long XID;
typedef GR_SCANCODE KeySym;
typedef GR_EVENT_KEYSTROKE XKeyEvent;
typedef struct {
    GR_FONT_INFO info;
    GR_FONT_ID fid;
} XFontStruct;
typedef struct {
    short	lbearing;	/* origin to left edge of raster */
    short	rbearing;	/* origin to right edge of raster */
    short	width;		/* advance to next char's origin */
    short	ascent;		/* baseline to top edge of raster */
    short	descent;	/* baseline to bottom edge of raster */
    unsigned short attributes;	/* per char flags (not predefined) */
} XCharStruct;

/* Configure window value mask bits */
#define   CWX                         (1<<0)
#define   CWY                         (1<<1)
#define   CWWidth                     (1<<2)
#define   CWHeight                    (1<<3)
#define   CWBorderWidth               (1<<4)
#define   CWSibling                   (1<<5)
#define   CWStackMode                 (1<<6)

/* Values */

typedef struct {
        int x, y;
        int width, height;
        int border_width;
        Window sibling;
        int stack_mode;
} XWindowChanges;

/* typedef unsigned long Time; */

#define Success 0
#define GrabSuccess Success
#define GrabNotViewable (Success+1)
#define InputOutput 1
#define InputOnly 2
#define IsUnmapped              0
#define IsUnviewable            1
#define IsViewable              2
/* Is this right? */
#define PropertyChangeMask GR_EVENT_MASK_SELECTION_CHANGED
#define GraphicsExpose GR_EVENT_TYPE_EXPOSURE
#define GraphicsExposeMask GR_EVENT_MASK_EXPOSURE
#define ColormapChangeMask 0
#define FillSolid 0
#define LineSolid 0
#define LineOnOffDash 0
#define CapNotLast 0
#define CapRound 0
#define CapProjecting 0
#define CapButt 0
#define JoinRound 0
#define JoinBevel 0
#define JoinMiter 0
#define IncludeInferiors 0
#define ClipByChildren 0
#define DoRed 0
#define DoGreen 0
#define DoBlue 0
#define NoEventMask GR_EVENT_MASK_NONE
#define RevertToParent 0
#define CurrentTime 0
#define GrabModeAsync 0

#define GXcopy GR_MODE_COPY
#define GXclear GR_MODE_CLEAR
#ifndef GXxor
#define GXxor GR_MODE_OR
#endif
#define GXinvert GR_MODE_INVERT
#define GXorReverse GR_MODE_ORREVERSE
#define GXandReverse GR_MODE_ANDREVERSE
#define GXand GR_MODE_AND
#define GXor GR_MODE_OR
#define GXandInverted GR_MODE_ANDINVERTED
#define GXnoop GR_MODE_NOOP
#define GXnor GR_MODE_NOR
#define GXequiv GR_MODE_EQUIV
#define GXcopyInverted GR_MODE_COPYINVERTED
#define GXorInverted GR_MODE_ORINVERTED
#define GXnand GR_MODE_NAND
#define GXset GR_MODE_SET

#define XSynchronize(display,sync)
#define XDefaultRootWindow(d) GR_ROOT_WINDOW_ID
#define RootWindowOfScreen(s) GR_ROOT_WINDOW_ID
#define XFreePixmap(d, p) GrDestroyWindow(p)
#define XFreeCursor(d, c) GrDestroyCursor(c)
#define XFreeGC(d, gc) GrDestroyGC(gc)
#define XSetBackground(d, gc, c) GrSetGCBackground(gc, c)
#define DefaultVisual(d, s) (NULL)
#define DefaultColormap(d, s) DefaultColormapOfScreen(NULL)
#define DefaultScreenOfDisplay(d) 0
#define XSetFillStyle(d, gc, s) wxNoop()
#define XSetLineAttributes(d, gc, a, b, c, e) wxNoop()
#define XSetClipMask(d, gc, m) wxNoop()
#define XSetTSOrigin(d, gc, x, y) wxNoop()
#define XFillArc(d, w, gc, x, y, rx, ry, a1, a2) GrArcAngle(w, gc, x, y, rx, ry, a1, a2, GR_PIE)
#define XDrawArc(d, w, gc, x, y, rx, ry, a1, a2) GrArcAngle(w, gc, x, y, rx, ry, a1, a2, GR_ARC)
#define XDrawPoint(d, w, gc, x, y) GrPoint(w, gc, x, y)
#define XFillPolygon(d, w, gc, p, n, s, m) GrFillPoly(w, gc, n, p)
#define XDrawRectangle(d, w, gc, x, y, width, height) GrRect(w, gc, x, y, width, height)
#define XSetClipOrigin(d, gc, x, y) GrSetGCClipOrigin(gc, x, y)
#define XSetRegion(d, gc, r) GrSetGCRegion(gc, r)
#define XSetTile(d, gc, p) wxNoop()
#define XSetStipple(d, gc, p) wxNoop()
#define XSetSubwindowMode(d, gc, mode) wxNoop()
#define XFreeColormap(d, cmap) wxNoop()
#define XSetTransientForHint(d, w, p) wxNoop()
#define XUnionRegion(sr1,sr2,r)	GrUnionRegion(r,sr1,sr2)
#define XIntersectRegion(sr1,sr2,r)	GrIntersectRegion(r,sr1,sr2)
#define XEqualRegion(r1, r2) GrEqualRegion(r1, r2)
#define XEmptyRegion(r) GrEmptyRegion(r)
#define XOffsetRegion(r, x, y) GrOffsetRegion(r, x, y)
#define XClipBox(r, rect) GrGetRegionBox(r, rect)
#define XPointInRegion(r, x, y) GrPointInRegion(r, x, y)
#define XXorRegion(sr1, sr2, r) GrXorRegion(r, sr1, sr2)
/* TODO: Cannot find equivalent for this. */
#define XIconifyWindow(d, w, s) 0
#define XCreateWindowWithColor(d,p,x,y,w,h,bw,depth,cl,vis,backColor,foreColor) \
			GrNewWindow(p,x,y,w,h,bw,backColor,foreColor)
#define XLookupString(event, buf, len, sym, status) (*sym = (event)->scancode)
#define XBell(a, b) GrBell()
#define DisplayWidthMM(d, s) 100
#define DisplayHeightMM(d, s) 100

/* These defines are wrongly defined in XtoNX.h, IMHO,
 * since they reference a static global.
 * Redefined as functions, below.
 */

#undef DisplayWidth
#undef DisplayHeight
#undef DefaultDepth

/*
 * Data structure used by color operations
 */
typedef struct {
	unsigned long pixel;
	unsigned short red, green, blue;
	char flags;  /* do_red, do_green, do_blue */
	char pad;
} XColor;

typedef struct {
	int type;
	Display *display;	/* Display the event was read from */
	XID resourceid;		/* resource id */
	unsigned long serial;	/* serial number of failed request */
	unsigned char error_code;	/* error code of failed request */
	unsigned char request_code;	/* Major op-code of failed request */
	unsigned char minor_code;	/* Minor op-code of failed request */
} XErrorEvent;

/*
 * Visual structure; contains information about colormapping possible.
 */
typedef struct {
	void *ext_data;	/* hook for extension to hang data */
	VisualID visualid;	/* visual id of this visual */
#if defined(__cplusplus) || defined(c_plusplus)
	int c_class;		/* C++ class of screen (monochrome, etc.) */
#else
	int class;		/* class of screen (monochrome, etc.) */
#endif
	unsigned long red_mask, green_mask, blue_mask;	/* mask values */
	int bits_per_rgb;	/* log base 2 of distinct color values */
	int map_entries;	/* color map entries */
} Visual;

/*
 * Depth structure; contains information for each possible depth.
 */	
typedef struct {
	int depth;		/* this depth (Z) of the depth */
	int nvisuals;		/* number of Visual types at this depth */
	Visual *visuals;	/* list of visuals possible at this depth */
} Depth;

/*
 * Information about the screen.  The contents of this structure are
 * implementation dependent.  A Screen should be treated as opaque
 * by application code.
 */

struct _XDisplay;		/* Forward declare before use for C++ */

typedef struct {
	void *ext_data;	/* hook for extension to hang data */
	struct _XDisplay *display;/* back pointer to display structure */
	Window root;		/* Root window id. */
	int width, height;	/* width and height of screen */
	int mwidth, mheight;	/* width and height of  in millimeters */
	int ndepths;		/* number of depths possible */
	Depth *depths;		/* list of allowable depths on the screen */
	int root_depth;		/* bits per pixel */
	Visual *root_visual;	/* root visual */
	GC default_gc;		/* GC for the root root visual */
	Colormap cmap;		/* default color map */
	unsigned long white_pixel;
	unsigned long black_pixel;	/* White and Black pixel values */
	int max_maps, min_maps;	/* max and min color maps */
	int backing_store;	/* Never, WhenMapped, Always */
	Bool save_unders;	
	long root_input_mask;	/* initial root input mask */
} Screen;


typedef struct {
     int x, y;		      /* location of window */
     int width, height;       /* width and height of window */
     int border_width;	      /* border width of window */
     int depth; 	      /* depth of window */
     Visual *visual;	      /* the associated visual structure */
     Window root;	      /* root of screen containing window */
     int _class; 	      /* InputOutput, InputOnly*/
     int bit_gravity;	      /* one of the bit gravity values */
     int win_gravity;	      /* one of the window gravity values */
     int backing_store;       /* NotUseful, WhenMapped, Always */
     unsigned long backing_planes;/* planes to be preserved if possible */
     unsigned long backing_pixel;/* value to be used when restoring planes */
     Bool save_under;	      /* boolean, should bits under be saved? */
     Colormap colormap;       /* color map to be associated with window */
     Bool map_installed;      /* boolean, is color map currently installed*/
     int map_state;	      /* IsUnmapped, IsUnviewable, IsViewable */
     long all_event_masks;    /* set of events all people have interest in*/
     long your_event_mask;    /* my event mask */
     long do_not_propagate_mask;/* set of events that should not propagate */
     Bool override_redirect;  /* boolean value for override-redirect */
     Screen *screen;	      /* back pointer to correct screen */
} XWindowAttributes;

typedef int (*XErrorHandler) (	    /* WARNING, this type not in Xlib spec */
    Display*		/* display */,
    XErrorEvent*	/* error_event */
);

/* events*/

/* What should this be? */
#if 0
#ifndef ResizeRequest
#define ResizeRequest ??
#endif
#endif

#ifndef MotionNotify
#define MotionNotify GR_EVENT_TYPE_MOUSE_POSITION
#define PointerMotionMask GR_EVENT_MASK_MOUSE_POSITION
#endif

#define ButtonMotionMask GR_EVENT_MASK_MOUSE_POSITION
#define KeymapStateMask 0
#define StructureNotifyMask GR_EVENT_MASK_UPDATE

#ifdef ConfigureNotify
/* XtoNX.h gets it wrong */
#undef ConfigureNotify
#endif
#define ConfigureNotify GR_EVENT_TYPE_UPDATE

#ifndef FocusIn
#define FocusIn GR_EVENT_TYPE_FOCUS_IN
#define FocusOut GR_EVENT_TYPE_FOCUS_OUT
#define FocusChangeMask GR_EVENT_MASK_FOCUS_IN|GR_EVENT_MASK_FOCUS_OUT
#endif

/* Fuunctions */

#ifdef __cplusplus
extern "C" {
#endif

Display *XOpenDisplay(char *name);
Colormap DefaultColormapOfScreen(Screen* /* screen */) ;
int XSetGraphicsExposures( Display* /* display */, GC /* gc */, Bool /* graphics_exposures */) ;
int XWarpPointer( Display* /* display */, Window /* srcW */, Window /* destW */,
                 int /* srcX */, int /* srcY */,
                 unsigned int /* srcWidth */,
                 unsigned int /* srcHeight */,
                 int destX, int destY);
int XSetInputFocus(Display* /* display */, Window focus, int /* revert_to */, Time /* time */) ;
int XGetInputFocus(Display* /* display */, Window* /* focus_return */, int* /* revert_to_return */) ;
int XGrabPointer(Display* /* display */, Window /* grab_window */,
                 Bool /* owner_events */, unsigned int /* event_mask */,
                 int /* pointer_mode */, int /* keyboard_mode */,
                 Window /* confine_to */, Cursor /* cursor */, Time /* time */) ;
int XUngrabPointer(Display* /* display */, Time /* time */) ;
int XCopyArea(Display* /* display */, Drawable src, Drawable dest, GC gc,
              int src_x, int src_y, unsigned int width, unsigned int height,
              int dest_x, int dest_y) ;
int XCopyPlane(Display* /* display */, Drawable src, Drawable dest, GC gc,
              int src_x, int src_y, unsigned int width, unsigned int height,
              int dest_x, int dest_y, unsigned long /* plane */) ;

XErrorHandler XSetErrorHandler (XErrorHandler /* handler */);
Screen *XScreenOfDisplay(Display* /* display */,
                         int /* screen_number */);
int DisplayWidth(Display* /* display */, int /* screen */);
int DisplayHeight(Display* /* display */, int /* screen */);
int DefaultDepth(Display* /* display */, int /* screen */);
int XAllocColor(Display* /* display */, Colormap /* cmap */,
                XColor* color);
int XParseColor(Display* display, Colormap cmap,
                const char* cname, XColor* color);
int XDrawLine(Display* display, Window win, GC gc,
              int x1, int y1, int x2, int y2);
int XTextExtents( XFontStruct* font, char* s, int len, int* direction,
        int* ascent, int* descent2, XCharStruct* overall);
int XPending(Display *d);
XFontStruct* XLoadQueryFont(Display* display, const char* fontSpec);
int XFreeFont(Display* display, XFontStruct* fontStruct);
int XQueryColor(Display* display, Colormap cmap, XColor* color);
Status XGetWindowAttributes(Display* display, Window w,
                            XWindowAttributes* window_attributes);

int XConfigureWindow(Display* display, Window w, int mask, XWindowChanges* changes);
int XTranslateCoordinates(Display* display, Window srcWindow, Window destWindow, int srcX, int srcY, int* destX, int* destY, Window* childReturn);

void wxNoop();

#ifdef __cplusplus
}
#endif

#define XMaxRequestSize(display) 16384

#endif
  /* _DUMMY_XLIBH_ */
