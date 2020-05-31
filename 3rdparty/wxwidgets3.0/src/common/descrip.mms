#*****************************************************************************
#                                                                            *
# Make file for VMS                                                          *
# Author : J.Jansen (joukj@hrem.nano.tudelft.nl)                             *
# Date : 19 September 2013                                                   *
#                                                                            *
#*****************************************************************************
.first
	define wx [--.include.wx]

.ifdef __WXMOTIF__
CXX_DEFINE = /define=(__WXMOTIF__=1)/name=(as_is,short)\
	   /assume=(nostdnew,noglobal_array_new)/incl=[-.regex]
CC_DEFINE = /define=(__WXMOTIF__=1)/name=(as_is,short)/incl=[-.regex]
.else
.ifdef __WXGTK__
CXX_DEFINE = /define=(__WXGTK__=1)/float=ieee/name=(as_is,short)/ieee=denorm\
	   /assume=(nostdnew,noglobal_array_new)/incl=[-.regex]
CC_DEFINE = /define=(__WXGTK__=1)/float=ieee/name=(as_is,short)/ieee=denorm\
	/incl=[-.regex]
.else
.ifdef __WXGTK2__
CXX_DEFINE = /define=(__WXGTK__=1,VMS_GTK2=1)/float=ieee/name=(as_is,short)/ieee=denorm\
	   /assume=(nostdnew,noglobal_array_new)/incl=[-.regex]
CC_DEFINE = /define=(__WXGTK__=1,VMS_GTK2=1)/float=ieee/name=(as_is,short)\
	/ieee=denorm/incl=[-.regex]
.else
.ifdef __WXX11__
CXX_DEFINE = /define=(__WXX11__=1,__WXUNIVERSAL__==1)/float=ieee\
	/name=(as_is,short)/assume=(nostdnew,noglobal_array_new)/incl=[-.regex]
CC_DEFINE = /define=(__WXX11__=1,__WXUNIVERSAL__==1)/float=ieee\
	/name=(as_is,short)/incl=[-.regex]
.else
CXX_DEFINE =
CC_DEFINE =
.endif
.endif
.endif
.endif

YACC=bison/yacc

SED=gsed

LEX=flex

.suffixes : .cpp

.cpp.obj :
	cxx $(CXXFLAGS)$(CXX_DEFINE) $(MMS$TARGET_NAME).cpp
.c.obj :
	cc $(CFLAGS)$(CC_DEFINE) $(MMS$TARGET_NAME).c

OBJECTS = \
		accelcmn.obj,\
		anidecod.obj,\
		animatecmn.obj,\
		appbase.obj,\
		appcmn.obj,\
		arrstr.obj,\
		artprov.obj,\
		artstd.obj,\
		base64.obj,\
		bmpbase.obj,\
		btncmn.obj,\
		bmpcboxcmn.obj,\
		bookctrl.obj,\
		calctrlcmn.obj,\
		choiccmn.obj,\
		clipcmn.obj,\
		clntdata.obj,\
		cmdline.obj,\
		cmdproc.obj,\
		cmndata.obj,\
		config.obj,\
		containr.obj,\
		convauto.obj,\
		colourcmn.obj,\
		cshelp.obj,\
		ctrlcmn.obj,\
		ctrlsub.obj,\
		datetime.obj,\
		datstrm.obj,\
		dcbase.obj,\
		dcbufcmn.obj,\
		dircmn.obj,\
		dlgcmn.obj,\
		dobjcmn.obj,\
		docmdi.obj,\
		docview.obj,\
		dpycmn.obj,\
		dynarray.obj,\
		dynlib.obj,\
		encconv.obj,\
		event.obj,\
		evtloopcmn.obj,\
		extended.obj,\
		fddlgcmn.obj,\
		ffile.obj,\
		file.obj,\
		fileback.obj,\
		fileconf.obj,\
		filename.obj,\
		filefn.obj,\
		filesys.obj,\
		filectrlcmn.obj,\
		fldlgcmn.obj,\
		fmapbase.obj,\
		fontcmn.obj,\
		fontenumcmn.obj,\
		fontmap.obj,\
		framecmn.obj

