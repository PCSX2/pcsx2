#*****************************************************************************
#                                                                            *
# Make file for VMS                                                          *
# Author : J.Jansen (joukj@hrem.nano.tudelft.nl)                             *
# Date : 14 December 2010                                                    *
#                                                                            *
#*****************************************************************************
.first
	define wx [--.include.wx]

.ifdef __WXMOTIF__
CXX_DEFINE = /define=(__WXMOTIF__=1)/name=(as_is,short)\
	   /assume=(nostdnew,noglobal_array_new)
CC_DEFINE = /define=(__WXMOTIF__=1)/name=(as_is,short)
.else
.ifdef __WXGTK__
CXX_DEFINE = /define=(__WXGTK__=1)/float=ieee/name=(as_is,short)/ieee=denorm\
	   /assume=(nostdnew,noglobal_array_new)
CC_DEFINE = /define=(__WXGTK__=1)/float=ieee/name=(as_is,short)/ieee=denorm
.else
.ifdef __WXGTK2__
CXX_DEFINE = /define=(__WXGTK__=1,VMS_GTK2=1)/float=ieee/name=(as_is,short)/ieee=denorm\
	   /assume=(nostdnew,noglobal_array_new)
CC_DEFINE = /define=(__WXGTK__=1,VMS_GTK2=1)/float=ieee/name=(as_is,short)/ieee=denorm
.else
.ifdef __WXX11__
CXX_DEFINE = /define=(__WXX11__=1,__WXUNIVERSAL__==1)/float=ieee\
	/name=(as_is,short)/assume=(nostdnew,noglobal_array_new)
CC_DEFINE = /define=(__WXX11__=1,__WXUNIVERSAL__==1)/float=ieee\
	/name=(as_is,short)
.else
CXX_DEFINE =
CC_DEFINE =
.endif
.endif
.endif
.endif

.suffixes : .cpp

.cpp.obj :
	cxx $(CXXFLAGS)$(CXX_DEFINE) $(MMS$TARGET_NAME).cpp
.c.obj :
	cc $(CFLAGS)$(CC_DEFINE) $(MMS$TARGET_NAME).c

OBJECTS=art_aui.obj,art_internal.obj,art_msw.obj,bar.obj,buttonbar.obj,\
	control_ribbon.obj,gallery.obj,page.obj,panel.obj,\
	toolbar_ribbon.obj

SOURCES=art_aui.cpp art_internal.cpp art_msw.cpp bar.cpp buttonbar.cpp\
	control.cpp gallery.cpp page.cpp panel.cpp toolbar.cpp

all : $(SOURCES)
	$(MMS)$(MMSQUALIFIERS) $(OBJECTS)
.ifdef __WXMOTIF__
	library [--.lib]libwx_motif.olb $(OBJECTS)
.else
.ifdef __WXGTK__
	library [--.lib]libwx_gtk.olb $(OBJECTS)
.else
.ifdef __WXGTK2__
	library [--.lib]libwx_gtk2.olb $(OBJECTS)
.else
.ifdef __WXX11__
	library [--.lib]libwx_x11_univ.olb $(OBJECTS)
.endif
.endif
.endif
.endif

$(OBJECTS) : [--.include.wx]setup.h

art_aui.obj : art_aui.cpp
art_internal.obj : art_internal.cpp
art_msw.obj : art_msw.cpp
bar.obj : bar.cpp
buttonbar.obj : buttonbar.cpp
control_ribbon.obj : control.cpp
	copy control.cpp control_ribbon.cpp
	cxx$(CXXFLAGS)$(CXX_DEFINE) control_ribbon.cpp
	delete control_ribbon.cpp;*
gallery.obj : gallery.cpp
page.obj : page.cpp
panel.obj : panel.cpp
toolbar_ribbon.obj : toolbar.cpp
	copy toolbar.cpp toolbar_ribbon.cpp
	cxx$(CXXFLAGS)$(CXX_DEFINE) toolbar_ribbon.cpp
	delete toolbar_ribbon.cpp;*
