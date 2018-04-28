#include "PrecompiledHeader.h"


#include "app.h"	// Counters.h
#include "Counters.h"	// use"g_FrameCount"

#include "KeyEditor.h"
#include "KeyMovie.h"
#include "MovieControle.h"

#include <string>
#include <wx/joystick.h>

enum {
	ID_MenuAuthor = 1,
	ID_MenuKeyMovieInfo,

	ID_List_KeyFrame,
	ID_Text_Edit,

	ID_Btn_Update,
	ID_Btn_Insert,
	ID_Btn_Delete,
	ID_Btn_Copy,
	ID_Btn_DrawFrame,
	ID_Btn_DrawNowFrame,

	ID_CheckList_NormalKey1,

	ID_Text_AnalogKey1,
	ID_Text_AnalogKey2,
	ID_Text_AnalogKey3,
	ID_Text_AnalogKey4,

};

wxBEGIN_EVENT_TABLE(KeyEditor, wxFrame)
	EVT_CLOSE(KeyEditor::OnClose)
wxEND_EVENT_TABLE()

KeyEditor::KeyEditor(wxWindow * parent)
	: wxFrame(parent, wxID_ANY, L"KeyEditor", wxPoint(437+680,52), wxSize(680,560))
{
	// menu bar
	wxMenu *menuFile = new wxMenu;
	menuFile->Append(ID_MenuAuthor, L"&set author");
	menuFile->Append(ID_MenuKeyMovieInfo, L"&KeyMovieInfo");
	wxMenuBar *menuBar = new wxMenuBar;
	menuBar->Append(menuFile, L"&menu");
	SetMenuBar(menuBar);

	// panel
	int x = 2;
	int y = 2;
	wxPanel *panel = new wxPanel(this, wxID_ANY);

	// listbox
	frameList = new wxListBox(panel, ID_List_KeyFrame, wxPoint(x, y),wxSize(250,460));
	x += 250 + 5;

	// key
	keyTextView = new wxTextCtrl(panel, wxID_ANY, L"", wxPoint(x, y), wxSize(420, 25));
	keyTextView->Disable();
	y += 25;
	new wxButton(panel, ID_Btn_Copy, L"copy", wxPoint(x, y));
	y += 28;
	keyTextEdit = new wxTextCtrl(panel, ID_Text_Edit, L"", wxPoint(x, y), wxSize(420, 25));
	y += 25;
	wxArrayString tmp;
	for (int i = 0; i < PadDataNormalKeysSize; i++) {
		tmp.Add(PadDataNormalKeys[i]);
	}
	keyCheckList1 = new wxCheckListBox(panel, ID_CheckList_NormalKey1, wxPoint(x, y), wxSize(90, 300), tmp);
	x += 90+5;

	// analog
	for (int i = 0; i < PadDataAnalogKeysSize; i++) {
		(new wxTextCtrl(panel, wxID_ANY, PadDataAnalogKeys[i] , wxPoint(x, y), wxSize(100, 28)))->Disable();
		analogKeyText[i] = new wxTextCtrl(panel, (ID_Text_AnalogKey1+i), "", wxPoint(x + 100, y), wxSize(80, 28));
		y += 28;
	}

	// button
	int w = 90;
	new wxButton(panel, ID_Btn_Update, L"update", wxPoint(x,y));
	new wxButton(panel, ID_Btn_Insert, L"insert", wxPoint(x+w, y));
	new wxButton(panel, ID_Btn_Delete, L"delete", wxPoint(x+w*2, y));
	y += 28+20;
	frameTextFoeMove = new wxTextCtrl(panel, wxID_ANY, L"100", wxPoint(x, y), wxSize(80, 25));
	y += 28;
	new wxButton(panel, ID_Btn_DrawFrame, L"draw frame:", wxPoint(x, y));
	new wxButton(panel, ID_Btn_DrawNowFrame, L"draw now frame", wxPoint(x + w, y));

	// status bar
	statusbar = CreateStatusBar();
	statusbar->SetStatusText(L"key editor open");

	// event
	Bind(wxEVT_COMMAND_MENU_SELECTED, &KeyEditor::OnMenuAuthor, this, ID_MenuAuthor);
	Bind(wxEVT_COMMAND_MENU_SELECTED, &KeyEditor::OnMenuKeyMovieInfo, this, ID_MenuKeyMovieInfo);
	// button
	Bind(wxEVT_COMMAND_BUTTON_CLICKED, &KeyEditor::OnBtnUpdate, this, ID_Btn_Update);
	Bind(wxEVT_COMMAND_BUTTON_CLICKED, &KeyEditor::OnBtnInsert, this, ID_Btn_Insert);
	Bind(wxEVT_COMMAND_BUTTON_CLICKED, &KeyEditor::OnBtnDelete, this, ID_Btn_Delete);
	Bind(wxEVT_COMMAND_BUTTON_CLICKED, &KeyEditor::OnBtnCopy, this, ID_Btn_Copy);
	Bind(wxEVT_COMMAND_BUTTON_CLICKED, &KeyEditor::OnBtnDrawFrame, this, ID_Btn_DrawFrame);
	Bind(wxEVT_COMMAND_BUTTON_CLICKED, &KeyEditor::OnBtnDrawNowFrame, this, ID_Btn_DrawNowFrame);

	//list box
	Bind(wxEVT_COMMAND_LISTBOX_SELECTED, &KeyEditor::OnListBox, this, ID_List_KeyFrame);

	//checklist
	Bind(wxEVT_CHECKLISTBOX, &KeyEditor::OnCheckList_NormalKey1, this, ID_CheckList_NormalKey1);

	//text
	Bind(wxEVT_TEXT, &KeyEditor::OnText_Edit, this, ID_Text_Edit);
	Bind(wxEVT_TEXT, &KeyEditor::OnText_Analog1, this, ID_Text_AnalogKey1);
	Bind(wxEVT_TEXT, &KeyEditor::OnText_Analog2, this, ID_Text_AnalogKey2);
	Bind(wxEVT_TEXT, &KeyEditor::OnText_Analog3, this, ID_Text_AnalogKey3);
	Bind(wxEVT_TEXT, &KeyEditor::OnText_Analog4, this, ID_Text_AnalogKey4);
}
void KeyEditor::OnClose(wxCloseEvent& evt)
{
	Hide();
}
void KeyEditor::OnMenuAuthor(wxCommandEvent& event)
{
	wxTextEntryDialog* dlg = new wxTextEntryDialog(NULL, L"input author.");
	if (dlg->ShowModal() != wxID_OK) {
		return;
	}
	g_KeyMovieHeader.setAuthor(dlg->GetValue());
	g_KeyMovieData.writeHeader();
}
void KeyEditor::OnMenuKeyMovieInfo(wxCommandEvent& event)
{
	wxString s = L"";
	s += wxString::Format(L"Ver:%d\n", g_KeyMovieHeader.version);
	s += wxString::Format(L"Author:%s\n", g_KeyMovieHeader.author);
	s += wxString::Format(L"Emu:%s\n", g_KeyMovieHeader.emu);
	s += wxString::Format(L"CD:%s\n", g_KeyMovieHeader.cdrom);
	s += wxString::Format(L"MaxFrame:%d\n", g_KeyMovieData.getMaxFrame());
	s += wxString::Format(L"UndoCount:%d\n", g_KeyMovieData.getUndoCount());

	wxMessageBox(s, L"KeyMovie file header info", wxOK | wxICON_INFORMATION);
}