OBJECTS1=fs_inet.obj,\
		ftp.obj,\
		gaugecmn.obj,\
		gbsizer.obj,\
		gdicmn.obj,\
		gifdecod.obj,\
		hash.obj,\
		hashmap.obj,\
		helpbase.obj,\
		http.obj,\
		hyperlnkcmn.obj,\
		iconbndl.obj,\
		init.obj,\
		imagall.obj,\
		imagbmp.obj,\
		image.obj,\
		imagfill.obj,\
		imaggif.obj,\
		imagiff.obj,\
		imagjpeg.obj,\
		imagpcx.obj,\
		imagpng.obj,\
		imagpnm.obj,\
		imagtga.obj,\
		imagtiff.obj,\
		imagxpm.obj,\
		intl.obj,\
		ipcbase.obj,\
		layout.obj,\
		lboxcmn.obj,\
		list.obj,\
		log.obj,\
		longlong.obj,\
		memory.obj,\
		menucmn.obj,\
		mimecmn.obj,\
		module.obj,\
		msgout.obj,\
		mstream.obj,\
		nbkbase.obj,\
		object.obj,\
		paper.obj,\
		platinfo.obj,\
		popupcmn.obj,\
		prntbase.obj,\
		process.obj,\
		protocol.obj,\
		quantize.obj,\
		radiocmn.obj,\
		rendcmn.obj,\
		sckaddr.obj,\
		sckfile.obj,\
		sckipc.obj,\
		sckstrm.obj,\
		sizer.obj,\
		socket.obj,\
		settcmn.obj,\
		statbar.obj,\
		stattextcmn.obj,\
		stdpbase.obj,\
		stockitem.obj,\
		stopwatch.obj,\
		strconv.obj,\
		stream.obj,\
		string.obj,\
		stringimpl.obj,\
		strvararg.obj,\
		sysopt.obj

OBJECTS2=tbarbase.obj,srchcmn.obj,\
		textbuf.obj,\
		textcmn.obj,\
		textfile.obj,\
		textentrycmn.obj,\
		timercmn.obj,\
		timerimpl.obj,\
		tokenzr.obj,\
		toplvcmn.obj,\
		treebase.obj,\
		txtstrm.obj,\
		url.obj,\
		utilscmn.obj,\
		rgncmn.obj,\
		unichar.obj,\
		uri.obj,\
		valgen.obj,\
		validate.obj,\
		valtext.obj,\
		variant.obj,\
		wfstream.obj,\
		wincmn.obj,\
		wxcrt.obj,\
		xpmdecod.obj,\
		zipstrm.obj,\
		zstream.obj,\
		clrpickercmn.obj,\
		filepickercmn.obj,\
		fontpickercmn.obj,\
		pickerbase.obj

OBJECTS3=listctrlcmn.obj,socketiohandler.obj,fdiodispatcher.obj,\
		selectdispatcher.obj,overlaycmn.obj,windowid.obj,sstream.obj,\
		wrapsizer.obj,headerctrlcmn.obj,headercolcmn.obj,\
		rearrangectrl.obj,spinctrlcmn.obj,datetimefmt.obj,xlocale.obj,\
		regex.obj,any.obj,archive.obj,fs_arc.obj,arcall.obj,\
		arcfind.obj,tarstrm.obj,datavcmn.obj,debugrpt.obj,\
		translation.obj,languageinfo.obj,filehistorycmn.obj,\
		stdstream.obj,uiactioncmn.obj,arttango.obj,mediactrlcmn.obj,\
		panelcmn.obj,checkboxcmn.obj,statboxcmn.obj,slidercmn.obj,\
		statlinecmn.obj,radiobtncmn.obj,bmpbtncmn.obj,checklstcmn.obj,\
		statbmpcmn.obj,dirctrlcmn.obj,gridcmn.obj,odcombocmn.obj,\
		spinbtncmn.obj,scrolbarcmn.obj,colourdata.obj,fontdata.obj,\
		valnum.obj,numformatter.obj,markupparser.obj,\
		affinematrix2d.obj,richtooltipcmn.obj,persist.obj,time.obj,\
		textmeasurecmn.obj,modalhook.obj,threadinfo.obj

