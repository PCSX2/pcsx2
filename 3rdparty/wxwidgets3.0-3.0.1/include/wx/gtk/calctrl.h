/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/calctrl.h
// Purpose:     wxGtkCalendarCtrl control
// Author:      Marcin Wojdyr
// Copyright:   (C) 2008 Marcin Wojdyr
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef GTK_CALCTRL_H__
#define GTK_CALCTRL_H__

class WXDLLIMPEXP_ADV wxGtkCalendarCtrl : public wxCalendarCtrlBase
{
public:
    wxGtkCalendarCtrl() {}
    wxGtkCalendarCtrl(wxWindow *parent,
                          wxWindowID id,
                          const wxDateTime& date = wxDefaultDateTime,
                          const wxPoint& pos = wxDefaultPosition,
                          const wxSize& size = wxDefaultSize,
                          long style = wxCAL_SHOW_HOLIDAYS,
                          const wxString& name = wxCalendarNameStr)
    {
        Create(parent, id, date, pos, size, style, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxDateTime& date = wxDefaultDateTime,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxCAL_SHOW_HOLIDAYS,
                const wxString& name = wxCalendarNameStr);

    virtual ~wxGtkCalendarCtrl() {}

    virtual bool SetDate(const wxDateTime& date);
    virtual wxDateTime GetDate() const;

    virtual bool SetDateRange(const wxDateTime& lowerdate = wxDefaultDateTime,
                              const wxDateTime& upperdate = wxDefaultDateTime);
    virtual bool GetDateRange(wxDateTime *lowerdate, wxDateTime *upperdate) const;

    virtual bool EnableMonthChange(bool enable = true);

    virtual void Mark(size_t day, bool mark);

    // implementation
    // --------------

    void GTKGenerateEvent(wxEventType type);

private:
    bool IsInValidRange(const wxDateTime& dt) const;

    // Range of the dates that can be selected by user, either or both may be
    // invalid to indicate that no corresponding restriction is set.
    wxDateTime m_validStart,
               m_validEnd;

    // Last known selected date, may be different from the real selection in
    // the control while a handler for day-selected is running.
    wxDateTime m_selectedDate;

    DECLARE_DYNAMIC_CLASS(wxGtkCalendarCtrl)
    wxDECLARE_NO_COPY_CLASS(wxGtkCalendarCtrl);
};

#endif // GTK_CALCTRL_H__