//-------------------------------
// every frame
//-------------------------------
void KeyEditor::FrameUpdate()
{
	if (g_FrameCount == 0)return;

	wxString pauseMessage = g_MovieControle.getStopFlag() ? L"[pause]" : L"[run]";
	wxString recordMessage = "";
	if (g_KeyMovie.getModeState() == KeyMovie::RECORD) {
		recordMessage = L"[record]";
	}
	else if (g_KeyMovie.getModeState() == KeyMovie::REPLAY) {
		recordMessage = L"[replay]";
	}
	SetTitle(wxString::Format(L"%d / %d ", g_FrameCount, g_KeyMovieData.getMaxFrame())+ pauseMessage+ recordMessage );
}

//-------------------------------
// draw
//-------------------------------
void KeyEditor::DrawKeyFrameList(long selectFrame)
{
	// Decide the number of frames to display
	// �\������t���[������߂�
	int start = selectFrame - 100;
	unsigned int end = selectFrame +100;
	if (start < 0) {
		start = 0;
	}
	frameListStartFrame = start;
	wxString selectKeyStr = "";
	frameList->Clear();
	for (unsigned long i = start; i < end; i++)
	{
		PadData key;
		g_KeyMovieData.getPadData(key, i);
		frameList->Append(wxString::Format("%d [%s]", i, key.serialize()));
		if (selectFrame == i) {
			selectKeyStr = key.serialize();
		}
	}
	// select
	long selectIndex = selectFrame - start;
	if (0 <= selectIndex && selectIndex < (signed)frameList->GetCount())
	{
		frameList->SetSelection(selectIndex);
		keyTextView->ChangeValue(selectKeyStr);
	}
}
void KeyEditor::DrawKeyButtonCheck()
{
	PadData key;
	key.deserialize(keyTextEdit->GetValue());
	auto key1 = key.getNormalKeys(0);
	for (int i = 0; i < PadDataNormalKeysSize;i++) {
		keyCheckList1->Check( i , key1[PadDataNormalKeys[i]]);
	}
	auto key2 = key.getAnalogKeys(0);
	for (int i = 0; i < PadDataAnalogKeysSize; i++) {
		analogKeyText[i]->ChangeValue(wxString::Format(L"%d", key2.at(PadDataAnalogKeys[i])));
	}

}