OBJECTS_MOTIF=radiocmn.obj,combocmn.obj

OBJECTS_X11=accesscmn.obj,dndcmn.obj,dpycmn.obj,dseldlg.obj,\
	dynload.obj,effects.obj,fddlgcmn.obj,fs_mem.obj,\
	gbsizer.obj,geometry.obj,matrix.obj,radiocmn.obj,\
	taskbarcmn.obj,xti.obj,xtistrm.obj,xtixml.obj,\
	combocmn.obj


OBJECTS_GTK2=fontutilcmn.obj,cairo.obj

SOURCES = \
		accelcmn.cpp,\
		anidecod.cpp,\
		animatecmn.cpp,\
		any.cpp,\
		appbase.cpp,\
		appcmn.cpp,\
		arrstr.cpp,\
		artprov.cpp,\
		artstd.cpp,\
		base64.cpp,\
		bmpbase.cpp,\
		btncmn.cpp,\
		bmpcboxcmn.cpp,\
		bookctrl.cpp,\
		calctrlcmn.cpp,\
		cairo.cpp,\
		choiccmn.cpp,\
		clipcmn.cpp,\
		clntdata.cpp,\
		cmdline.cpp,\
		cmdproc.cpp,\
		cmndata.cpp,\
		config.cpp,\
		containr.cpp,\
		convauto.cpp,\
		colourcmn.cpp,\
		cshelp.cpp,\
		ctrlcmn.cpp,\
		ctrlsub.cpp,\
		datetime.cpp,\
		datstrm.cpp,\
		dcbase.cpp,\
		dcbufcmn.cpp,\
		dircmn.cpp,\
		dlgcmn.cpp,\
		dobjcmn.cpp,\
		docmdi.cpp,\
		docview.cpp,\
		dpycmn.cpp,\
		dynarray.cpp,\
		dynlib.cpp,\
		encconv.cpp,\
		event.cpp,\
		evtloopcmn.cpp,\
		extended.c,\
		ffile.cpp,\
		fddlgcmn.cpp,\
		fdiodispatcher.cpp,\
		file.cpp,\
		fileback.cpp,\
		fileconf.cpp,\
		filename.cpp,\
		filefn.cpp,\
		filesys.cpp,\
		filectrlcmn.cpp,\
		fldlgcmn.cpp,\
		fmapbase.cpp,\
		fontcmn.cpp,\
		fontenumcmn.cpp,\
		fontmap.cpp,\
		fontutilcmn.cpp,\
		framecmn.cpp,\
		fs_inet.cpp,\
		ftp.cpp,\
		gaugecmn.cpp,\
		gbsizer.cpp,\
		gdicmn.cpp,\
		gifdecod.cpp,\
		socketiohandler.cpp,\
		hash.cpp,\
		hashmap.cpp,\
		helpbase.cpp,\
		http.cpp,\
		hyperlnkcmn.cpp,\
		iconbndl.cpp,\
		init.cpp,\
		imagall.cpp,\
		imagbmp.cpp,\
		image.cpp,\
		imagfill.cpp,\
		imaggif.cpp,\
		imagiff.cpp,\
		imagjpeg.cpp,\
		imagpcx.cpp,\
		imagpng.cpp,\
		imagpnm.cpp,\
		imagtga.cpp,\
		imagtiff.cpp,\
		imagxpm.cpp,\
		intl.cpp,\
		ipcbase.cpp,\
		layout.cpp,\
		lboxcmn.cpp,\
		list.cpp,\
		listctrlcmn.cpp,\
		log.cpp,\
		longlong.cpp,\
		memory.cpp,\
		menucmn.cpp,\
		mimecmn.cpp,\
		module.cpp,\
		msgout.cpp,\
		mstream.cpp,\
		nbkbase.cpp,\
		object.cpp,\
		overlaycmn.cpp,\
		paper.cpp,\
		platinfo.cpp,\
		popupcmn.cpp,\
		prntbase.cpp,\
		process.cpp,\
		protocol.cpp,\
		quantize.cpp,\
		radiocmn.cpp,\
		rendcmn.cpp,\
		rgncmn.cpp,\
		sckaddr.cpp,\
		sckfile.cpp,\
		sckipc.cpp,\
		sckstrm.cpp,\
		sizer.cpp,\
		socket.cpp,\
		selectdispatcher.cpp,\
		settcmn.cpp,\
		sstream.cpp,\
		statbar.cpp,\
		stattextcmn.cpp,\
		stdpbase.cpp,\
		stockitem.cpp,\
		stopwatch.cpp,\
		srchcmn.cpp,\
		strconv.cpp,\
		stream.cpp,\
		strvararg.cpp,\
		sysopt.cpp,\
		string.cpp,\
		stringimpl.cpp,\
		tbarbase.cpp,\
		textbuf.cpp,\
		textcmn.cpp,\
		textfile.cpp,\
		textentrycmn.cpp,\
		timercmn.cpp,\
		timerimpl.cpp,\
		tokenzr.cpp,\
		toplvcmn.cpp,\
		treebase.cpp,\
		txtstrm.cpp,\
		unichar.cpp,\
		url.cpp,\
		utilscmn.cpp,\
		valgen.cpp,\
		validate.cpp,\
		valtext.cpp,\
		variant.cpp,\
		wfstream.cpp,\
		wincmn.cpp,\
		wxcrt.cpp,\
		xpmdecod.cpp,\
		zipstrm.cpp,\
		zstream.cpp,\
		clrpickercmn.cpp,\
		filepickercmn.cpp,\
		fontpickercmn.cpp,\
		pickerbase.cpp,\
		accesscmn.cpp,\
		dndcmn.cpp,\
		dpycmn.cpp,\
		dseldlg.cpp,\
		dynload.cpp,\
		effects.cpp,\
		fddlgcmn.cpp,\
		fs_mem.cpp,\
		gbsizer.cpp,\
		geometry.cpp,\
		matrix.cpp,\
		radiocmn.cpp,\
		regex.cpp,\
		taskbarcmn.cpp,\
		uri.cpp,\
		xti.cpp,\
		xtistrm.cpp,\
		xtixml.cpp,\
		wrapsizer.cpp,archive.cpp,fs_arc.cpp,arcall.cpp,arcfind.cpp,\
		tarstrm.cpp,datavcmn.cpp,debugrpt.cpp,uiactioncmn.cpp,\
		arttango.cpp,mediactrlcmn.cpp,panelcmn.cpp,checkboxcmn.cpp,\
		statboxcmn.cpp,slidercmn.cpp,statlinecmn.cpp,radiobtncmn.cpp,\
		bmpbtncmn.cpp,checklstcmn.cpp,statbmpcmn.cpp,dirctrlcmn.cpp,\
		gridcmn.cpp,odcombocmn.cpp,spinbtncmn.cpp,scrolbarcmn.cpp,\
		colourdata.cpp,fontdata.cpp affinematrix2d.cpp\
		richtooltipcmn.cpp persist.cpp time.cpp textmeasurecmn.cpp \
		modalhook.cpp

