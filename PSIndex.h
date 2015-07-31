#define INSERT_CMD				1
#define OPEN_CMD				2
#define ABOUT_CMD				3
#define ADDRANGE_CMD			4
#define HELP_CMD				5
#define MARKER_CMD				6
#define EDITBUTTONS_CMD			7
#define SMALLSIZE_CMD			9
#define MEDIUMSIZE_CMD			10
#define LARGESIZE_CMD			11
#define EDITORFONT_CMD			12
#define SAVEBUTTONS_CMD			13
#define LOADBUTTONS_CMD			14
#define AUTOCOMPLETE_ON_CMD		15
#define AUTOCOMPLETE_OFF_CMD	16
#define AUTOCOMPLETE_CASE_CMD	17

#define ITEMLIST_DELETE	WM_USER+98
#define ITEMLIST_UPDATE	WM_USER+99
#define LISTBOX_UPDATE	WM_USER+100
#define ITEMLIST_CLEAR	WM_USER+101
#define CATCHER_OPEN	WM_USER+102
#define SETFOCUS		WM_USER+105
#define EDIT_BUTTONS	WM_USER+107

#define MARKER_WINDOW	1
#define RANGE_WINDOW	2

#define SMALL	1
#define MEDIUM	2
#define LARGE	3

F_ObjHandleT getMarkerPageId FARGS((F_ObjHandleT docId, F_ObjHandleT markerId));
F_ObjHandleT getPageId FARGS((F_ObjHandleT docId, F_ObjHandleT graphicId));
IntT updateAllMarkers FARGS((F_ObjHandleT docId));
IntT updateAllBookMarkers FARGS((F_ObjHandleT docId));
VoidT updateDocName FARGS((F_ObjHandleT docId));
VoidT showDialog() ;

UCharT szWindowTitle[800];
StringListT markerTypes ;
StringListT tempTypes ;
