/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/carbon/uma.h
// Purpose:     Universal MacOS API
// Author:      Stefan Csomor
// Modified by:
// Created:     03/02/99
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef H_UMA
#define H_UMA

#include "wx/osx/private.h"

#if wxUSE_GUI

// menu manager

MenuRef         UMANewMenu( SInt16 id , const wxString& title , wxFontEncoding encoding) ;
void             UMASetMenuTitle( MenuRef menu , const wxString& title , wxFontEncoding encoding) ;
void             UMAEnableMenuItem( MenuRef inMenu , MenuItemIndex item , bool enable ) ;

void            UMAAppendMenuItem( MenuRef menu , const wxString& title , wxFontEncoding encoding , wxAcceleratorEntry *entry = NULL  ) ;
void            UMAInsertMenuItem( MenuRef menu , const wxString& title , wxFontEncoding encoding , MenuItemIndex item , wxAcceleratorEntry *entry = NULL ) ;
void             UMASetMenuItemShortcut( MenuRef menu , MenuItemIndex item , wxAcceleratorEntry *entry ) ;

void            UMASetMenuItemText(  MenuRef menu,  MenuItemIndex item, const wxString& title , wxFontEncoding encoding ) ;

// Retrieves the Help menu handle. Warning: As a side-effect this functions also
// creates the Help menu if it didn't exist yet.
OSStatus UMAGetHelpMenu(
  MenuRef *        outHelpMenu,
  MenuItemIndex *  outFirstCustomItemIndex);      /* can be NULL */

// Same as UMAGetHelpMenu, but doesn't create the Help menu if UMAGetHelpMenu hasn't been called yet.
OSStatus UMAGetHelpMenuDontCreate(
  MenuRef *        outHelpMenu,
  MenuItemIndex *  outFirstCustomItemIndex);      /* can be NULL */

#endif // wxUSE_GUI

#endif