all : $(SOURCES)
	$(MMS)$(MMSQUALIFIERS) $(OBJECTS)
	$(MMS)$(MMSQUALIFIERS) $(OBJECTS1)
	$(MMS)$(MMSQUALIFIERS) $(OBJECTS2)
	$(MMS)$(MMSQUALIFIERS) $(OBJECTS3)
.ifdef __WXMOTIF__
	$(MMS)$(MMSQUALIFIERS) $(OBJECTS_MOTIF)
	library [--.lib]libwx_motif.olb $(OBJECTS)
	library [--.lib]libwx_motif.olb $(OBJECTS1)
	library [--.lib]libwx_motif.olb $(OBJECTS2)
	library [--.lib]libwx_motif.olb $(OBJECTS3)
	library [--.lib]libwx_motif.olb $(OBJECTS_MOTIF)
.else
.ifdef __WXGTK__
	$(MMS)$(MMSQUALIFIERS) $(OBJECTS_X11)
	library [--.lib]libwx_gtk.olb $(OBJECTS)
	library [--.lib]libwx_gtk.olb $(OBJECTS1)
	library [--.lib]libwx_gtk.olb $(OBJECTS2)
	library [--.lib]libwx_gtk.olb $(OBJECTS3)
	library [--.lib]libwx_gtk.olb $(OBJECTS_X11)