//----------------------------------------------
// event
// TODO: all of these need to be updated for the key editor window 
//----------------------------------------------
void KeyEditor::OnBtnUpdate(wxCommandEvent& event)
{
	int select = frameList->GetSelection();
	if (select == wxNOT_FOUND)return;
	long frame = select + frameListStartFrame;
	PadData key;
	key.deserialize(keyTextEdit->GetValue());
	if (g_KeyMovieData.UpdatePadData(frame, key))
	{
		frameTextFoeMove->ChangeValue(wxString::Format(L"%d", frame));
		DrawKeyFrameList(frame);
	}
}
void KeyEditor::OnBtnInsert(wxCommandEvent& event)
{
	int select = frameList->GetSelection();
	if (select == wxNOT_FOUND)return;
	long frame = select + frameListStartFrame;
	PadData key;
	key.deserialize(keyTextEdit->GetValue());
	if (g_KeyMovieData.InsertPadData(frame,key))
	{
		frameTextFoeMove->ChangeValue(wxString::Format(L"%d", frame));
		DrawKeyFrameList(frame);
	}

}
void KeyEditor::OnBtnDelete(wxCommandEvent& event)
{
	int select = frameList->GetSelection();
	if (select == wxNOT_FOUND)return;
	long frame = select + frameListStartFrame;
	if (g_KeyMovieData.DeletePadData(frame))
	{
		frameTextFoeMove->ChangeValue(wxString::Format(L"%d",frame));
		DrawKeyFrameList(frame);
	}
}
void KeyEditor::OnBtnCopy(wxCommandEvent& event)
{
	keyTextEdit->ChangeValue(keyTextView->GetValue());
	DrawKeyButtonCheck();
}
void KeyEditor::OnText_Edit(wxCommandEvent& event)
{
	DrawKeyButtonCheck();
}
void KeyEditor::OnListBox(wxCommandEvent& event)
{
	int select = frameList->GetSelection();
	if (select == wxNOT_FOUND)return;
	long frame = select + frameListStartFrame;
	PadData key;
	g_KeyMovieData.getPadData(key,frame);
	keyTextView->ChangeValue(key.serialize());
	frameTextFoeMove->ChangeValue(wxString::Format(L"%d", frame));
}

void KeyEditor::OnBtnDrawFrame(wxCommandEvent& event)
{
	long selectFrame;
	if (frameTextFoeMove->GetValue().ToLong(&selectFrame))
	{
		DrawKeyFrameList(selectFrame);
	}
}
void KeyEditor::OnBtnDrawNowFrame(wxCommandEvent& event)
{
	frameTextFoeMove->ChangeValue(wxString::Format(L"%d", g_FrameCount));
	DrawKeyFrameList(g_FrameCount);
}
void KeyEditor::OnCheckList_NormalKey1(wxCommandEvent& event)
{
	PadData key;
	key.deserialize(keyTextEdit->GetValue());
	if (!key.fExistKey)return;

	auto tmpkey = key.getNormalKeys(0);
	for (unsigned int i = 0; i < keyCheckList1->GetCount(); i++)
	{
		tmpkey[keyCheckList1->GetString(i)] = keyCheckList1->IsChecked(i);
	}
	key.setNormalKeys(0,tmpkey);
	keyTextEdit->ChangeValue(key.serialize());
}
void KeyEditor::_OnText_Analog(int num)
{
	PadData key;
	key.deserialize(keyTextEdit->GetValue());
	if (!key.fExistKey)return;

	auto tmpkey = key.getAnalogKeys(0);

	try {
		tmpkey[PadDataAnalogKeys[num]] = std::stoi( analogKeyText[num]->GetValue().ToStdString(), NULL, 10);
	}
	catch (std::invalid_argument e) {/*none*/ }
	catch (std::out_of_range e) {/*none*/ }

	key.setAnalogKeys(0, tmpkey);
	wxString s = key.serialize();
	keyTextEdit->ChangeValue(s);

}
void KeyEditor::OnText_Analog1(wxCommandEvent& event)
{
	_OnText_Analog(0);
}
void KeyEditor::OnText_Analog2(wxCommandEvent& event)
{
	_OnText_Analog(1);
}
void KeyEditor::OnText_Analog3(wxCommandEvent& event)
{
	_OnText_Analog(2);
}
void KeyEditor::OnText_Analog4(wxCommandEvent& event)
{
	_OnText_Analog(3);
}

