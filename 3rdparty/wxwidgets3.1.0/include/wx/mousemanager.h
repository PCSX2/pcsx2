///////////////////////////////////////////////////////////////////////////////
// Name:        wx/mousemanager.h
// Purpose:     wxMouseEventsManager class declaration
// Author:      Vadim Zeitlin
// Created:     2009-04-20
// Copyright:   (c) 2009 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MOUSEMANAGER_H_
#define _WX_MOUSEMANAGER_H_

#include "wx/event.h"

// ----------------------------------------------------------------------------
// wxMouseEventsManager
// ----------------------------------------------------------------------------

/*
    This class handles mouse events and synthesizes high-level notifications
    such as clicks and drag events from low level mouse button presses and
    mouse movement events. It is useful because handling the mouse events is
    less obvious than might seem at a first glance: for example, clicks on an
    object should only be generated if the mouse was both pressed and released
    over it and not just released (so it requires storing the previous state)
    and dragging shouldn't start before the mouse moves away far enough.

    This class encapsulates all these dull details for controls containing
    multiple items which can be identified by a positive integer index and you
    just need to implement its pure virtual functions to use it.
 */
class WXDLLIMPEXP_CORE wxMouseEventsManager : public wxEvtHandler
{
public:
    // a mouse event manager is always associated with a window and must be
    // deleted by the window when it is destroyed so if it is created using the
    // default ctor Create() must be called later
    wxMouseEventsManager() { Init(); }
    wxMouseEventsManager(wxWindow *win) { Init(); Create(win); }
    bool Create(wxWindow *win);

    virtual ~wxMouseEventsManager();

protected:
    // called to find the item at the given position: return wxNOT_FOUND (-1)
    // if there is no item here
    virtual int MouseHitTest(const wxPoint& pos) = 0;

    // called when the user clicked (i.e. pressed and released mouse over the
    // same item), should normally generate a notification about this click and
    // return true if it was handled or false otherwise, determining whether
    // the original mouse event is skipped or not
    virtual bool MouseClicked(int item) = 0;

    // called to start dragging the given item, should generate the appropriate
    // BEGIN_DRAG event and return false if dragging this item was forbidden
    virtual bool MouseDragBegin(int item, const wxPoint& pos) = 0;

    // called while the item is being dragged, should normally update the
    // feedback on screen (usually using wxOverlay)
    virtual void MouseDragging(int item, const wxPoint& pos) = 0;

    // called when the mouse is released after dragging the item
    virtual void MouseDragEnd(int item, const wxPoint& pos) = 0;

    // called when mouse capture is lost while dragging the item, should remove
    // the visual feedback drawn by MouseDragging()
    virtual void MouseDragCancelled(int item) = 0;


    // you don't need to override those but you will want to do if it your
    // control renders pressed items differently

    // called when the item is becomes pressed, can be used to change its
    // appearance
    virtual void MouseClickBegin(int WXUNUSED(item)) { }

    // called if the mouse capture was lost while the item was pressed, can be
    // used to restore the default item appearance if it was changed in
    // MouseClickBegin()
    virtual void MouseClickCancelled(int WXUNUSED(item)) { }

private:
    /*
        Here is a small diagram explaining the switches between different
        states:


                /---------->NORMAL<--------------- Drag end
               /     /   /    |                      event
              /     /    |    |                        ^
             /     /     |    |                        |
           Click  /    N |    | mouse                  | mouse up
           event /       |    | down                   |
             |  /        |    |                     DRAGGING
             | /         |    |                        ^
            Y|/ N        \    v                        |Y
      +-------------+     +--------+           N +-----------+
      |On same item?|     |On item?|  -----------|Begin drag?|
      +-------------+     +--------+ /           +-----------+
             ^                |     /                  ^
             |                |    /                   |
              \      mouse    |   /   mouse moved      |
                \     up      v  v     far enough     /
                  \--------PRESSED-------------------/


        There are also transitions from PRESSED and DRAGGING to NORMAL in case
        the mouse capture is lost or Escape key is pressed which are not shown.
     */
    enum State
    {
        State_Normal,   // initial, default state
        State_Pressed,  // mouse was pressed over an item
        State_Dragging  // the item is being dragged
    };

    // common part of both ctors
    void Init();

    // various event handlers
    void OnCaptureLost(wxMouseCaptureLostEvent& event);
    void OnLeftDown(wxMouseEvent& event);
    void OnLeftUp(wxMouseEvent& event);
    void OnMove(wxMouseEvent& event);


    // the associated window, never NULL except between the calls to the
    // default ctor and Create()
    wxWindow *m_win;

    // the current state
    State m_state;

    // the details of the operation currently in progress, only valid if
    // m_state is not normal

    // the item being pressed or dragged (always valid, i.e. != wxNOT_FOUND if
    // m_state != State_Normal)
    int m_item;

    // the position of the last mouse event of interest: either mouse press in
    // State_Pressed or last movement event in State_Dragging
    wxPoint m_posLast;


    wxDECLARE_EVENT_TABLE();

    wxDECLARE_NO_COPY_CLASS(wxMouseEventsManager);
};

#endif // _WX_MOUSEMANAGER_H_