.else
.ifdef __WXGTK2__
	$(MMS)$(MMSQUALIFIERS) $(OBJECTS_X11)
	$(MMS)$(MMSQUALIFIERS) $(OBJECTS_GTK2)
	library [--.lib]libwx_gtk2.olb $(OBJECTS)
	library [--.lib]libwx_gtk2.olb $(OBJECTS1)
	library [--.lib]libwx_gtk2.olb $(OBJECTS2)
	library [--.lib]libwx_gtk2.olb $(OBJECTS3)
	library [--.lib]libwx_gtk2.olb $(OBJECTS_X11)
	library [--.lib]libwx_gtk2.olb $(OBJECTS_GTK2)
.else
.ifdef __WXX11__
	$(MMS)$(MMSQUALIFIERS) $(OBJECTS_X11)
	library [--.lib]libwx_x11_univ.olb $(OBJECTS)
	library [--.lib]libwx_x11_univ.olb $(OBJECTS1)
	library [--.lib]libwx_x11_univ.olb $(OBJECTS2)
	library [--.lib]libwx_x11_univ.olb $(OBJECTS3)
	library [--.lib]libwx_x11_univ.olb $(OBJECTS_X11)
.endif
.endif
.endif
.endif

$(OBJECTS) : [--.include.wx]setup.h
$(OBJECTS1) : [--.include.wx]setup.h
$(OBJECTS2) : [--.include.wx]setup.h
$(OBJECTS3) : [--.include.wx]setup.h
$(OBJECTS_X11) : [--.include.wx]setup.h
$(OBJECTS_GTK2) : [--.include.wx]setup.h
$(OBJECTS_MOTIF) : [--.include.wx]setup.h

accelcmn.obj : accelcmn.cpp
anidecod.obj : anidecod.cpp
animatecmn.obj : animatecmn.cpp
any.obj : any.cpp
appbase.obj : appbase.cpp
appcmn.obj : appcmn.cpp
arrstr.obj : arrstr.cpp
artprov.obj : artprov.cpp
artstd.obj : artstd.cpp
base64.obj : base64.cpp
bmpbase.obj : bmpbase.cpp
btncmn.obj : btncmn.cpp
bmpcboxcmn.obj : bmpcboxcmn.cpp
bookctrl.obj : bookctrl.cpp
choiccmn.obj : choiccmn.cpp
clipcmn.obj : clipcmn.cpp
clntdata.obj : clntdata.cpp
cmdline.obj : cmdline.cpp
cmdproc.obj : cmdproc.cpp
cmndata.obj : cmndata.cpp
config.obj : config.cpp
containr.obj : containr.cpp
convauto.obj : convauto.cpp
colourcmn.obj : colourcmn.cpp
cshelp.obj : cshelp.cpp
ctrlcmn.obj : ctrlcmn.cpp
ctrlsub.obj : ctrlsub.cpp
datetime.obj : datetime.cpp
datstrm.obj : datstrm.cpp
dcbase.obj : dcbase.cpp
dcbufcmn.obj : dcbufcmn.cpp
dircmn.obj : dircmn.cpp
dlgcmn.obj : dlgcmn.cpp
dobjcmn.obj : dobjcmn.cpp
docmdi.obj : docmdi.cpp
docview.obj : docview.cpp
dynarray.obj : dynarray.cpp
dynlib.obj : dynlib.cpp
encconv.obj : encconv.cpp
event.obj : event.cpp
evtloopcmn.obj : evtloopcmn.cpp
extended.obj : extended.c
ffile.obj : ffile.cpp
fddlgcmn.obj : fddlgcmn.cpp
fdiodispatcher.obj : fdiodispatcher.cpp
file.obj : file.cpp
fileback.obj : fileback.cpp
fileconf.obj : fileconf.cpp
filefn.obj : filefn.cpp
filename.obj : filename.cpp
filesys.obj : filesys.cpp
fldlgcmn.obj : fldlgcmn.cpp
fmapbase.obj : fmapbase.cpp
fontcmn.obj : fontcmn.cpp
fontenumcmn.obj : fontenumcmn.cpp
fontmap.obj : fontmap.cpp
fontutilcmn.obj : fontutilcmn.cpp
framecmn.obj : framecmn.cpp
fs_inet.obj : fs_inet.cpp
ftp.obj : ftp.cpp
gaugecmn.obj : gaugecmn.cpp
gbsizer.obj : gbsizer.cpp
gdicmn.obj : gdicmn.cpp
gifdecod.obj : gifdecod.cpp
socketiohandler.obj : socketiohandler.cpp
hash.obj : hash.cpp
hashmap.obj : hashmap.cpp
helpbase.obj : helpbase.cpp
http.obj : http.cpp
	cxx$(CXXFLAGS)$(CXX_DEFINE)/warn=disable=(UNSCOMZER)/obj=http.obj \
	http.cpp
