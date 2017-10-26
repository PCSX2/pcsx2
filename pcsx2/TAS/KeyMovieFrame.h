#pragma once
#include <wx/wx.h>
#include <wx/filepicker.h>

/*
 * The Dialog to pop-up when recording a new movie
*/
class KeyMovieFrame : public wxDialog
{
public:
	KeyMovieFrame(wxWindow *parent);

	wxString getFile() const;
	wxString getAuthor() const;
	int getFrom() const;
	
private:
	wxStaticText *m_fileLabel;
	wxFilePickerCtrl *m_filePicker;
	wxStaticText *m_authorLabel;
	wxTextCtrl *m_author;
	wxStaticText *m_fromLabel;
	wxChoice *m_fromChoice;
};

