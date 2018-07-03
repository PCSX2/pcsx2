#pragma once
#include <wx/wx.h>
#include <wx/filepicker.h>

/*
 * The Dialog to pop-up when recording a new movie
*/
class NewRecordingFrame : public wxDialog
{
public:
	NewRecordingFrame(wxWindow *parent);

	wxString getFile() const;
	wxString getAuthor() const;
	int getFrom() const;
	
private:
	wxStaticText *m_fileLabel;
	wxFilePickerCtrl *m_filePicker;
	wxStaticText *m_authorLabel;
	wxTextCtrl *m_authorInput;
	wxStaticText *m_fromLabel;
	wxChoice *m_fromChoice;
	wxButton *m_startRecording;
	wxButton *m_cancelRecording;
};