hyperlnkcmn.obj : hyperlnkcmn.cpp
iconbndl.obj : iconbndl.cpp
init.obj : init.cpp
imagall.obj : imagall.cpp
imagbmp.obj : imagbmp.cpp
image.obj : image.cpp
imagfill.obj : imagfill.cpp
imaggif.obj : imaggif.cpp
imagiff.obj : imagiff.cpp
imagjpeg.obj : imagjpeg.cpp
imagpcx.obj : imagpcx.cpp
imagpng.obj : imagpng.cpp
imagpnm.obj : imagpnm.cpp
imagtga.obj : imagtga.cpp
imagtiff.obj : imagtiff.cpp
imagxpm.obj : imagxpm.cpp
intl.obj : intl.cpp
ipcbase.obj : ipcbase.cpp
layout.obj : layout.cpp
lboxcmn.obj : lboxcmn.cpp
list.obj : list.cpp
log.obj : log.cpp
longlong.obj : longlong.cpp
memory.obj : memory.cpp
menucmn.obj : menucmn.cpp
mimecmn.obj : mimecmn.cpp
module.obj : module.cpp
msgout.obj : msgout.cpp
mstream.obj : mstream.cpp
nbkbase.obj : nbkbase.cpp
object.obj : object.cpp
paper.obj : paper.cpp
platinfo.obj : platinfo.cpp
popupcmn.obj : popupcmn.cpp
prntbase.obj : prntbase.cpp
process.obj : process.cpp
	cxx $(CXXFLAGS)$(CXX_DEFINE)/warn=disable=(UNSCOMZER) process.cpp
protocol.obj : protocol.cpp
quantize.obj : quantize.cpp
radiocmn.obj : radiocmn.cpp
rendcmn.obj : rendcmn.cpp
rgncmn.obj : rgncmn.cpp
sckaddr.obj : sckaddr.cpp
sckfile.obj : sckfile.cpp
sckipc.obj : sckipc.cpp
sckstrm.obj : sckstrm.cpp
selectdispatcher.obj : selectdispatcher.cpp
sizer.obj : sizer.cpp
socket.obj : socket.cpp
settcmn.obj : settcmn.cpp
statbar.obj : statbar.cpp
stattextcmn.obj : stattextcmn.cpp
stdpbase.obj : stdpbase.cpp
stockitem.obj : stockitem.cpp
stopwatch.obj : stopwatch.cpp
strconv.obj : strconv.cpp
stream.obj : stream.cpp
strvararg.obj : strvararg.cpp
sysopt.obj : sysopt.cpp
string.obj : string.cpp
stringimpl.obj : stringimpl.cpp
tbarbase.obj : tbarbase.cpp
textbuf.obj : textbuf.cpp
textcmn.obj : textcmn.cpp
textfile.obj : textfile.cpp
timercmn.obj : timercmn.cpp
timerimpl.obj : timerimpl.cpp
tokenzr.obj : tokenzr.cpp
toplvcmn.obj : toplvcmn.cpp
treebase.obj : treebase.cpp
txtstrm.obj : txtstrm.cpp
unichar.obj : unichar.cpp
url.obj : url.cpp
utilscmn.obj : utilscmn.cpp
valgen.obj : valgen.cpp
validate.obj : validate.cpp
valtext.obj : valtext.cpp
variant.obj : variant.cpp
wfstream.obj : wfstream.cpp
wincmn.obj : wincmn.cpp
wxcrt.obj : wxcrt.cpp
xpmdecod.obj : xpmdecod.cpp
zipstrm.obj : zipstrm.cpp
zstream.obj : zstream.cpp
accesscmn.obj : accesscmn.cpp
dndcmn.obj : dndcmn.cpp
dpycmn.obj : dpycmn.cpp
dseldlg.obj : dseldlg.cpp
dynload.obj : dynload.cpp
effects.obj : effects.cpp
fddlgcmn.obj : fddlgcmn.cpp
fs_mem.obj : fs_mem.cpp
gbsizer.obj : gbsizer.cpp
geometry.obj : geometry.cpp
matrix.obj : matrix.cpp
radiocmn.obj : radiocmn.cpp
regex.obj : regex.cpp
taskbarcmn.obj : taskbarcmn.cpp
xti.obj : xti.cpp
xtistrm.obj : xtistrm.cpp
xtixml.obj : xtixml.cpp
uri.obj : uri.cpp
dpycmn.obj : dpycmn.cpp
combocmn.obj : combocmn.cpp
clrpickercmn.obj : clrpickercmn.cpp
filepickercmn.obj : filepickercmn.cpp
fontpickercmn.obj : fontpickercmn.cpp
pickerbase.obj : pickerbase.cpp
listctrlcmn.obj : listctrlcmn.cpp
srchcmn.obj : srchcmn.cpp
textentrycmn.obj : textentrycmn.cpp
filectrlcmn.obj : filectrlcmn.cpp
cairo.obj : cairo.cpp
	cxx$(CXXFLAGS)$(CXX_DEFINE)/obj=cairo.obj cairo.cpp
overlaycmn.obj : overlaycmn.cpp
windowid.obj : windowid.cpp
calctrlcmn.obj : calctrlcmn.cpp
sstream.obj : sstream.cpp
wrapsizer.obj : wrapsizer.cpp
headerctrlcmn.obj : headerctrlcmn.cpp
headercolcmn.obj : headercolcmn.cpp
rearrangectrl.obj : rearrangectrl.cpp
spinctrlcmn.obj : spinctrlcmn.cpp
datetimefmt.obj : datetimefmt.cpp
xlocale.obj : xlocale.cpp
archive.obj : archive.cpp
fs_arc.obj : fs_arc.cpp
arcall.obj : arcall.cpp
arcfind.obj : arcfind.cpp
tarstrm.obj : tarstrm.cpp
datavcmn.obj : datavcmn.cpp
debugrpt.obj : debugrpt.cpp
translation.obj : translation.cpp
languageinfo.obj : languageinfo.cpp
filehistorycmn.obj : filehistorycmn.cpp
stdstream.obj : stdstream.cpp
uiactioncmn.obj : uiactioncmn.cpp
arttango.obj : arttango.cpp
mediactrlcmn.obj : mediactrlcmn.cpp
panelcmn.obj : panelcmn.cpp
checkboxcmn.obj : checkboxcmn.cpp
statboxcmn.obj : statboxcmn.cpp
slidercmn.obj : slidercmn.cpp
statlinecmn.obj : statlinecmn.cpp
radiobtncmn.obj : radiobtncmn.cpp
bmpbtncmn.obj : bmpbtncmn.cpp
checklstcmn.obj : checklstcmn.cpp
statbmpcmn.obj : statbmpcmn.cpp
dirctrlcmn.obj : dirctrlcmn.cpp
gridcmn.obj : gridcmn.cpp
odcombocmn.obj : odcombocmn.cpp
spinbtncmn.obj : spinbtncmn.cpp
scrolbarcmn.obj : scrolbarcmn.cpp
colourdata.obj : colourdata.cpp
fontdata.obj : fontdata.cpp
valnum.obj : valnum.cpp
numformatter.obj : numformatter.cpp
markupparser.obj : markupparser.cpp
affinematrix2d.obj : affinematrix2d.cpp
richtooltipcmn.obj : richtooltipcmn.cpp
persist.obj : persist.cpp
time.obj : time.cpp
textmeasurecmn.obj : textmeasurecmn.cpp
modalhook.obj : modalhook.cpp
threadinfo.obj : threadinfo.cpp
