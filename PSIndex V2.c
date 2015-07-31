/************************************************************************/
/*																		*/
/* emDEX																*/
/*																		*/
/* FrameMaker indexing plugin DLL.										*/
/*																		*/
/* 17/09/02		    Version 1.00 -	Creation.							*/
/* 03/06/03		    Version 2.00 -	Added emDEX marker window.			*/
/*																		*/
/************************************************************************/

#include <windows.h>
#include <ddeml.h>
#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <FCNTL.H>

#include "fapi.h"
#include "fdetypes.h"
#include "fstrings.h"
#include "fstrlist.h"
#include "fmemory.h"
#include "futils.h"
#include "fprogs.h"
#include "fcodes.h"
#include "fcharmap.h"
#include "fm_struct.h"

#include "cutils.h"

#include "PSIndex.h"
#include "PSIndexCommon.h"

#define INCLUDE_FMTS 1
#define EXCLUDE_BOX 1
#define RIGHT_ARROW 5
#define INCLUDE_BOX 4
#define ELEMENTTAG_BOX 22
#define LEFT_ARROW 2
#define OK 6
#define ADD 2
#define CANCEL 7
#define INCLUDE_DIALOG 1
#define RANGE_DIALOG 2
#define MARKER_DIALOG 3

#define RANGETEXT	4

#define BOOK	1
#define DOC		2
#define UNKNOWN	3

#define INCLUDE_DLG (StringT) "include"
#define RANGE_DLG (StringT) "range"
#define MARKER_DLG (StringT) "marker"

StringListT getListOfTypes();
StringListT getListOfAllTypes();
F_StringsT  convertToApiList();
StringT getSelectedLabel();
IntT getSelectedLabelIndex();
VoidT setSelectedLabelIndex() ;
VoidT findAndSelectLabel() ;
VoidT findStringAndSelectLabel() ;
IntT getDefaultSelection() ;
VoidT findAndSelectDefaultType() ;
VoidT findAndSelectDefaultElement() ;
NativeIntT fn(ConStringT *s1, ConStringT *s2) ;
IntT doDelete(F_ObjHandleT docId, StringT s1, StringT s2) ;
IntT doFind(F_ObjHandleT docId, StringT s1, StringT s2, INT pageNum, StringT refText) ;
IntT doReplace(F_ObjHandleT docId, StringT findString, StringT replaceString, BoolT isCaseSensitive) ;
VoidT doUpdate(F_ObjHandleT docId) ;
VoidT doCheckSpelling(F_ObjHandleT docId) ;
IntT getExtension(F_ObjHandleT docId) ;
F_ObjHandleT getContainedInBook(StringT docName) ;
VoidT openAllDocs(F_ObjHandleT docId) ;
F_ObjHandleT InsertMarker(F_ObjHandleT docId, F_TextLocT loc, StringT markername);
BoolT isEscapeSequence(StringT text, IntT pos) ;
VoidT getAddText(StringT inText, StringT outText) ;
VoidT removeTagText(StringT inText, StringT outText) ;
IntT noTagCompare(StringT inText1, StringT inText2) ;
VoidT removeFormatting(StringT inText, StringT outText) ;
VoidT removeIndentSpaces(StringT inText, StringT outText) ;
IntT getBlankMarkerText(F_ObjHandleT docId, F_ObjHandleT markerId, StringT tempMarkerName) ;
IntT convertFromRoman(StringT pageString, IntT pageStyle) ;
IntT convertFromAlpha(StringT pageString, IntT pageStyle) ;
VoidT parseReferenceText(F_ObjHandleT docId, StringT pageString, StringT referenceText, StringT outText) ;
VoidT toAlpha(IntT inNum, IntT inNumbering, StringT outString) ;
VoidT toRoman(IntT inNum, IntT inNumbering, StringT outString) ;
VoidT updateReferenceFormat(F_ObjHandleT docId) ;
VoidT openEditorWindow(StringT title) ;
VoidT setEditorText() ;
VoidT setEditorSize(IntT size) ;
VoidT saveButtons() ;
VoidT loadButtons() ;
VoidT loadAutoCompleteText(F_ObjHandleT docId) ;
VoidT addAutoCompleteText(StringT inString) ;

F_ObjHandleT dialogId;
F_ObjHandleT includeDialogId;
F_ObjHandleT activeDocId;
F_ObjHandleT excludeScrollBoxId;
F_ObjHandleT includeScrollBoxId;
F_ObjHandleT rangeListBoxId;
F_ObjHandleT rangeTextId;
StringListT excludeList = NULL;
StringListT rangeList = NULL;
IntT moveItemNum;
StringT moveItemName = NULL;
StringT rangeTypeName = NULL;
UCharT		IncludeTypes[80] [800] ;
BoolT	updateInRealTime = True ;
IntT itemCount, itemLimit ;
StringT rangeText ;
UCharT referenceFormat[800] ;
UCharT helpPath[800] ;
BoolT	g_bUseEditor ;
IntT	g_CurrentEditor = 0 ;
IntT	defaultType = -1 ;
UCharT  strDefaultType[800] ;
UCharT  strElementDefaultType[800] ;
StringT	productName ;
F_ObjHandleT	smallId, mediumId, largeId ;
F_ObjHandleT    editorPopUpId, menuId, bookmenuId;
HFONT hf ;
LOGFONT lf;
UCharT			RegValue[800] ;
UCharT			autoComplete[MAXITEMS*2] [80] ;
IntT			g_autoComplete ;

StringT			g_markerName ;
F_ObjHandleT	g_currentMarkerId = 0 ;

/* DDE Initialization instance ID */
static DWORD ddeInst;

/* DDE String Handles */
static HSZ hszFrameDDE;

FILE *tempHandle ;

typedef struct EDITORBUTTONS {
	UCharT	text[MAXLEN] ;
	UCharT	action[MAXLEN] ;
} EDITORBUTTONS ;

EDITORBUTTONS buttons[10] ;

HDDEDATA CALLBACK DdeCallBack(UINT type, UINT format,
	HCONV conv, HSZ hsz1, HSZ hsz2, HDDEDATA hData,
	DWORD data1, DWORD data2)
{
	F_ObjHandleT docId, mrkrId, prvmrkrId, compId, tmpdocId ;
	IntT i, j, iSave ;
	UCharT tempString[800], numString[800], fileName[800], searchText[800], replaceText[800], option[2] ;
	UCharT szSaveOption[800], refText[800] ;
	StringT docName ;
	IntT fcodes[1], pageNum ;
	StringT buf = NULL, compName = NULL ;
	BoolT	isCaseSensitive = False ;
	ATOM tempAtom ;

	switch (type)
	{
	case XTYP_CONNECT:
		/* Accept connection */
		return (HDDEDATA)TRUE;
		break;
	
	case XTYP_EXECUTE:
		{
			// Log the data received from the catcher.
			DWORD lng;

			buf = (StringT)DdeAccessData(hData, &lng);

			option[0] = buf[0] ;
			option[1] = '\0' ;

			if (option[0] == 'T')
			{
				// Real-Time updating has been switched on or off.
				GetPrivateProfileString("Options", "RealTimeUpdates", "1", szSaveOption, 800, "emDEX.ini") ;

				if (szSaveOption[0] == '1')
					updateInRealTime = TRUE ;
				else
					updateInRealTime = FALSE ;

				return (HDDEDATA)DDE_FACK;
			}

			// Get the filename, removing quotation marks.
			for (i = 1 ; i < F_StrLen(buf) ; i++)
			{
				if ((buf[i] == '@') && (buf[i+1] == '!'))
				{
					break ;
				}
				else
				{
					fileName[i-1] = buf[i] ;
					fileName[i] = '\0' ;
				}
			}

			i+= 2 ;

			// Cope with the single quotation mark displaying strangely.
			for (j = 0 ; i < F_StrLen(buf) ; i++, j++)
			{
				if ((buf[i] == '@') && (buf[i+1] == '$'))
					break ;

				if (buf[i] == '\'')
				{
					tempString[j] = 0xd5 ;
				}
				else
				{
					tempString[j] = buf[i] ;
				}

				tempString[j+1] = '\0' ;

				searchText[j] = buf[i] ;
				searchText[j+1] = '\0' ;
			}

			if (option[0] == 'R')
			{
				// Get the replacement text.
				i = F_StrSubString(searchText, "@!") ;
				iSave = i ;

				F_StrCpy(replaceText, &searchText[i+2]) ;

				i = F_StrSubString(replaceText, "@!") ;

				if (replaceText[i+2] == '1')
					isCaseSensitive = True ;
				else
					isCaseSensitive = False ;

				replaceText[i] = '\0' ;
				searchText[iSave] = '\0' ;

//				F_Printf(NULL, (StringT)"searchText = %s, replaceText = %s, isCase = %d\n", searchText, replaceText, isCaseSensitive);
			}

			if (option[0] == 'P')
			{
//				F_Printf(NULL, (StringT)"searchText = %s\n", searchText);

				// Get the page number and reference text to find.
				i = F_StrSubString(searchText, "@!") ;

				iSave = i ;

				F_StrCpy(tempString, &searchText[i+2]) ;

//				F_Printf(NULL, (StringT)"tempString = %s\n", tempString);

				i = F_StrSubString(tempString, "@!") ;

				F_StrCpyN(numString, tempString, i+1) ;

//				F_Printf(NULL, (StringT)"numString = %s\n", numString);

				pageNum = F_StrAlphaToInt(numString) ;

				F_StrCpy(refText, &tempString[i+2]) ;

				searchText[iSave] = '\0' ;

//				F_Printf(NULL, (StringT)"refText = %s\n", refText);
			}

/*			if (F_StrICmp(&fileName[F_StrLen(fileName)-4], "book") != 0)
			{ */
			// Get the id of the document.
			docId = F_ApiGetId(0, FV_SessionId, FP_FirstOpenDoc);

			while (docId)
			{
				docName = F_ApiGetString(docId, docId, FP_Name) ;

				if (F_StrICmp(docName, fileName) == 0)
				{
					F_Free(docName) ;
					updateDocName(docId) ;
					break ;
				}
				else	
				{	
					F_Free(docName) ;
				}

				docId = F_ApiGetId(FV_SessionId, docId,	FP_NextOpenDocInSession);
			}

//			F_Printf(NULL, (StringT)"docName = %s, fileName = %s\n", docName, fileName);

			if (/*F_StrICmp(docName, fileName) == 0*/docId)
			{
				// Found the doc, so it's not a book.
				F_ApiSetId(FV_SessionId, FV_SessionId, FP_ActiveDoc, docId);
				
				// Make sure that the document isn't minimized
				F_ApiSetInt(docId, docId, FP_IsIconified, 0);
/*				fcodes[0] = KBD_RESTORE ;
				F_ApiFcodes(sizeof(fcodes)/sizeof(IntT), fcodes);*/

				switch (option[0])
				{
					case 'D' :
						doDelete(docId, searchText, tempString) ;

						setEditorText() ;

						// Set the focus back to the catcher window.
						SendMessage(FindWindow(NULL, szWindowTitle), SETFOCUS, (WPARAM)0, (LPARAM)0) ;

						break ;

					case 'F' :
						doFind(docId, searchText, tempString, -1, (StringT)"") ;

						setEditorText() ;

						break ;

					case 'E' :
						// Bring up the markers dialog box.
						GetPrivateProfileString("Options", "UseEditor", "1", szSaveOption, 800, "emDEX.ini") ;
						g_bUseEditor = F_StrAlphaToInt(szSaveOption) ;

						if (g_bUseEditor == FALSE)
						{
							fcodes[0] = KBD_MARKERS ;
							F_ApiFcodes(sizeof(fcodes)/sizeof(IntT), fcodes);
						}
						else
						{
							openEditorWindow("emDEX Marker") ;
						}

						break ;

					case 'R' :
						doReplace(docId, searchText, replaceText, isCaseSensitive) ;

						setEditorText() ;

						SendMessage(FindWindow(NULL, szWindowTitle), ITEMLIST_CLEAR, (WPARAM)0, (LPARAM)0) ;

						if (updateAllMarkers(docId) != -1)
						{
							// Update the listbox
							SendMessage(FindWindow(NULL, szWindowTitle), LISTBOX_UPDATE, (WPARAM)0, (LPARAM)0) ;
						}

						break ;

					case 'P' :
						updateReferenceFormat(docId) ;

						doFind(docId, searchText, tempString, pageNum, refText) ;

						setEditorText() ;

						break ;

					case 'U' :
						doUpdate(docId) ;
						break ;

					default :
						break ;
				}
			}
			else
			{
				// It's a book, so find the marker in the book's open documents.
				// Get the id of the document.
				docId = F_ApiGetId(0, FV_SessionId, FP_FirstOpenBook);

				while (docId)
				{
					docName = F_ApiGetString(docId, docId, FP_Name) ;

					if (F_StrICmp(docName, fileName) == 0)
					{
						F_Free(docName) ;
						updateDocName(docId) ;
						break ;
					}
					else	
					{	
						F_Free(docName) ;
					}

					docId = F_ApiGetId(FV_SessionId, docId,	FP_NextOpenBookInSession);
				}

				if (option[0] == 'U')
				{
					doUpdate(docId) ;
				}
				else
				{
					compId = F_ApiGetId(FV_SessionId, docId, FP_FirstComponentInBook);

					while(compId)
					{
						compName = F_ApiGetString(docId, compId, FP_Name);
	
						tmpdocId = F_ApiGetId(0, FV_SessionId, FP_FirstOpenDoc);

						while (tmpdocId)
						{
							if (F_StrICmp(F_ApiGetString(tmpdocId, tmpdocId, FP_Name), compName) == 0)
								break ;

							tmpdocId = F_ApiGetId(FV_SessionId, tmpdocId, FP_NextOpenDocInSession);
						}

						if (tmpdocId)
						{
							switch (option[0])
							{
								case 'D' :
									if (doDelete(tmpdocId, searchText, tempString) == 0)
									{
										if(compName != NULL)
											F_Free(compName);

										setEditorText() ;

										// Set the focus back to the catcher window.
										SendMessage(FindWindow(NULL, szWindowTitle), SETFOCUS, (WPARAM)0, (LPARAM)0) ;

										return (HDDEDATA)DDE_FACK;
									}
									break ;

								case 'F' :
									if (doFind(tmpdocId, searchText, tempString, -1, (StringT)"") == 0)
									{
										if(compName != NULL)
											F_Free(compName);

										F_ApiSetId(FV_SessionId, FV_SessionId, FP_ActiveDoc, tmpdocId);

										// Make sure that the document isn't minimized
										F_ApiSetInt(tmpdocId, tmpdocId, FP_IsIconified, 0);
/*										fcodes[0] = KBD_RESTORE ;
										F_ApiFcodes(sizeof(fcodes)/sizeof(IntT), fcodes); */

										// Set the focus back to the catcher window.
										SendMessage(FindWindow(NULL, szWindowTitle), SETFOCUS, (WPARAM)0, (LPARAM)0) ;

										setEditorText() ;

										return (HDDEDATA)DDE_FACK;
									}
									break ;

								case 'E' :
									// Bring up the markers dialog box.
									GetPrivateProfileString("Options", "UseEditor", "1", szSaveOption, 800, "emDEX.ini") ;
									g_bUseEditor = F_StrAlphaToInt(szSaveOption) ;

									if (g_bUseEditor == FALSE)
									{
										fcodes[0] = KBD_MARKERS ;
										F_ApiFcodes(sizeof(fcodes)/sizeof(IntT), fcodes);
									}
									else
									{
										openEditorWindow("emDEX Marker") ;
									}

									// Set the focus back to the catcher window.
//									SendMessage(FindWindow(NULL, szWindowTitle), SETFOCUS, (WPARAM)0, (LPARAM)0) ;

									return (HDDEDATA)DDE_FACK;

								case 'R' :
									doReplace(tmpdocId, searchText, replaceText, isCaseSensitive) ;

									setEditorText() ;

									break ;

								case 'P' :
									updateReferenceFormat(tmpdocId) ;

									if (doFind(tmpdocId, searchText, tempString, pageNum, refText) == 0)
									{
										if(compName != NULL)
											F_Free(compName);

										F_ApiSetId(FV_SessionId, FV_SessionId, FP_ActiveDoc, tmpdocId);

										// Make sure that the document isn't minimized
										F_ApiSetInt(tmpdocId, tmpdocId, FP_IsIconified, 0);
/*										fcodes[0] = KBD_RESTORE ;
										F_ApiFcodes(sizeof(fcodes)/sizeof(IntT), fcodes); */

										// Set the focus back to the catcher window.
										SendMessage(FindWindow(NULL, szWindowTitle), SETFOCUS, (WPARAM)0, (LPARAM)0) ;

										setEditorText() ;

										return (HDDEDATA)DDE_FACK;
									}
									break ;

/*								case 'U' :
									doUpdate(docId) ;
									break ; */

								default :
									break ;
							}
						}

						/* get next component in book */
						compId = F_ApiGetId(docId, compId, FP_NextComponentInBook);
					}
				}

				// Update the listbox if replacing marker text.
				if (option[0] == 'R')
				{
					SendMessage(FindWindow(NULL, szWindowTitle), ITEMLIST_CLEAR, (WPARAM)0, (LPARAM)0) ;

					if (updateAllMarkers(docId) != -1)
					{
						// Update the listbox
						SendMessage(FindWindow(NULL, szWindowTitle), LISTBOX_UPDATE, (WPARAM)0, (LPARAM)0) ;
					}
				}
			}

/*			DdeUnaccessData(hData);
			DdeFreeDataHandle(hData);  */

			// Set the focus back to the catcher window.
//			SendMessage(FindWindow(NULL, szWindowTitle), SETFOCUS, (WPARAM)0, (LPARAM)0) ;

			return (HDDEDATA)DDE_FACK;
		}
	}
	return (HDDEDATA) NULL;
}

/* Call back to put up menu and menu item */
VoidT F_ApiInitialize(init)
IntT init;{
	F_ObjHandleT	openCatcherCmdId, addRangeMarkerCmdId, includeFmtsCmdId, markerCmdId, editButtonsCmdId, editorFontCmdId ;
	F_ObjHandleT	saveButtonsCmdId, loadButtonsCmdId, autoCompleteId ;
	F_ObjHandleT    menubarId;
	F_ObjHandleT    bookmenubarId;
	F_ObjHandleT    separatorId;
	F_ObjHandleT	popupMenuId, markerId, popupId ;
	F_ObjHandleT    buttonsPopUpId;
	UCharT			szSaveOption[800] ;
	IntT			i, iStart, Count = 0, iSize ;
	UCharT			executeCommand[800], size[80] ;
	IntT			RegValueSize ;
	IntT			versionMajor;
	IntT			versionMinor;
	INT				exeRetCode ;

	switch(init){
		case FA_Init_First:
			if (VERSION == DEMO)
				F_Printf(NULL, (StringT)"\nemDEX v2.00 Demo\n");
			else
				F_Printf(NULL, (StringT)"\nemDEX v2.00\n");

			F_Printf(NULL, (StringT)"Copyright (c) 2003. All rights reserved.\n\n");

			versionMajor = getVersionMajor();
			versionMinor = getVersionMinor();

			// Only allow these versions of Framemaker.
			if (!(((versionMajor == 5) && (versionMinor >= 5)) ||
					((versionMajor == 6) /*&& (versionMinor == 0)*/) ||
					((versionMajor == 7) /*&& (versionMinor == 0)*/)))
			{
				F_ApiAlert((StringT)"This version of emDEX is incompatible with the version of FrameMaker you are running.", FF_ALERT_CONTINUE_WARN) ;
				break ;
			}

			GetPrivateProfileString("Options", "EditorLfHeight", "-15", szSaveOption, 800, "emDEX.ini") ;
			lf.lfHeight = F_StrAlphaToInt(szSaveOption) ;

			GetPrivateProfileString("Options", "EditorLfWidth", "0", szSaveOption, 800, "emDEX.ini") ;
			lf.lfWidth = F_StrAlphaToInt(szSaveOption) ;

			GetPrivateProfileString("Options", "EditorLfEscapement", "0", szSaveOption, 800, "emDEX.ini") ;
			lf.lfEscapement = F_StrAlphaToInt(szSaveOption) ;

			GetPrivateProfileString("Options", "EditorLfOrientation", "0", szSaveOption, 800, "emDEX.ini") ;
			lf.lfOrientation = F_StrAlphaToInt(szSaveOption) ;

			GetPrivateProfileString("Options", "EditorLfWeight", "400", szSaveOption, 800, "emDEX.ini") ;
			lf.lfWeight = F_StrAlphaToInt(szSaveOption) ;

			GetPrivateProfileString("Options", "EditorLfItalic", "0", szSaveOption, 800, "emDEX.ini") ;
			lf.lfItalic = F_StrAlphaToInt(szSaveOption) ;

			GetPrivateProfileString("Options", "EditorLfUnderline", "0", szSaveOption, 800, "emDEX.ini") ;
			lf.lfUnderline = F_StrAlphaToInt(szSaveOption) ;

			GetPrivateProfileString("Options", "EditorLfStrikeOut", "0", szSaveOption, 800, "emDEX.ini") ;
			lf.lfStrikeOut = F_StrAlphaToInt(szSaveOption) ;

			GetPrivateProfileString("Options", "EditorLfCharSet", "0", szSaveOption, 800, "emDEX.ini") ;
			lf.lfCharSet = F_StrAlphaToInt(szSaveOption) ;

			GetPrivateProfileString("Options", "EditorLfOutPrecision", "3", szSaveOption, 800, "emDEX.ini") ;
			lf.lfOutPrecision = F_StrAlphaToInt(szSaveOption) ;

			GetPrivateProfileString("Options", "EditorLfQuality", "1", szSaveOption, 800, "emDEX.ini") ;
			lf.lfQuality = F_StrAlphaToInt(szSaveOption) ;

			GetPrivateProfileString("Options", "EditorLfPitchAndFamily", "18", szSaveOption, 800, "emDEX.ini") ;
			lf.lfPitchAndFamily = F_StrAlphaToInt(szSaveOption) ;

			GetPrivateProfileString("Options", "EditorLfFaceName", "Times New Roman", szSaveOption, 800, "emDEX.ini") ;
			F_StrCpy(lf.lfFaceName, szSaveOption) ;

			// Set the button text
			GetPrivateProfileString("Options", "Button0Text", "&Bold", buttons[0].text, 800, "emDEX.ini") ;
			GetPrivateProfileString("Options", "Button0Action", "<Bold>", buttons[0].action, 800, "emDEX.ini") ;
			GetPrivateProfileString("Options", "Button1Text", "&Emphasis", buttons[1].text, 800, "emDEX.ini") ;
			GetPrivateProfileString("Options", "Button1Action", "<Emphasis>", buttons[1].action, 800, "emDEX.ini") ;
			GetPrivateProfileString("Options", "Button2Text", "&Default", buttons[2].text, 800, "emDEX.ini") ;
			GetPrivateProfileString("Options", "Button2Action", "</>", buttons[2].action, 800, "emDEX.ini") ;
			GetPrivateProfileString("Options", "Button3Text", "&No Page", buttons[3].text, 800, "emDEX.ini") ;
			GetPrivateProfileString("Options", "Button3Action", "<$nopage>", buttons[3].action, 800, "emDEX.ini") ;
			GetPrivateProfileString("Options", "Button4Text", "&Page", buttons[4].text, 800, "emDEX.ini") ;
			GetPrivateProfileString("Options", "Button4Action", "<$singlepage>", buttons[4].action, 800, "emDEX.ini") ;
			GetPrivateProfileString("Options", "Button5Text", "See &Also", buttons[5].text, 800, "emDEX.ini") ;
			GetPrivateProfileString("Options", "Button5Action", "<$nopage><Emphasis>see also </>", buttons[5].action, 800, "emDEX.ini") ;
			GetPrivateProfileString("Options", "Button6Text", "&See", buttons[6].text, 800, "emDEX.ini") ;
			GetPrivateProfileString("Options", "Button6Action", "<$nopage><Emphasis>see </>", buttons[6].action, 800, "emDEX.ini") ;
			GetPrivateProfileString("Options", "Button7Text", "s&tartrange", buttons[7].text, 800, "emDEX.ini") ;
			GetPrivateProfileString("Options", "Button7Action", "<$startrange>", buttons[7].action, 800, "emDEX.ini") ;
			GetPrivateProfileString("Options", "Button8Text", "end&range", buttons[8].text, 800, "emDEX.ini") ;
			GetPrivateProfileString("Options", "Button8Action", "<$endrange>", buttons[8].action, 800, "emDEX.ini") ;
			GetPrivateProfileString("Options", "Button9Text", "", buttons[9].text, 800, "emDEX.ini") ;
			GetPrivateProfileString("Options", "Button9Action", "", buttons[9].action, 800, "emDEX.ini") ;

			/* Get specialMenuId */
			menubarId = F_ApiGetNamedObject(FV_SessionId, FO_Menu, "!MakerMainMenu");
			if (menubarId) 
			{
				menuId = F_ApiDefineAndAddMenu(menubarId, "APIMainMenu", "emDEX");

				/* Define and add new commands to the emDEX menu */
				openCatcherCmdId = F_ApiDefineAndAddCommand(OPEN_CMD, menuId, (StringT) "mainopenCatcherCmd", (StringT) "Open emDEX window", (StringT) "");
				includeFmtsCmdId = F_ApiDefineAndAddCommand(INCLUDE_FMTS, menuId, (StringT) "mainincludeFmtsCmd", (StringT) "Include marker types ...", (StringT) "");

				/* Get the ID of Separator1 */
				separatorId = F_ApiGetNamedObject(FV_SessionId, FO_MenuItemSeparator, (StringT) "Separator1");
				F_ApiAddCommandToMenu(menuId, separatorId);

				markerCmdId = F_ApiDefineAndAddCommand(MARKER_CMD, menuId, (StringT) "mainmarkerCmd", (StringT) "Marker ...", (StringT) "");

				addRangeMarkerCmdId = F_ApiDefineAndAddCommand(ADDRANGE_CMD, menuId, (StringT) "mainaddrangeCmd", (StringT) "Range marker ...", (StringT) "");
				buttonsPopUpId = F_ApiDefineAndAddMenu(menuId, "mainEditorButtons", "Marker buttons");
				editButtonsCmdId = F_ApiDefineAndAddCommand(EDITBUTTONS_CMD, buttonsPopUpId, (StringT) "maineditbuttonsCmd", (StringT) "Edit ...", (StringT) "");
				saveButtonsCmdId = F_ApiDefineAndAddCommand(SAVEBUTTONS_CMD, buttonsPopUpId, (StringT) "mainsavebuttonsCmd", (StringT) "Save ...", (StringT) "");
				loadButtonsCmdId = F_ApiDefineAndAddCommand(LOADBUTTONS_CMD, buttonsPopUpId, (StringT) "mainloadbuttonsCmd", (StringT) "Load ...", (StringT) "");
				editorPopUpId = F_ApiDefineAndAddMenu(menuId, "mainEditorSize", "Marker size");
				smallId= F_ApiDefineAndAddCommand(SMALLSIZE_CMD, editorPopUpId, (StringT) "mainsmallsizeCmd", (StringT) "Small", (StringT) "");
				mediumId= F_ApiDefineAndAddCommand(MEDIUMSIZE_CMD, editorPopUpId, (StringT) "mainmediumsizeCmd", (StringT) "Medium", (StringT) "");
				largeId= F_ApiDefineAndAddCommand(LARGESIZE_CMD, editorPopUpId, (StringT) "mainlargesizeCmd", (StringT) "Large", (StringT) "");
				editorFontCmdId = F_ApiDefineAndAddCommand(EDITORFONT_CMD, menuId, (StringT) "maineditorfontCmd", (StringT) "Marker font ...", (StringT) "");
				autoCompleteId= F_ApiDefineAndAddCommand(AUTOCOMPLETE_CMD, menuId, (StringT) "mainautocompleteCmd", (StringT) "Autocomplete marker text", (StringT) "");

				F_ApiSetInt(editorPopUpId, smallId, FP_CanHaveCheckMark, 1);
				F_ApiSetInt(editorPopUpId, mediumId, FP_CanHaveCheckMark, 1);
				F_ApiSetInt(editorPopUpId, largeId, FP_CanHaveCheckMark, 1);
				F_ApiSetInt(menuId, autoCompleteId, FP_CanHaveCheckMark, 1);

				GetPrivateProfileString("Options", "EditorSize", "2", size, 800, "emDEX.ini") ;
				iSize = atoi(size) ;

				F_ApiSetInt(editorPopUpId, smallId, FP_CheckMarkIsOn, 0);
				F_ApiSetInt(editorPopUpId, mediumId, FP_CheckMarkIsOn, 0);
				F_ApiSetInt(editorPopUpId, largeId, FP_CheckMarkIsOn, 0);
				F_ApiSetInt(menuId, autoCompleteId, FP_CheckMarkIsOn, 0);

				if (iSize == 1)
					F_ApiSetInt(editorPopUpId, smallId, FP_CheckMarkIsOn, 1);

				if (iSize == 2)
					F_ApiSetInt(editorPopUpId, mediumId, FP_CheckMarkIsOn, 1);

				if (iSize == 3)
					F_ApiSetInt(editorPopUpId, largeId, FP_CheckMarkIsOn, 1);

				GetPrivateProfileString("Options", "AutoComplete", "1", size, 800, "emDEX.ini") ;
				g_autoComplete = atoi(size) ;

				if (g_autoComplete == 1)
					F_ApiSetInt(menuId, autoCompleteId, FP_CheckMarkIsOn, 1);

				/* Get the ID of Separator2 */
				separatorId = F_ApiGetNamedObject(FV_SessionId, FO_MenuItemSeparator, (StringT) "Separator2");
				F_ApiAddCommandToMenu(menuId, separatorId);

				F_ApiDefineAndAddCommand(HELP_CMD, menuId, (StringT) "mainhelp", (StringT) "Help ...", (StringT) "");
				F_ApiDefineAndAddCommand(ABOUT_CMD, menuId, (StringT) "mainabout", (StringT) "About ...", (StringT) "");

				// Only enable when a document or book is open.
				F_ApiSetInt(FV_SessionId, openCatcherCmdId, FP_EnabledWhen, FV_ENABLE_NEEDS_DOCP_OR_BOOKP);
				F_ApiSetInt(FV_SessionId, markerCmdId, FP_EnabledWhen, FV_ENABLE_NEEDS_DOCP_OR_BOOKP);
				F_ApiSetInt(FV_SessionId, addRangeMarkerCmdId, FP_EnabledWhen, FV_ENABLE_NEEDS_DOCP_OR_BOOKP);
				F_ApiSetInt(FV_SessionId, includeFmtsCmdId, FP_EnabledWhen, FV_ENABLE_NEEDS_DOCP_OR_BOOKP);
			}
			else
				F_ApiAlert((StringT) "Problem in finding main menu", FF_ALERT_CONTINUE_WARN);

			bookmenubarId = F_ApiGetNamedObject(FV_SessionId, FO_Menu, "!BookMainMenu") ;
			if (bookmenubarId) 
			{
				bookmenuId = F_ApiDefineAndAddMenu(bookmenubarId, "APIBookMenu", "emDEX");

				/* Define and add new commands to the emDEX menu */
				openCatcherCmdId = F_ApiDefineAndAddCommand(OPEN_CMD, bookmenuId, (StringT) "bookopenCatcherCmd", (StringT) "Open emDEX window", (StringT) "");
				includeFmtsCmdId = F_ApiDefineAndAddCommand(INCLUDE_FMTS, bookmenuId, (StringT) "bookincludeFmtsCmd", (StringT) "Include marker types ...", (StringT) "");

				/* Get the ID of Separator1 */
				separatorId = F_ApiGetNamedObject(FV_SessionId, FO_MenuItemSeparator, (StringT) "Separator1");
				F_ApiAddCommandToMenu(bookmenuId, separatorId);

				markerCmdId = F_ApiDefineAndAddCommand(MARKER_CMD, bookmenuId, (StringT) "bookmarkerCmd", (StringT) "Marker ...", (StringT) "");
				addRangeMarkerCmdId = F_ApiDefineAndAddCommand(ADDRANGE_CMD, bookmenuId, (StringT) "bookaddrangeCmd", (StringT) "Range marker ...", (StringT) "");
				buttonsPopUpId = F_ApiDefineAndAddMenu(bookmenuId, "bookEditorButtons", "Marker buttons");
				editButtonsCmdId = F_ApiDefineAndAddCommand(EDITBUTTONS_CMD, buttonsPopUpId, (StringT) "bookeditbuttonsCmd", (StringT) "Edit ...", (StringT) "");
				saveButtonsCmdId = F_ApiDefineAndAddCommand(SAVEBUTTONS_CMD, buttonsPopUpId, (StringT) "booksavebuttonsCmd", (StringT) "Save ...", (StringT) "");
				loadButtonsCmdId = F_ApiDefineAndAddCommand(LOADBUTTONS_CMD, buttonsPopUpId, (StringT) "bookloadbuttonsCmd", (StringT) "Load ...", (StringT) "");
				editorPopUpId = F_ApiDefineAndAddMenu(bookmenuId, "bookEditorSize", "Marker size");
				smallId= F_ApiDefineAndAddCommand(SMALLSIZE_CMD, editorPopUpId, (StringT) "booksmallsizeCmd", (StringT) "Small", (StringT) "");
				mediumId= F_ApiDefineAndAddCommand(MEDIUMSIZE_CMD, editorPopUpId, (StringT) "bookmediumsizeCmd", (StringT) "Medium", (StringT) "");
				largeId= F_ApiDefineAndAddCommand(LARGESIZE_CMD, editorPopUpId, (StringT) "booklargesizeCmd", (StringT) "Large", (StringT) "");
				editorFontCmdId = F_ApiDefineAndAddCommand(EDITORFONT_CMD, bookmenuId, (StringT) "bookeditorfontCmd", (StringT) "Marker font ...", (StringT) "");
				autoCompleteId= F_ApiDefineAndAddCommand(AUTOCOMPLETE_CMD, bookmenuId, (StringT) "bookautocompleteCmd", (StringT) "Autocomplete marker text", (StringT) "");

				F_ApiSetInt(editorPopUpId, smallId, FP_CanHaveCheckMark, 1);
				F_ApiSetInt(editorPopUpId, mediumId, FP_CanHaveCheckMark, 1);
				F_ApiSetInt(editorPopUpId, largeId, FP_CanHaveCheckMark, 1);
				F_ApiSetInt(bookmenuId, autoCompleteId, FP_CanHaveCheckMark, 1);

				GetPrivateProfileString("Options", "EditorSize", "2", size, 800, "emDEX.ini") ;
				iSize = atoi(size) ;

				F_ApiSetInt(editorPopUpId, smallId, FP_CheckMarkIsOn, 0);
				F_ApiSetInt(editorPopUpId, mediumId, FP_CheckMarkIsOn, 0);
				F_ApiSetInt(editorPopUpId, largeId, FP_CheckMarkIsOn, 0);
				F_ApiSetInt(bookmenuId, autoCompleteId, FP_CheckMarkIsOn, 0);

				if (iSize == 1)
					F_ApiSetInt(editorPopUpId, smallId, FP_CheckMarkIsOn, 1);

				if (iSize == 2)
					F_ApiSetInt(editorPopUpId, mediumId, FP_CheckMarkIsOn, 1);

				if (iSize == 3)
					F_ApiSetInt(editorPopUpId, largeId, FP_CheckMarkIsOn, 1);

				GetPrivateProfileString("Options", "AutoComplete", "1", size, 800, "emDEX.ini") ;
				g_autoComplete = atoi(size) ;

				if (g_autoComplete == 1)
					F_ApiSetInt(bookmenuId, autoCompleteId, FP_CheckMarkIsOn, 1);

				/* Get the ID of Separator2 */
				separatorId = F_ApiGetNamedObject(FV_SessionId, FO_MenuItemSeparator, (StringT) "Separator2");
				F_ApiAddCommandToMenu(bookmenuId, separatorId);

				F_ApiDefineAndAddCommand(HELP_CMD, bookmenuId, (StringT) "bookhelp", (StringT) "Help ...", (StringT) "");
				F_ApiDefineAndAddCommand(ABOUT_CMD, bookmenuId, (StringT) "bookabout", (StringT) "About ...", (StringT) "");

				// Only enable when a document or book is open.
				F_ApiSetInt(FV_SessionId, openCatcherCmdId, FP_EnabledWhen, FV_ENABLE_NEEDS_DOCP_OR_BOOKP);
				F_ApiSetInt(FV_SessionId, markerCmdId, FP_EnabledWhen, FV_ENABLE_NEEDS_DOCP_OR_BOOKP);
				F_ApiSetInt(FV_SessionId, addRangeMarkerCmdId, FP_EnabledWhen, FV_ENABLE_NEEDS_DOCP_OR_BOOKP);
				F_ApiSetInt(FV_SessionId, includeFmtsCmdId, FP_EnabledWhen, FV_ENABLE_NEEDS_DOCP_OR_BOOKP);
			}
			else
				F_ApiAlert((StringT) "Problem in finding main menu", FF_ALERT_CONTINUE_WARN);

			// Add it into the popup menus too
			popupMenuId = F_ApiGetNamedObject(FV_SessionId, FO_Menu, "!TextContextMenu") ;
			markerId = F_ApiGetNamedObject(FV_SessionId, FO_Command, (StringT) "Marker");
			popupId = F_ApiDefineAndAddCommand(ADDRANGE_CMD, popupMenuId, (StringT) "textContextRangemarkerCmd", (StringT) "emDEX Range Marker ...", (StringT) "") ;
			// Place the menu item after the FM marker item
			F_ApiSetId(popupMenuId, popupId, FP_PrevMenuItemInMenu, markerId);
			popupId = F_ApiDefineAndAddCommand(MARKER_CMD, popupMenuId, (StringT) "textContextmarkerCmd", (StringT) "emDEX Marker ...", (StringT) "") ;
			// Place the menu item after the FM marker item
			F_ApiSetId(popupMenuId, popupId, FP_PrevMenuItemInMenu, markerId);

			popupMenuId = F_ApiGetNamedObject(FV_SessionId, FO_Menu, "!TableTextContextMenu") ;
			markerId = F_ApiGetNamedObject(FV_SessionId, FO_Command, (StringT) "Marker");
			popupId = F_ApiDefineAndAddCommand(ADDRANGE_CMD, popupMenuId, (StringT) "tableTextContextRangemarkerCmd", (StringT) "emDEX Range Marker ...", (StringT) "") ;
			// Place the menu item after the FM marker item
			F_ApiSetId(popupMenuId, popupId, FP_PrevMenuItemInMenu, markerId);
			popupId = F_ApiDefineAndAddCommand(MARKER_CMD, popupMenuId, (StringT) "tableTextContextmarkerCmd", (StringT) "emDEX Marker ...", (StringT) "") ;
			// Place the menu item after the FM marker item
			F_ApiSetId(popupMenuId, popupId, FP_PrevMenuItemInMenu, markerId);

			// Set notification messages.
			F_ApiNotification(FA_Note_PostOpenDoc, True);
//			F_ApiNotification(FA_Note_PostOpenBook, True);
			F_ApiNotification(FA_Note_PreQuitDoc, True);
			F_ApiNotification(FA_Note_PreQuitBook, True);
			F_ApiNotification(FA_Note_PostQuitDoc, True);
			F_ApiNotification(FA_Note_PostQuitBook, True);
			F_ApiNotification(FA_Note_PostFunction, True);
			F_ApiNotification(FA_Note_PostQuitSession, True);
//			F_ApiNotification(FA_Note_PostMouseCommand, True);

			if (VERSION == DEMO)
				itemLimit = 100 ;
			else
				itemLimit = 5000 ;

			autoComplete[0] [0] = '\0' ;

			// Get the types to include
			GetPrivateProfileString("Options", "IncludeTypes", "Index@!", szSaveOption, 800, "emDEX.ini") ;

			for (i = 0, iStart = 0 ; i < F_StrLen(szSaveOption) ; i++)
			{
				if ((szSaveOption[i] == '@') && (szSaveOption[i+1] == '!'))
				{
					F_StrCpyN(IncludeTypes[Count], &szSaveOption[iStart], i-iStart+1) ;
					IncludeTypes[Count][i-iStart+1] = '\0' ;
					IncludeTypes[Count+1][0] = '\0' ;

					Count++ ;
					i += 2 ;
					iStart = i ;
				}
			}

			markerTypes = F_StrListNew(80, 2);

			for (i = 0 ; IncludeTypes[i][0] != '\0' ; i++)
				F_StrListAppend(markerTypes, IncludeTypes[i]);

			// Check if real-time update is on.
			GetPrivateProfileString("Options", "RealTimeUpdates", "1", szSaveOption, 800, "emDEX.ini") ;

			if (szSaveOption[0] == '1')
				updateInRealTime = TRUE ;
			else
				updateInRealTime = FALSE ;

			productName = getProductName() ;

			/* Register as a DDE server */
			if (DdeInitialize(&ddeInst, DdeCallBack, APPCLASS_STANDARD, 0))
        	{
				F_ApiAlert((StringT)"DDE Initialize FAILED.",
					FF_ALERT_CONTINUE_WARN);
				break;
			}

			/* Register the DDE Instance ID with the FrameMaker API */
			F_ApiSetDdeInstance(ddeInst);

			/* Create DDE string handles */
			hszFrameDDE = DdeCreateStringHandle(ddeInst,
				"emDEX", CP_WINANSI);
			
   	   		/* Register service name */
			if (DdeNameService(ddeInst,	hszFrameDDE, 0, DNS_REGISTER) == 0)
				F_ApiAlert((StringT)"DDE NameService FAILED.",
					FF_ALERT_CONTINUE_WARN);

			// Get the installation directory from the registry
			RegValueSize = _MAX_PATH * 2 ;

			RegQueryValue(HKEY_LOCAL_MACHINE, "SOFTWARE\\emDEX\\Directory", RegValue, &RegValueSize) ;

			F_Sprintf(executeCommand, "%s\\emDEX.exe", RegValue) ;
			F_Sprintf(helpPath, "%s\\emDEX.chm", RegValue) ;

			exeRetCode = (INT)ShellExecute(NULL, "open", executeCommand, "", NULL, SW_SHOWNORMAL) ;

			if ((exeRetCode == 0) || (exeRetCode == SE_ERR_OOM))
			{
				F_ApiAlert((StringT) "Out of memory opening the emDEX server.\n\nIncrease the system virtual memory setting in your control panel.", FF_ALERT_CONTINUE_WARN);
			}
			else
			{
				if (exeRetCode < 32)
				{
					F_Sprintf(szSaveOption, "Error %d opening the emDEX server.\n\nRetry or contact support at support@emdex.ca.", exeRetCode) ;

					F_ApiAlert((StringT) szSaveOption, FF_ALERT_CONTINUE_WARN);
				}
			}

//			ShellExecute(NULL, "open", "c:\\cpp\\psindexcatcher\\debug\\emdex.exe", "", NULL, SW_SHOWNORMAL) ;
		
			F_ApiBailOut();
			break;
	}
}

VoidT F_ApiCommand(command)
IntT command;
{
	F_StringsT apiExcludeList;
	F_StringsT apiIncludeList;
	F_StringsT apiRangeList;

	F_ObjHandleT docId = 0, compId, fmtId, markerId ;
	F_ObjHandleT testId ;
	StringT docName ;
	HWND catcherWindow ;
	CHAR word[800] ;
	UCharT tempMarkerName[800], szSaveOption[800] ;
	IntT i, commandId, def ;
	F_TextRangeT    tr;
	F_TextLocT loc;
	ATOM tempAtom ;
	HWND	wnd ;
	CHOOSEFONT cf = {sizeof(CHOOSEFONT)};
	HWND	hEditWindow ;
	INT		exeRetCode ;
	LOGFONT lfTemp;

	F_StringsT strings;

	docId = F_ApiGetId(FV_SessionId, FV_SessionId, FP_ActiveDoc);

	switch(command)
	{
		case OPEN_CMD:
			catcherWindow = FindWindow(NULL, "emDEX") ;

			if (catcherWindow == NULL)
			{
				// Main hidden window not active.
				F_ApiAlert((StringT) "The emDEX server is not active.\n\nStart the server and retry.", FF_ALERT_CONTINUE_WARN);
				break ;
			}

			if (VERSION == DEMO)
				F_ApiAlert((StringT) "You are running the demo version of emDEX.\n\nOnly the first 100 index entries will be displayed.\n\nYou can order the full version at www.emdex.ca", FF_ALERT_CONTINUE_WARN);

			if (docId == 0)
			{
				// Check to see if it's a book.
				docId = F_ApiGetId(FV_SessionId, FV_SessionId, FP_ActiveBook);

				if (docId != 0)
				{
					openAllDocs(docId) ;
				}
			}

/*			strings = F_ApiGetStrings(FV_SessionId, docId, FP_Dictionary);

			for (i = 0 ; i < strings.len ; i++)
			{
				F_Printf(NULL, (StringT) "%s\n", strings.val[i]);
			} */

			// Check if real-time update is on.
			GetPrivateProfileString("Options", "RealTimeUpdates", "1", szSaveOption, 800, "emDEX.ini") ;

			if (szSaveOption[0] == '1')
				updateInRealTime = TRUE ;
			else
				updateInRealTime = FALSE ;

			docName = F_ApiGetString(docId, docId, FP_Name) ;

			updateDocName(docId) ;

			catcherWindow = FindWindow(NULL, szWindowTitle) ;

			if (catcherWindow != NULL)
			{
				// Catcher window already exists, bring it back into focus.
				ShowWindow(catcherWindow, SW_RESTORE) ;
				SetFocus(catcherWindow) ;
				SetForegroundWindow(catcherWindow) ;
				F_Free(docName) ;
//				break ;
			}
			else
			{
				while ((tempAtom = GlobalFindAtom(docName)) != 0)
					GlobalDeleteAtom(tempAtom) ;

				SendMessage(FindWindow(NULL, "emDEX"), CATCHER_OPEN, (WPARAM)0, 
							(LPARAM)GlobalAddAtom(docName)) ;

				F_Free(docName) ;
			}

			if (updateAllMarkers(docId) == -1)
			{
				break ;
			}

			// Update the listbox
			SendMessage(FindWindow(NULL, szWindowTitle), LISTBOX_UPDATE, (WPARAM)0, (LPARAM)0) ;

			// Do it again just to make sure.
			updateDocName(docId) ;

			if (updateAllMarkers(docId) == -1)
			{
				break ;
			}

			// Update the listbox
			SendMessage(FindWindow(NULL, szWindowTitle), LISTBOX_UPDATE, (WPARAM)0, (LPARAM)0) ;

			setEditorText() ;

			break ;

		case MARKER_CMD :
			openEditorWindow("emDEX Marker") ;

			break ;

		case EDITBUTTONS_CMD :
			catcherWindow = FindWindow(NULL, "emDEX") ;

			if (catcherWindow == NULL)
			{
				// Main hidden window not active.
				F_ApiAlert((StringT) "The emDEX server is not active.\n\nStart the server and retry.", FF_ALERT_CONTINUE_WARN);
				break ;
			}

			if (g_CurrentEditor != 0)
			{
				F_ApiAlert((StringT) "Close the emDEX Marker window before editing the buttons.\n", FF_ALERT_CONTINUE_WARN);
				break ;
			}

			SendMessage(FindWindow(NULL, "emDEX"), EDIT_BUTTONS, (WPARAM)0, (LPARAM)0) ;

			break ;

		case SAVEBUTTONS_CMD :
			if (FindWindow(NULL, "emDEX Marker Buttons") != NULL)
			{
				F_ApiAlert((StringT) "Close the emDEX Marker Buttons window before saving the buttons.\n", FF_ALERT_CONTINUE_WARN);
				return ;
			}

			saveButtons() ;

			break ;

		case LOADBUTTONS_CMD :
			if (g_CurrentEditor != 0)
			{
				F_ApiAlert((StringT) "Close the emDEX Marker window before loading the buttons.\n", FF_ALERT_CONTINUE_WARN);
				break ;
			}

			if (FindWindow(NULL, "emDEX Marker Buttons") != NULL)
			{
				F_ApiAlert((StringT) "Close the emDEX Marker Buttons window before loading the buttons.\n", FF_ALERT_CONTINUE_WARN);
				return ;
			}

			loadButtons() ;

			break ;

		case EDITORFONT_CMD :
			cf.Flags = /*CF_EFFECTS |*/ CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS ;
            cf.hwndOwner = GetActiveWindow() ;

			lfTemp = lf ;
            cf.lpLogFont = &lf;

            if(ChooseFont(&cf))
            {
                hf = CreateFontIndirect(&lf);

				/* Save the options. */
				ltoa(lf.lfHeight, szSaveOption, 10) ;
				WritePrivateProfileString("Options", "EditorLfHeight", szSaveOption, "emDEX.ini");

				ltoa(lf.lfWidth, szSaveOption, 10) ;
				WritePrivateProfileString("Options", "EditorLfWidth", szSaveOption, "emDEX.ini");

				ltoa(lf.lfEscapement, szSaveOption, 10) ;
				WritePrivateProfileString("Options", "EditorLfEscapement", szSaveOption, "emDEX.ini");

				ltoa(lf.lfOrientation, szSaveOption, 10) ;
				WritePrivateProfileString("Options", "EditorLfOrientation", szSaveOption, "emDEX.ini");

				ltoa(lf.lfWeight, szSaveOption, 10) ;
				WritePrivateProfileString("Options", "EditorLfWeight", szSaveOption, "emDEX.ini");

				ltoa(lf.lfItalic, szSaveOption, 10) ;
				WritePrivateProfileString("Options", "EditorLfItalic", szSaveOption, "emDEX.ini");

				ltoa(lf.lfUnderline, szSaveOption, 10) ;
				WritePrivateProfileString("Options", "EditorLfUnderline", szSaveOption, "emDEX.ini");

				ltoa(lf.lfStrikeOut, szSaveOption, 10) ;
				WritePrivateProfileString("Options", "EditorLfStrikeOut", szSaveOption, "emDEX.ini");

				ltoa(lf.lfCharSet, szSaveOption, 10) ;
				WritePrivateProfileString("Options", "EditorLfCharSet", szSaveOption, "emDEX.ini");

				ltoa(lf.lfOutPrecision, szSaveOption, 10) ;
				WritePrivateProfileString("Options", "EditorLfOutPrecision", szSaveOption, "emDEX.ini");

				ltoa(lf.lfQuality, szSaveOption, 10) ;
				WritePrivateProfileString("Options", "EditorLfQuality", szSaveOption, "emDEX.ini");

				ltoa(lf.lfPitchAndFamily, szSaveOption, 10) ;
				WritePrivateProfileString("Options", "EditorLfPitchAndFamily", szSaveOption, "emDEX.ini");

				WritePrivateProfileString("Options", "EditorLfFaceName", lf.lfFaceName, "emDEX.ini");

				hEditWindow = FindWindow(NULL, "emDEX Marker") ;
				if (hEditWindow == NULL)
					hEditWindow = FindWindow(NULL, "emDEX Range Marker") ;

				if (hEditWindow)
				{
					SendMessage(GetDlgItem(hEditWindow, 101), WM_SETFONT, (WPARAM)hf, MAKELPARAM(1, 0)) ;
				}
			}
			else
			{
				lf = lfTemp ;
			}

			break ;

		case SMALLSIZE_CMD :
			F_ApiSetInt(editorPopUpId, F_ApiGetNamedObject(FV_SessionId, FO_Command, "mainsmallsizeCmd"), FP_CheckMarkIsOn, True);
			F_ApiSetInt(editorPopUpId, F_ApiGetNamedObject(FV_SessionId, FO_Command, "mainmediumsizeCmd"), FP_CheckMarkIsOn, False);
			F_ApiSetInt(editorPopUpId, F_ApiGetNamedObject(FV_SessionId, FO_Command, "mainlargesizeCmd"), FP_CheckMarkIsOn, False);

			F_ApiSetInt(editorPopUpId, F_ApiGetNamedObject(FV_SessionId, FO_Command, "booksmallsizeCmd"), FP_CheckMarkIsOn, True);
			F_ApiSetInt(editorPopUpId, F_ApiGetNamedObject(FV_SessionId, FO_Command, "bookmediumsizeCmd"), FP_CheckMarkIsOn, False);
			F_ApiSetInt(editorPopUpId, F_ApiGetNamedObject(FV_SessionId, FO_Command, "booklargesizeCmd"), FP_CheckMarkIsOn, False);

			WritePrivateProfileString("Options", "EditorSize", "1", "emDEX.ini");
			setEditorSize(SMALL) ;

			break ;

		case MEDIUMSIZE_CMD :
			F_ApiSetInt(editorPopUpId, F_ApiGetNamedObject(FV_SessionId, FO_Command, "mainsmallsizeCmd"), FP_CheckMarkIsOn, False);
			F_ApiSetInt(editorPopUpId, F_ApiGetNamedObject(FV_SessionId, FO_Command, "mainmediumsizeCmd"), FP_CheckMarkIsOn, True);
			F_ApiSetInt(editorPopUpId, F_ApiGetNamedObject(FV_SessionId, FO_Command, "mainlargesizeCmd"), FP_CheckMarkIsOn, False);

			F_ApiSetInt(editorPopUpId, F_ApiGetNamedObject(FV_SessionId, FO_Command, "booksmallsizeCmd"), FP_CheckMarkIsOn, False);
			F_ApiSetInt(editorPopUpId, F_ApiGetNamedObject(FV_SessionId, FO_Command, "bookmediumsizeCmd"), FP_CheckMarkIsOn, True);
			F_ApiSetInt(editorPopUpId, F_ApiGetNamedObject(FV_SessionId, FO_Command, "booklargesizeCmd"), FP_CheckMarkIsOn, False);

			WritePrivateProfileString("Options", "EditorSize", "2", "emDEX.ini");
			setEditorSize(MEDIUM) ;

			break ;

		case LARGESIZE_CMD :
			F_ApiSetInt(editorPopUpId, F_ApiGetNamedObject(FV_SessionId, FO_Command, "mainsmallsizeCmd"), FP_CheckMarkIsOn, False);
			F_ApiSetInt(editorPopUpId, F_ApiGetNamedObject(FV_SessionId, FO_Command, "mainmediumsizeCmd"), FP_CheckMarkIsOn, False);
			F_ApiSetInt(editorPopUpId, F_ApiGetNamedObject(FV_SessionId, FO_Command, "mainlargesizeCmd"), FP_CheckMarkIsOn, True);

			F_ApiSetInt(editorPopUpId, F_ApiGetNamedObject(FV_SessionId, FO_Command, "booksmallsizeCmd"), FP_CheckMarkIsOn, False);
			F_ApiSetInt(editorPopUpId, F_ApiGetNamedObject(FV_SessionId, FO_Command, "bookmediumsizeCmd"), FP_CheckMarkIsOn, False);
			F_ApiSetInt(editorPopUpId, F_ApiGetNamedObject(FV_SessionId, FO_Command, "booklargesizeCmd"), FP_CheckMarkIsOn, True);

			WritePrivateProfileString("Options", "EditorSize", "3", "emDEX.ini");
			setEditorSize(LARGE) ;

			break ;

		case AUTOCOMPLETE_CMD :
			if (g_autoComplete == 0)
			{
				F_ApiSetInt(menuId, F_ApiGetNamedObject(FV_SessionId, FO_Command, "mainautocompleteCmd"), FP_CheckMarkIsOn, True);
				F_ApiSetInt(bookmenuId, F_ApiGetNamedObject(FV_SessionId, FO_Command, "bookautocompleteCmd"), FP_CheckMarkIsOn, True);

				WritePrivateProfileString("Options", "AutoComplete", "1", "emDEX.ini");
				g_autoComplete = 1 ;
			}
			else
			{
				F_ApiSetInt(menuId, F_ApiGetNamedObject(FV_SessionId, FO_Command, "mainautocompleteCmd"), FP_CheckMarkIsOn, False);
				F_ApiSetInt(bookmenuId, F_ApiGetNamedObject(FV_SessionId, FO_Command, "bookautocompleteCmd"), FP_CheckMarkIsOn, False);

				WritePrivateProfileString("Options", "AutoComplete", "0", "emDEX.ini");
				g_autoComplete = 0 ;
			}

			break ;

		case ADDRANGE_CMD :
		    /* Get the text selection, if any. */
//			F_Printf(NULL, (StringT) "docid = %d\n", docId);

			openEditorWindow("emDEX Range Marker") ;

			break ;

		case ABOUT_CMD:
			if (VERSION == DEMO)
				F_ApiAlert("emDEX client v2.00 Demo\n\nCopyright (c) 2003\n\n\nemail : support@emdex.ca\n\nweb : www.emdex.ca", FF_ALERT_CONTINUE_NOTE);
			else
				F_ApiAlert("emDEX client v2.00\n\nCopyright (c) 2003\n\n\nemail : support@emdex.ca\n\nweb : www.emdex.ca", FF_ALERT_CONTINUE_NOTE);

			break ;

		case HELP_CMD:
//			WinHelp(NULL, helpPath, HELP_CONTENTS, 0) ;
//			HtmlHelp(NULL, helpPath, HH_DISPLAY_TOPIC, 0) ;
			exeRetCode = (INT)ShellExecute(NULL, "open", helpPath, "", NULL, SW_SHOWNORMAL) ;

			if ((exeRetCode == 0) || (exeRetCode == SE_ERR_OOM))
			{
				F_ApiAlert((StringT) "Out of memory opening the help window.\n\nIncrease the system virtual memory setting in your control panel.", FF_ALERT_CONTINUE_WARN);
			}
			else
			{
				if (exeRetCode < 32)
				{
					F_Sprintf(szSaveOption, "Error %d opening the help window.\n\nRetry or contact support at support@emdex.ca.", exeRetCode) ;

					F_ApiAlert((StringT) szSaveOption, FF_ALERT_CONTINUE_WARN);
				}
			}

			break ;

		case INCLUDE_FMTS:
			activeDocId = F_ApiGetId(FV_SessionId, FV_SessionId, FP_ActiveDoc);
			if(!activeDocId)
			{
				activeDocId = F_ApiGetId(FV_SessionId, FV_SessionId, FP_ActiveBook);

				if(!activeDocId)
				{
					F_ApiAlert((StringT) "Please open a document before invoking this command.", FF_ALERT_CONTINUE_WARN);
					return;	
				}
			}

			excludeList = getListOfTypes(activeDocId);
			if(!excludeList){
				F_ApiAlert((StringT) "No formats currently defined", FF_ALERT_CONTINUE_WARN);
				return;
			}
			apiExcludeList = convertToApiList(excludeList);
			if(apiExcludeList.val == NULL){
				F_ApiAlert((StringT) "Could not convert list: possible memory problem", FF_ALERT_CONTINUE_WARN);
				return;
			}

			tempTypes = F_StrListCopyList(markerTypes) ;

			/* Open the dialog box resource */
			includeDialogId = F_ApiOpenResource(FO_DialogResource, INCLUDE_DLG);
			if(!includeDialogId){
				F_ApiAlert((StringT) "Unable to open dialog resource", FF_ALERT_CONTINUE_WARN);
				return;
			}
			excludeScrollBoxId = F_ApiDialogItemId(includeDialogId, EXCLUDE_BOX);
			includeScrollBoxId = F_ApiDialogItemId(includeDialogId, INCLUDE_BOX);
			/* stuff the exclude box with the list of currently define paragraph foramts */
			F_ApiSetStrings(includeDialogId, excludeScrollBoxId,  FP_Labels, &apiExcludeList);
			F_ApiDeallocateStrings(&apiExcludeList);
			F_ApiSetInt(includeDialogId, excludeScrollBoxId, FP_State, -1);
			apiIncludeList = convertToApiList(markerTypes);
			F_ApiSetStrings(includeDialogId, includeScrollBoxId,  FP_Labels, &apiIncludeList);
			F_ApiDeallocateStrings(&apiIncludeList);
			F_ApiModalDialog(INCLUDE_DIALOG, includeDialogId);
			F_ApiClose(includeDialogId, 0) ;
			break;
	}
}

VoidT F_ApiNotify(notification, docId, sparm, iparm)
     IntT notification;
     F_ObjHandleT docId;
     StringT sparm;
     IntT iparm; /* inset identifier */
{
	F_ObjHandleT insetId = 0;
	F_ObjHandleT testId ;
	IntT commandId ;
	static StringT docName;
	F_ObjHandleT markerTypeId, rPg1Id, paraId, pageFrameId ;
	F_TextItemsT textItems; 
	IntT	i ;
	F_ObjHandleT pgfId,  flowId, objectId, textframeId;
	IntT pageType;
	HWND	hEditWindow, hTextEntry ;

	switch(notification) 
	{
/*		case FA_Note_PreOpenDoc:
		case FA_Note_PreOpenBook: */
		case FA_Note_PostFunction:
/*			testId = F_ApiGetNamedObject(FV_SessionId, FO_Command, (StringT)"Marker") ;
			commandId = F_ApiGetInt(testId, testId, FP_CommandNum);
			F_ApiCommand(commandId) ; */

			setEditorText() ;

/*			if (((iparm >= 0x100) && (iparm <= 0x110)) ||
				((iparm >= 0x140) && (iparm <= 0x14D)) ||
				(iparm == 0x355) || (iparm == 0x682) ||
				(iparm == 0x34C) || (iparm == 0x34D)) */

			// Disregard cursor-positioning and dialog opening/closing commands.
			if (((iparm >= 0x100) && (iparm <= 0x110)) ||
				((iparm >= 0x140) && (iparm <= 0x14D)) ||
				((iparm >= 0x681) && (iparm <= 0x68D)) ||
				(iparm == 0x355) || (iparm == 0x359) ||
				(iparm == 0x357) || (iparm == 0x35E) ||
				(iparm == 0xF81) || (iparm == 0xF86) ||
				(iparm == 0x34C) || (iparm == 0x34D))
			{
				break ;
			}

//			F_Printf(NULL, (StringT) "iparm = %d\n", iparm);

			if (updateInRealTime == False)
				break ;

			// If updating switched off, ignore text changes.
/*			if ((updateInRealTime == False) && ((iparm == 0xDF2) || (iparm == 0x22F) || (iparm == 0x323) ||
				((iparm >= 0x112) && (iparm <= 0x119))))
				break ; */

			updateDocName(docId) ;

			if (FindWindow(NULL, szWindowTitle) != NULL)
			{
				if (updateAllMarkers(docId) != -1)
				{
					// Update the listbox
					SendMessage(FindWindow(NULL, szWindowTitle), LISTBOX_UPDATE, (WPARAM)0, (LPARAM)0) ;
				}
			}

			docName = F_ApiGetString(docId, docId, FP_Name) ;

			if ((docId = getContainedInBook(docName)) != 0)
			{
				updateDocName(docId) ;

				if (FindWindow(NULL, szWindowTitle) != NULL)
				{
					if (updateAllMarkers(docId) == -1)
					{
						if (docName != NULL)
							F_Free(docName) ;

						break ;
					}

					// Update the listbox
					SendMessage(FindWindow(NULL, szWindowTitle), LISTBOX_UPDATE, (WPARAM)0, (LPARAM)0) ;
				}
			}

			if (docName != NULL)
				F_Free(docName) ;

			break;

/*		case FA_Note_PostOpenBook:
		case FA_Note_PostMouseCommand:

			break ; */

		case FA_Note_PostOpenDoc:
			loadAutoCompleteText(docId) ;

			break ;

		case FA_Note_BackToUser :
			setEditorText() ;

			break ;

		case FA_Note_PreQuitDoc:
		case FA_Note_PreQuitBook:
			updateDocName(docId) ;

			docName = F_ApiGetString(docId, docId, FP_Name) ;

			break ;

		case FA_Note_PostQuitDoc:
		case FA_Note_PostQuitBook:
			setEditorText() ;

			SendMessage(FindWindow(NULL, szWindowTitle), WM_CLOSE, (WPARAM)0, (LPARAM)0) ;

			if ((docId = getContainedInBook(docName)) != 0)
			{
				// Remove the doc from the book's catcher window.
				updateDocName(docId) ;

				if (FindWindow(NULL, szWindowTitle) != NULL)
				{
					if (updateAllMarkers(docId) == -1)
					{
						if (docName != NULL)
							F_Free(docName) ;

						break ;
					}

					// Update the listbox
					SendMessage(FindWindow(NULL, szWindowTitle), LISTBOX_UPDATE, (WPARAM)0, (LPARAM)0) ;
				}
			}

			if (docName != NULL)
				F_Free(docName) ;

			break;

		case FA_Note_PostQuitSession:
			// Kill the main server.
			SendMessage(FindWindow(NULL, "emDEX"), WM_CLOSE, (WPARAM)0, (LPARAM)0) ;

			/* Free DDE String Handles */
/*			DdeFreeStringHandle(ddeInst, hszFrameDDE);

			/* Unregister DDE Instance with FrameMaker */
/*			F_ApiSetDdeInstance(0);

			/* Unregister with DDE */
/*		    if (!DdeUninitialize(ddeInst))
    			F_ApiAlert((StringT)"Unable to Uninitialize DDE",
    				FF_ALERT_OK_DEFAULT); */

			break;
	}
}

VoidT F_ApiDialogEvent(dlgNum, itemNum, modifiers)
IntT dlgNum;
IntT itemNum;
IntT modifiers;{
	IntT i, j, k ;
	static IntT completeCount = 0 ;
	StringT s;
	F_StringsT apiExcludeList;
	F_StringsT apiIncludeList;
	F_ObjHandleT docId, tmpDocId ;
	F_ObjHandleT saveBookId[80] ;
	StringT docName;
	UCharT	tempText[800], autoCompleteText[800] ;
	StringT listName ;
	F_TextRangeT    tr, tr1;
	F_TextLocT loc;
	BoolT doProcess = True ;
	HWND	hEditWindow ;
	HWND	hTextEntry ;
	F_ObjHandleT markerId ;
	UCharT tempMarkerName[800] ;
	UCharT SaveOption[800] ;
	F_ObjHandleT markerTypeId ;
	RECT	winPos ;
	POINT	pt ;
	IntT fcodes[1] ;
	F_ObjHandleT    edefId, elemId, insetId, childId ;
	F_AttributesT   attributes, att ;
	F_ElementRangeT er;
	F_TextItemsT textItems ;
	StringT elementName = NULL;
	DWORD	start, end ;
	F_TextLocT tl;
	static IntT demoCount = 1 ;

	for (i = 0 ; i < 80 ; i++)
		saveBookId[i] = 0 ;

	// Cope different dialogs being handled by this routine.
	if (dlgNum == INCLUDE_DIALOG)
	{
		switch(itemNum)
		{
			case EXCLUDE_BOX:
				moveItemName  = getSelectedLabel(includeDialogId, itemNum);
				moveItemNum = getSelectedLabelIndex(includeDialogId, itemNum);
				F_ApiReturnValue(FR_ModalStayUp);
				break ;
			case INCLUDE_BOX:
				moveItemName  = getSelectedLabel(includeDialogId, itemNum);
				moveItemNum = getSelectedLabelIndex(includeDialogId, itemNum);
				F_ApiReturnValue(FR_ModalStayUp);
				break;
			case RIGHT_ARROW:
				if(moveItemName){
					if(F_StrListIndex(excludeList, moveItemName) != -1){
						if(excludeList && moveItemName){
							F_StrListRemove(excludeList, moveItemNum);
							for(i = 0; i < F_StrListLen(tempTypes); i++){
								s = F_StrListGet(tempTypes, i);
								if(F_StrCmp(s, moveItemName ) >= 0)
									break;
							}
							if(i < F_StrListLen(tempTypes)){
								F_StrListInsert(tempTypes, moveItemName, i);
								F_Free(moveItemName);
								F_ApiSetInt(includeDialogId, includeScrollBoxId, FP_FirstVis, i);
							}
							else{
								F_StrListAppend(tempTypes, moveItemName);
								F_ApiSetInt(includeDialogId, includeScrollBoxId, FP_FirstVis, F_StrListLen(tempTypes) -1);
							}
							moveItemName = NULL;
							apiExcludeList = convertToApiList(excludeList);
							F_ApiSetStrings(includeDialogId, excludeScrollBoxId,  FP_Labels, &apiExcludeList);
							F_ApiDeallocateStrings(&apiExcludeList);
							F_ApiSetInt(includeDialogId, excludeScrollBoxId, FP_State, -1);
							apiIncludeList = convertToApiList(tempTypes);
							F_ApiSetStrings(includeDialogId, includeScrollBoxId,  FP_Labels, &apiIncludeList);
							F_ApiDeallocateStrings(&apiIncludeList);
							F_ApiSetInt(includeDialogId, includeScrollBoxId, FP_State, -1);
						}
					}
					F_Free(moveItemName);
				}
				F_ApiReturnValue(FR_ModalStayUp);
				break;

			case LEFT_ARROW:
				if(moveItemName){
					if(F_StrListIndex(tempTypes, moveItemName) != -1){
						if(tempTypes && moveItemName){
							F_StrListRemove(tempTypes, moveItemNum);
							for(i = 0; i < F_StrListLen(excludeList); i++){
								s = F_StrListGet(excludeList, i);
								if(F_StrCmp(s, moveItemName ) >= 0)
									break;
							}
							if(i < F_StrListLen(excludeList)){
								F_StrListInsert(excludeList, moveItemName, i);
								F_Free(moveItemName);
								F_ApiSetInt(includeDialogId, excludeScrollBoxId, FP_FirstVis, i);
							}
							else{
								F_StrListAppend(excludeList, moveItemName);
								F_ApiSetInt(includeDialogId, excludeScrollBoxId, FP_FirstVis, F_StrListLen(excludeList) -1);
							}
							moveItemName = NULL;
							apiExcludeList = convertToApiList(excludeList);
							F_ApiSetStrings(includeDialogId, excludeScrollBoxId,  FP_Labels, &apiExcludeList);
							F_ApiDeallocateStrings(&apiExcludeList);
							F_ApiSetInt(includeDialogId, excludeScrollBoxId, FP_State, -1);
							apiIncludeList = convertToApiList(tempTypes);
							F_ApiSetStrings(includeDialogId, includeScrollBoxId,  FP_Labels, &apiIncludeList);
							F_ApiDeallocateStrings(&apiIncludeList);
							F_ApiSetInt(includeDialogId, includeScrollBoxId, FP_State, -1);
						}
					}
					F_Free(moveItemName);
				}
				F_ApiReturnValue(FR_ModalStayUp);
				break;
			case OK:
				// Save the types to the .ini file
				tempText[0] = '\0' ;

				for (i = 0 ; (listName = F_StrListGet(tempTypes, i)) != NULL ; i++)
				{
					F_StrCat(tempText, listName) ;
					F_StrCat(tempText, (StringT)"@!") ;

					F_StrCpy(IncludeTypes[i], listName) ;
					IncludeTypes[i+1][0] = '\0' ;
				}

				WritePrivateProfileString("Options", "IncludeTypes", tempText, "emDEX.ini");

				markerTypes = F_StrListCopyList(tempTypes) ;

				if (updateInRealTime != True)
					break ;

				// Update all open documents
				docId = F_ApiGetId(0, FV_SessionId, FP_FirstOpenDoc);

				while (docId)
				{
					updateDocName(docId) ;

					if (FindWindow(NULL, szWindowTitle) != NULL)
					{
						if (updateAllMarkers(docId) != -1)
						{
							// Update the listbox
							SendMessage(FindWindow(NULL, szWindowTitle), LISTBOX_UPDATE, (WPARAM)0, (LPARAM)0) ;
						}
					}

					docName = F_ApiGetString(docId, docId, FP_Name) ;

					if ((tmpDocId = getContainedInBook(docName)) != 0)
					{
						for (i = 0 ; i < 80 ; i++)
						{
							if (saveBookId[i] == 0)
							{
								saveBookId[i] = tmpDocId ;
								break ;
							}

							if (saveBookId[i] == tmpDocId)
							{
								doProcess = False ;
								break ;
							}
						}

						if (doProcess == True)
						{
							updateDocName(tmpDocId) ;

							if (FindWindow(NULL, szWindowTitle) != NULL)
							{
								if (updateAllMarkers(tmpDocId) == -1)
								{
									if (docName != NULL)
										F_Free(docName) ;

									break ;
								}

								// Update the listbox
								SendMessage(FindWindow(NULL, szWindowTitle), LISTBOX_UPDATE, (WPARAM)0, (LPARAM)0) ;
							}
						}
					}

					if (docName != NULL)
						F_Free(docName) ;

					docId = F_ApiGetId(FV_SessionId, docId,	FP_NextOpenDocInSession);
				}

				break;
		}
	}

	if (dlgNum == RANGE_DIALOG)
	{
//		F_Printf(NULL, (StringT) "Range dlgNum = %d, itemNum = %d\n", dlgNum, itemNum) ;

		switch(itemNum)
		{
			case ADD:
/*				tr = F_ApiGetTextRange(dialogId,rangeTextId, FP_TextSelection);

				F_Printf(NULL, (StringT) "dialogId = %d, rangeTextId = %d, tr.beg.offset = %d\n", dialogId, rangeTextId, tr.beg.offset);

				s = F_ApiGetString(dialogId, rangeTextId, FP_Text);

				F_StrCpyN(tempText, s, tr.beg.offset) ;
				F_StrCat(tempText, (StringT)"<Bold>") ;
				F_StrCat(tempText, &s[i]) ;

				F_ApiSetString(dialogId, rangeTextId, FP_Text, tempText); */

				docId = F_ApiGetId(FV_SessionId, FV_SessionId, FP_ActiveDoc);

				if (docId == 0)
				{
					F_ApiAlert("No insertion point.", FF_ALERT_CONTINUE_NOTE);
					return ;
				}

				tr = F_ApiGetTextRange(FV_SessionId, docId, FP_TextSelection);

			    if ((tr.beg.objId == 0) || (tr.beg.offset == tr.end.offset))
				{
					F_ApiAlert("A text range must be selected before adding a range marker.", FF_ALERT_CONTINUE_NOTE);
				    return;
				}

				rangeText = F_ApiGetString(dialogId, rangeTextId, FP_Text);
				F_StrStripLeadingSpaces(rangeText) ;

				if (F_StrLen(rangeText) > 255)
				{
					F_ApiAlert("Marker text is too long. Marker text is limited to 255 characters.", FF_ALERT_CONTINUE_NOTE);
					return ;
				}

				if ((rangeText == NULL) || (F_StrLen(rangeText) == 0))
					return ;

				rangeTypeName = getSelectedLabel(dialogId, INCLUDE_BOX);

				if ((rangeTypeName == NULL) || (F_StrLen(rangeTypeName) == 0))
				{
					F_ApiAlert("No marker type selected.", FF_ALERT_CONTINUE_NOTE);
					return ;
				}

				if (VERSION == DEMO)
				{
					if (demoCount > 10)
					{
						F_ApiAlert((StringT) "The demo version is limited to 10 marker edits per session.\n\nYou can order the full version at www.emdex.ca", FF_ALERT_CONTINUE_WARN);
						return ;
					}
					else
					{
						demoCount++ ;
					}
				}

				tr = F_ApiGetTextRange(FV_SessionId, docId, FP_TextSelection);

				if ((tr.beg.objId == 0) || (tr.beg.offset == tr.end.offset))
					loc = tr.beg = tr.end;
				else
					loc = tr.beg ;

				// Insert the markers at the beginning and end of the text selection.
				loc = tr.beg ;

				strElementDefaultType[0] = '\0' ;

				if ((F_StrCmp(productName, (StringT)"FrameMaker+SGML") == 0) &&
					(getSelectedLabelIndex(dialogId, ELEMENTTAG_BOX) > 1))
				{
					// Structured marker
					tr = F_ApiGetTextRange(FV_SessionId, docId, FP_TextSelection);
//					tr.end.offset += 1 ;
//					edefId = F_ApiGetNamedObject(docId, FO_ElementDef, (StringT)"IndexTerm");
					F_StrCpy(strElementDefaultType, (StringT)getSelectedLabel(dialogId, ELEMENTTAG_BOX)) ;
					edefId = F_ApiGetNamedObject(docId, FO_ElementDef, (StringT)getSelectedLabel(dialogId, ELEMENTTAG_BOX));
					elemId = F_ApiNewElement(docId, edefId, &tr.beg);

					textItems = F_ApiGetTextForRange(docId, &tr, FTI_MarkerAnchor);
//					F_Printf(NULL, (StringT) "beg textitems.len = %d\n", textItems.len) ;
					if (textItems.len > 0)
						markerId = textItems.val[0].u.idata ;

//					F_Printf(NULL, (StringT) "start id = %d\n", markerId) ;

					markerTypeId = F_ApiGetNamedObject(docId, FO_MarkerType, rangeTypeName);
					F_ApiSetId(docId, markerId, FP_MarkerTypeId, markerTypeId);
				}
				else
				{
					markerId = InsertMarker(docId, loc, rangeTypeName);
				}

				F_Sprintf(tempMarkerName, "<$startrange>%s", rangeText) ;

				F_ApiSetString(docId, markerId, FP_MarkerText, tempMarkerName);

//				tr.end.offset++ ;
				loc = tr.end ;

				if ((F_StrCmp(productName, (StringT)"FrameMaker+SGML") == 0) &&
					(getSelectedLabelIndex(dialogId, ELEMENTTAG_BOX) > 1))
				{
					// Structured marker
					tr = F_ApiGetTextRange(FV_SessionId, docId, FP_TextSelection);
//					edefId = F_ApiGetNamedObject(docId, FO_ElementDef, (StringT)"IndexTerm");
					edefId = F_ApiGetNamedObject(docId, FO_ElementDef, (StringT)getSelectedLabel(dialogId, ELEMENTTAG_BOX));
					elemId = F_ApiNewElement(docId, edefId, &tr.end);

					tr.beg = tr.end;
					tr.beg.offset-- ;
					tr.end.offset++ ;

					textItems = F_ApiGetTextForRange(docId, &tr, FTI_MarkerAnchor);
//					F_Printf(NULL, (StringT) "end textitems.len = %d\n", textItems.len) ;

					if (textItems.len > 0)
						markerId = textItems.val[0].u.idata ;

//					F_Printf(NULL, (StringT) "end id = %d\n", markerId) ;

					markerTypeId = F_ApiGetNamedObject(docId, FO_MarkerType, rangeTypeName);
					F_ApiSetId(docId, markerId, FP_MarkerTypeId, markerTypeId);
				}
				else
				{
					markerId = InsertMarker(docId, loc, rangeTypeName);
				}

				F_Sprintf(tempMarkerName, "<$endrange>%s", rangeText) ;

				F_ApiSetString(docId, markerId, FP_MarkerText, tempMarkerName);

				// Add text into the autocomplete list
				addAutoCompleteText(rangeText) ;

				// Update the catcher window.
				rangeText = NULL ;
				F_Free(rangeText);
			
				doUpdate(F_ApiGetId(FV_SessionId, FV_SessionId, FP_ActiveDoc)) ;

				g_CurrentEditor = 0 ;

				defaultType = getSelectedLabelIndex(dialogId, INCLUDE_BOX) ;

				F_ApiClose(dialogId, 0) ;

				break ;

			case 11 :
			case 12 :
			case 13 :
			case 14 :
			case 15 :
			case 16 :
			case 17 :
			case 18 :
			case 19 :
			case 20 :
				hEditWindow = FindWindow(NULL, "emDEX Range Marker") ;
				hTextEntry = GetDlgItem(hEditWindow, 101) ;

				for (i = 0 ; i < F_StrLen(buttons[itemNum-11].action) ; i++)
					SendMessage(hTextEntry, WM_CHAR, (WPARAM)toascii(buttons[itemNum-11].action[i]), (LPARAM)0) ;

				SetFocus(hTextEntry) ;
				SetForegroundWindow(hTextEntry) ;

				F_ApiReturnValue(FR_DialogStayUp) ;

				break ;

			case INCLUDE_BOX :
				rangeTypeName = getSelectedLabel(dialogId, INCLUDE_BOX);

				if (F_StrCmp(rangeTypeName, "Edit ...") == 0)
				{
					if ((F_ApiGetId(0, FV_SessionId, FP_FirstOpenDoc) == 0) && 
						(F_ApiGetId(0, FV_SessionId, FP_FirstOpenBook) == 0))
					{
						F_ApiAlert((StringT) "No current document", FF_ALERT_CONTINUE_WARN);
					}
					else
					{
						// Bring up the types edit box.
						fcodes[0] = KBD_EDITMARKERTYPE ;
						F_ApiFcodes(sizeof(fcodes)/sizeof(IntT), fcodes);

						docId = F_ApiGetId(FV_SessionId, FV_SessionId, FP_ActiveDoc);
						doUpdate(docId) ;

						setEditorText() ;
					}

					findAndSelectDefaultType(docId, dialogId, INCLUDE_BOX) ;
				}

				break ;

			case ELEMENTTAG_BOX :
				elementName = getSelectedLabel(dialogId, ELEMENTTAG_BOX);

//				F_Printf(NULL, (StringT) "elementName = %s\n", elementName);

				// Set the appropriate marker type.
				if (F_StrICmp(elementName, "IndexTerm") == 0)
				{
					findStringAndSelectLabel(docId, dialogId, INCLUDE_BOX, "Index") ;
				}

				if (F_StrICmp(elementName, "Keyword") == 0)
				{
					findStringAndSelectLabel(docId, dialogId, INCLUDE_BOX, "Keyword") ;
				}

				if (F_StrICmp(elementName, "Remark") == 0)
				{
					findStringAndSelectLabel(docId, dialogId, INCLUDE_BOX, "Comment") ;
				}

				if (F_StrICmp(elementName, "SubjectTerm") == 0)
				{
					findStringAndSelectLabel(docId, dialogId, INCLUDE_BOX, "Subject") ;
				}

				if (F_StrICmp(elementName, "Comment") == 0)
				{
					findStringAndSelectLabel(docId, dialogId, INCLUDE_BOX, "Comment") ;
				}

				if (F_StrICmp(elementName, "Title") == 0)
				{
					findStringAndSelectLabel(docId, dialogId, INCLUDE_BOX, "Header/Footer $1") ;
				}

				break ;

			case 1:
				// Text entry box
				if (g_autoComplete == 0)
					break ;

				// Text being entered, try to autocomplete
				if (completeCount == 1)
				{
					// Only test alternately - this fixes loop problems.
					completeCount = 0 ;
					break ;
				}

				hEditWindow = FindWindow(NULL, "emDEX Range Marker") ;
				hTextEntry = GetDlgItem(hEditWindow, 101) ;

				SendMessage(hTextEntry, EM_GETSEL, (WPARAM)&start, (LPARAM)&end) ;

				docId = F_ApiGetId(FV_SessionId, FV_SessionId, FP_ActiveDoc);

				rangeText = F_ApiGetString(dialogId, rangeTextId, FP_Text);

//				F_Printf(NULL, (StringT) "rangeText = %s, start = %d, end = %d\n", rangeText, start, end);

				for (i = start-1 ; i > 0 ; i--)
				{
					if (rangeText[i] == ' ')
						break ;
				}

				for (j = start ; j < F_StrLen(rangeText) ; j++)
				{
					if (rangeText[j] == ' ')
						break ;
				}

				j++ ;

				if (i > 0)
					i++ ;

				F_StrCpyN(tempText, &rangeText[i], j-i) ;

//				F_Printf(NULL, (StringT) "rangeText = %s, start = %d, end = %d, i = %d, j = %d, tempText = %s\n", rangeText, start, end, i, j, tempText);

				for (k = 0 ; autoComplete[k] [0] != '\0' ; k++)
				{
					if ((F_StrLen(tempText) > 2) && (F_StrICmpN(tempText, autoComplete[k], F_StrLen(tempText)) == 0) &&
						(F_StrICmp(tempText, autoComplete[k]) != 0))
					{
						completeCount = 1 ;

						F_StrCpyN(autoCompleteText, rangeText, j) ;
						F_StrCat(autoCompleteText, &autoComplete[k] [F_StrLen(tempText)]) ;
						F_StrCat(autoCompleteText, &rangeText[j-1]) ;

//						F_Printf(NULL, (StringT) "autoCompleteText = %s\n", autoCompleteText);
						F_ApiSetString(dialogId, rangeTextId, FP_Text, autoCompleteText);
						SendMessage(hTextEntry, EM_SETSEL, (WPARAM)j + (F_StrLen(autoComplete[k]) - F_StrLen(tempText)) - 1, (LPARAM)end) ;

						break ;
					}
				}

				break ;

			case FV_DlgClose :
				// Save the position of the editor window
				GetWindowRect(FindWindow(NULL, "emDEX Range Marker"), &winPos) ;

				ltoa(winPos.left, SaveOption, 10) ;
				WritePrivateProfileString("Options", "EditorXPos", SaveOption, "emDEX.ini");
				ltoa(winPos.top, SaveOption, 10) ;
				WritePrivateProfileString("Options", "EditorYPos", SaveOption, "emDEX.ini");

				g_CurrentEditor = 0 ;
				F_ApiNotification(FA_Note_BackToUser, False);
				break ;

			default :
				F_ApiReturnValue(FR_DialogStayUp) ;

				break ;
		}
	}

	if (dlgNum == MARKER_DIALOG)
	{
		switch(itemNum)
		{
			case ADD:
				docId = F_ApiGetId(FV_SessionId, FV_SessionId, FP_ActiveDoc);

				if (docId == 0)
				{
					F_ApiAlert("No insertion point.", FF_ALERT_CONTINUE_NOTE);
					return ;
				}

				tr = F_ApiGetTextRange(FV_SessionId, docId, FP_TextSelection);
				if (tr.beg.objId == 0)
				{
					F_ApiAlert("No insertion point.", FF_ALERT_CONTINUE_NOTE);
					return ;
				}

				rangeText = F_ApiGetString(dialogId, rangeTextId, FP_Text);
				F_StrStripLeadingSpaces(rangeText) ;

				if (F_StrLen(rangeText) > 255)
				{
					F_ApiAlert("Marker text is too long. Marker text is limited to 255 characters.", FF_ALERT_CONTINUE_NOTE);
					return ;
				}

				rangeTypeName = getSelectedLabel(dialogId, INCLUDE_BOX);

				if ((rangeTypeName == NULL) || (F_StrLen(rangeTypeName) == 0))
				{
					F_ApiAlert("No marker type selected.", FF_ALERT_CONTINUE_NOTE);
					return ;
				}

				if (VERSION == DEMO)
				{
					if (demoCount > 10)
					{
						F_ApiAlert((StringT) "The demo version is limited to 10 marker edits per session.\n\nYou can order the full version at www.emdex.ca", FF_ALERT_CONTINUE_WARN);
						return ;
					}
					else
					{
						demoCount++ ;
					}
				}

				tr = F_ApiGetTextRange(FV_SessionId, docId, FP_TextSelection);

				if ((tr.beg.objId == 0) || (tr.beg.offset == tr.end.offset))
					loc = tr.beg = tr.end;
				else
					loc = tr.beg ;

				strElementDefaultType[0] = '\0' ;

				if (g_currentMarkerId == 0)
				{
					if ((F_StrCmp(productName, (StringT)"FrameMaker+SGML") == 0) &&
						(getSelectedLabelIndex(dialogId, ELEMENTTAG_BOX) > 1))
					{
						// Structured marker
						tr = F_ApiGetTextRange(FV_SessionId, docId, FP_TextSelection);
						tr.end.offset += 1 ;
//						edefId = F_ApiGetNamedObject(docId, FO_ElementDef, (StringT)"IndexTerm");
						F_StrCpy(strElementDefaultType, (StringT)getSelectedLabel(dialogId, ELEMENTTAG_BOX)) ;
						edefId = F_ApiGetNamedObject(docId, FO_ElementDef, (StringT)getSelectedLabel(dialogId, ELEMENTTAG_BOX));
						elemId = F_ApiNewElement(docId, edefId, &tr.beg);

						textItems = F_ApiGetTextForRange(docId, &tr, FTI_MarkerAnchor);
						//F_Printf(NULL, (StringT) "textitems.len = %d\n", textItems.len) ;
						if (textItems.len > 0)
							markerId = textItems.val[0].u.idata ;

						markerTypeId = F_ApiGetNamedObject(docId, FO_MarkerType, rangeTypeName);
						F_ApiSetId(docId, markerId, FP_MarkerTypeId, markerTypeId);
					}
					else
					{
						// Unstructured marker
						markerId = InsertMarker(docId, loc, rangeTypeName);
					}
				}
				else
				{
					markerId = g_currentMarkerId ;

					markerTypeId = F_ApiGetNamedObject(docId, FO_MarkerType, rangeTypeName);
					F_ApiSetId(docId, markerId, FP_MarkerTypeId, markerTypeId);
				}

				F_ApiSetString(docId, markerId, FP_MarkerText, rangeText);

				if (g_currentMarkerId == 0)
				{
					F_ApiSetString(dialogId, rangeTextId, FP_Text, "");

					F_StrCpy(g_markerName, "") ;
					g_currentMarkerId = 0 ;
				}

				// Add text into the autocomplete list
				addAutoCompleteText(rangeText) ;

				rangeText = NULL ;
				F_Free(rangeText);

				defaultType = getSelectedLabelIndex(dialogId, INCLUDE_BOX) ;
//				setSelectedLabelIndex(dialogId, ELEMENTTAG_BOX, 1) ;
			
				doUpdate(F_ApiGetId(FV_SessionId, FV_SessionId, FP_ActiveDoc)) ;

				if (g_currentMarkerId == 0)
				{
					// Clear the range selection
					tr.end = tr.beg ;
					F_ApiSetTextRange(FV_SessionId, docId, FP_TextSelection, &tr);
				}

				hEditWindow = FindWindow(NULL, "emDEX Marker") ;
				hTextEntry = GetDlgItem(hEditWindow, 101) ;
				SetFocus(hTextEntry) ;
				SetForegroundWindow(hTextEntry) ;

				break ;

			case 11 :
			case 12 :
			case 13 :
			case 14 :
			case 15 :
			case 16 :
			case 17 :
			case 18 :
			case 19 :
			case 20 :
				hEditWindow = FindWindow(NULL, "emDEX Marker") ;
				hTextEntry = GetDlgItem(hEditWindow, 101) ;

				for (i = 0 ; i < F_StrLen(buttons[itemNum-11].action) ; i++)
					SendMessage(hTextEntry, WM_CHAR, (WPARAM)toascii(buttons[itemNum-11].action[i]), (LPARAM)0) ;
				
				SetFocus(hTextEntry) ;
				SetForegroundWindow(hTextEntry) ;

				F_ApiReturnValue(FR_DialogStayUp) ;

				break ;

			case 3:
				// Delete button
				if (VERSION == DEMO)
				{
					if (demoCount > 10)
					{
						F_ApiAlert((StringT) "The demo version is limited to 10 marker edits per session.\n\nYou can order the full version at www.emdex.ca", FF_ALERT_CONTINUE_WARN);
						return ;
					}
					else
					{
						demoCount++ ;
					}
				}

				hEditWindow = FindWindow(NULL, "emDEX Marker") ;
				hTextEntry = GetDlgItem(hEditWindow, 101) ;

				docId = F_ApiGetId(FV_SessionId, FV_SessionId, FP_ActiveDoc);

				// Check if contained within an inset
				if (F_ApiGetId(docId, docId, FP_FirstSelectedTiInDoc) != 0)
				{
					SendMessage(hTextEntry, EM_GETSEL, (WPARAM)&start, (LPARAM)&end) ;

					if (F_ApiAlert((StringT)"If the marker is contained within the highlighted text inset,\ndeleting will remove the entire inset.\n\nContinue ?", FF_ALERT_NO_DEFAULT) == -1)
					{
						SetFocus(hTextEntry) ;
						SetForegroundWindow(hTextEntry) ;
						SendMessage(hTextEntry, EM_SETSEL, (WPARAM)start, (LPARAM)start) ;
						break ;
					}
				}

				F_ApiDelete(docId, g_currentMarkerId);

				doUpdate(docId) ;

				SetFocus(hTextEntry) ;
				SetForegroundWindow(hTextEntry) ;

				g_currentMarkerId = 0 ;
				F_StrCpy(g_markerName, "") ;

				setEditorText() ;

				break ;

			case 1:
				// Text entry box
				if (g_autoComplete == 0)
					break ;

				// Text being entered, try to autocomplete
				if (completeCount == 1)
				{
					// Only test alternately - this fixes loop problems.
					completeCount = 0 ;
					break ;
				}

				hEditWindow = FindWindow(NULL, "emDEX Marker") ;
				hTextEntry = GetDlgItem(hEditWindow, 101) ;

				SendMessage(hTextEntry, EM_GETSEL, (WPARAM)&start, (LPARAM)&end) ;

				docId = F_ApiGetId(FV_SessionId, FV_SessionId, FP_ActiveDoc);

				rangeText = F_ApiGetString(dialogId, rangeTextId, FP_Text);

//				F_Printf(NULL, (StringT) "rangeText = %s, start = %d, end = %d\n", rangeText, start, end);

				for (i = start-1 ; i > 0 ; i--)
				{
					if (rangeText[i] == ' ')
						break ;
				}

				for (j = start ; j < F_StrLen(rangeText) ; j++)
				{
					if (rangeText[j] == ' ')
						break ;
				}

				j++ ;

				if (i > 0)
					i++ ;

				F_StrCpyN(tempText, &rangeText[i], j-i) ;

//				F_Printf(NULL, (StringT) "rangeText = %s, start = %d, end = %d, i = %d, j = %d, tempText = %s\n", rangeText, start, end, i, j, tempText);

				for (k = 0 ; autoComplete[k] [0] != '\0' ; k++)
				{
					if ((F_StrLen(tempText) > 2) && (F_StrICmpN(tempText, autoComplete[k], F_StrLen(tempText)) == 0) &&
						(F_StrICmp(tempText, autoComplete[k]) != 0))
					{
						completeCount = 1 ;

						F_StrCpyN(autoCompleteText, rangeText, j) ;
						F_StrCat(autoCompleteText, &autoComplete[k] [F_StrLen(tempText)]) ;
						F_StrCat(autoCompleteText, &rangeText[j-1]) ;

//						F_Printf(NULL, (StringT) "autoCompleteText = %s\n", autoCompleteText);
						F_ApiSetString(dialogId, rangeTextId, FP_Text, autoCompleteText);
						SendMessage(hTextEntry, EM_SETSEL, (WPARAM)j + (F_StrLen(autoComplete[k]) - F_StrLen(tempText)) - 1, (LPARAM)end) ;

						break ;
					}
				}

				break ;

			case INCLUDE_BOX :
				rangeTypeName = getSelectedLabel(dialogId, INCLUDE_BOX);

				if (F_StrCmp(rangeTypeName, "Edit ...") == 0)
				{
					if ((F_ApiGetId(0, FV_SessionId, FP_FirstOpenDoc) == 0) && 
						(F_ApiGetId(0, FV_SessionId, FP_FirstOpenBook) == 0))
					{
						F_ApiAlert((StringT) "No current document", FF_ALERT_CONTINUE_WARN);
					}
					else
					{
						// Bring up the types edit box.
						fcodes[0] = KBD_EDITMARKERTYPE ;
						F_ApiFcodes(sizeof(fcodes)/sizeof(IntT), fcodes);

						docId = F_ApiGetId(FV_SessionId, FV_SessionId, FP_ActiveDoc);
						doUpdate(docId) ;

						setEditorText() ;
					}

					findAndSelectDefaultType(docId, dialogId, INCLUDE_BOX) ;
				}

				break ;

			case ELEMENTTAG_BOX :
				elementName = getSelectedLabel(dialogId, ELEMENTTAG_BOX);

//				F_Printf(NULL, (StringT) "elementName = %s\n", elementName);

				// Set the appropriate marker type.
				if (F_StrICmp(elementName, "IndexTerm") == 0)
				{
					findStringAndSelectLabel(docId, dialogId, INCLUDE_BOX, "Index") ;
				}

				if (F_StrICmp(elementName, "Keyword") == 0)
				{
					findStringAndSelectLabel(docId, dialogId, INCLUDE_BOX, "Keyword") ;
				}

				if (F_StrICmp(elementName, "Remark") == 0)
				{
					findStringAndSelectLabel(docId, dialogId, INCLUDE_BOX, "Comment") ;
				}

				if (F_StrICmp(elementName, "SubjectTerm") == 0)
				{
					findStringAndSelectLabel(docId, dialogId, INCLUDE_BOX, "Subject") ;
				}

				if (F_StrICmp(elementName, "Comment") == 0)
				{
					findStringAndSelectLabel(docId, dialogId, INCLUDE_BOX, "Comment") ;
				}

				if (F_StrICmp(elementName, "Title") == 0)
				{
					findStringAndSelectLabel(docId, dialogId, INCLUDE_BOX, "Header/Footer $1") ;
				}

				break ;

			case FV_DlgClose :
				// Save the position of the editor window
				GetWindowRect(FindWindow(NULL, "emDEX Marker"), &winPos) ;

				ltoa(winPos.left, SaveOption, 10) ;
				WritePrivateProfileString("Options", "EditorXPos", SaveOption, "emDEX.ini");
				ltoa(winPos.top, SaveOption, 10) ;
				WritePrivateProfileString("Options", "EditorYPos", SaveOption, "emDEX.ini");

				g_CurrentEditor = 0 ;
				F_ApiNotification(FA_Note_BackToUser, False);
				break ;

			default :
				F_ApiReturnValue(FR_DialogStayUp) ;

				break ;
		}
	}
}

F_StringsT  convertToApiList(fdeList)
StringListT fdeList;{
	IntT numFmts;
	F_StringsT list;
	IntT i;
	
	numFmts = F_StrListLen(fdeList);
	list.val = (StringT *) F_Alloc(numFmts*sizeof(StringT), NO_DSE);
	if(list.val)
	{
		list.len = numFmts;
		for(i =0; i < numFmts; i++)
		{
			list.val[i] = F_StrNew(MAXSTRING + 1);
			F_StrCpy(list.val[i], F_StrListGet(fdeList,i));
		}
	}
	else
	{
		F_ApiAlert((StringT) "Unable to allocate memory for format list", FF_ALERT_CONTINUE_WARN);
		list.val = NULL;
	}
	return(list);
}

StringListT getListOfTypes(docId)
F_ObjHandleT docId;
{
	F_ObjHandleT fmtId;
	StringListT fmtList;
	StringT fmtName, listName ;
	IntT i = 0 ;
	BoolT included ;
	
	fmtList = F_StrListNew((UIntT) 1, (UIntT)1);
	fmtId = F_ApiGetId(FV_SessionId, docId, FP_FirstMarkerTypeInDoc);
	while(fmtId)
	{
		included = False ;

		fmtName = F_ApiGetString(docId, fmtId, FP_Name);

		for (i = 0 ; i < F_StrListLen(markerTypes) ; i++)
		{
			listName = F_StrListGet(markerTypes, i) ;

			if (F_StrCmp(fmtName, listName) == 0)
			{
				included = True ;
				break ;
			}
		}

		if (included == False)
			F_StrListAppend(fmtList, fmtName);

		F_Free(fmtName);
		fmtId = F_ApiGetId(docId, fmtId, FP_NextMarkerTypeInDoc);
	}

	F_StrListSort(fmtList, &fn);

	return(fmtList);
}

StringListT getListOfAllTypes(docId)
F_ObjHandleT docId;
{
	F_ObjHandleT fmtId;
	StringListT fmtList;
	StringT fmtName, listName ;
	IntT i = 0 ;
	
	fmtList = F_StrListNew((UIntT) 1, (UIntT)1);
	fmtId = F_ApiGetId(FV_SessionId, docId, FP_FirstMarkerTypeInDoc);

	while(fmtId)
	{
		fmtName = F_ApiGetString(docId, fmtId, FP_Name);

		F_StrListAppend(fmtList, fmtName);

		F_Free(fmtName);
		fmtId = F_ApiGetId(docId, fmtId, FP_NextMarkerTypeInDoc);
	}

	F_StrListSort(fmtList, &fn);

	return(fmtList);
}

StringT getSelectedLabel(dialogId, itemNum)
F_ObjHandleT dialogId;
IntT itemNum;{
	IntT index;
	F_StringsT strings;
	StringT s;
	F_ObjHandleT itemId = F_ApiDialogItemId(dialogId, itemNum);
	UIntT type = F_ApiGetObjectType(dialogId, itemId);
	
	if(!dialogId || !itemId || (type != FO_DlgPopUp && type != FO_DlgScrollBox && type != FO_DlgImage))
	{
		FA_errno = FE_BadParameter;
		return NULL;
	}
	index = F_ApiGetInt(dialogId, itemId, FP_State);
	strings = F_ApiGetStrings(dialogId, itemId, FP_Labels);
	if(!strings.len || !strings.val || index < 0)
		return NULL;
	s = F_StrCopyString(strings.val[index]);
	F_ApiDeallocateStrings(&strings);
	return s;
}

/* Returns the index of the selected label on the popup menu or scrollbox.
 *	If nothing is selected, -1 is returned.*/
IntT getSelectedLabelIndex(dialogId, itemNum)
F_ObjHandleT dialogId;
IntT itemNum;
{
	IntT index;
	F_StringsT strings;
	StringT s ;

	F_ObjHandleT itemId = F_ApiDialogItemId(dialogId, itemNum);
	UIntT type = F_ApiGetObjectType(dialogId, itemId);
	
	if(!dialogId || !itemId || (type != FO_DlgPopUp && type != FO_DlgScrollBox && type != FO_DlgImage))
	{
		FA_errno = FE_BadParameter;
		return 0;
	}

	index = F_ApiGetInt(dialogId, itemId, FP_State);

	strings = F_ApiGetStrings(dialogId, itemId, FP_Labels);

	if (strings.val)
		F_StrCpy(strDefaultType, strings.val[index]) ;

	return (index) ;
}

VoidT setSelectedLabelIndex(dialogId, itemNum, index)
F_ObjHandleT dialogId;
IntT itemNum;
IntT index;
{
	F_ObjHandleT itemId = F_ApiDialogItemId(dialogId, itemNum);
	UIntT type = F_ApiGetObjectType(dialogId, itemId);
	
	if(!dialogId || !itemId || (type != FO_DlgPopUp && type != FO_DlgScrollBox && type != FO_DlgImage))
	{
		FA_errno = FE_BadParameter;
		return ;
	}

	F_ApiSetInt(dialogId, itemId, FP_State, index);
	F_ApiSetInt(dialogId, itemId, FP_FirstVis, index);
}

VoidT findAndSelectLabel(docId, dialogId, itemNum, typeId)
F_ObjHandleT docId;
F_ObjHandleT dialogId;
IntT itemNum;
F_ObjHandleT typeId;{
	IntT i;
	F_StringsT strings;
	StringT s, fmtName;

	F_ObjHandleT itemId = F_ApiDialogItemId(dialogId, itemNum);

	if (!docId || !typeId)
		return ;

	fmtName = F_ApiGetString(docId, typeId, FP_Name);

	strings = F_ApiGetStrings(dialogId, itemId, FP_Labels);
	if(!strings.len || !strings.val)
	{
		if (fmtName != NULL)
			F_Free(fmtName) ;

		return;
	}
	
	for (i = 0 ; i < strings.len ; i++)
	{
		if (F_StrCmp(strings.val[i], fmtName) == 0)
			break ;
	}

	if (i == strings.len)
	{
		F_ApiDeallocateStrings(&strings);
		if (fmtName != NULL)
			F_Free(fmtName) ;

		return ;
	}

	F_ApiDeallocateStrings(&strings);
	if (fmtName != NULL)
		F_Free(fmtName) ;

	setSelectedLabelIndex(dialogId, itemNum, i) ;
}

VoidT findStringAndSelectLabel(docId, dialogId, itemNum, text)
F_ObjHandleT docId;
F_ObjHandleT dialogId;
IntT itemNum;
StringT text;
{
	IntT i;
	F_StringsT strings;

	F_ObjHandleT itemId = F_ApiDialogItemId(dialogId, itemNum);

	if (!docId)
		return ;

	strings = F_ApiGetStrings(dialogId, itemId, FP_Labels);
	if(!strings.len || !strings.val)
	{
		return;
	}

	for (i = 0 ; i < strings.len ; i++)
	{
		if (F_StrICmp(strings.val[i], text) == 0)
			break ;
	}

	if (i == strings.len)
	{
		F_ApiDeallocateStrings(&strings);

		return ;
	}

	F_ApiDeallocateStrings(&strings);

	setSelectedLabelIndex(dialogId, itemNum, i) ;
}

IntT getDefaultSelection(dialogId, itemNum)
F_ObjHandleT dialogId;
IntT itemNum;{
	IntT index;
	F_StringsT strings;
	StringT s;
	F_ObjHandleT itemId = F_ApiDialogItemId(dialogId, itemNum);
	UIntT type = F_ApiGetObjectType(dialogId, itemId);

	F_StrCpy(strDefaultType, "") ;
	
	if(!dialogId || !itemId || (type != FO_DlgPopUp && type != FO_DlgScrollBox && type != FO_DlgImage))
	{
		FA_errno = FE_BadParameter;
		return (-1) ;
	}

	strings = F_ApiGetStrings(dialogId, itemId, FP_Labels);
	if(!strings.len || !strings.val)
		return (-1) ;

	for (index = 0 ; strings.val[index] ; index++)
	{
		if (F_StrCmp(strings.val[index], "Index") == 0)
		{
			F_StrCpy(strDefaultType, strings.val[index]) ;
			break ;
		}
	}

	F_ApiDeallocateStrings(&strings);
	return (index);
}

VoidT findAndSelectDefaultType(docId, dialogId, itemNum)
F_ObjHandleT docId;
F_ObjHandleT dialogId;
IntT itemNum;
{
	IntT i;
	F_StringsT strings;
	StringT s, fmtName;

	F_ObjHandleT itemId = F_ApiDialogItemId(dialogId, itemNum);

/*	if (!docId)
		return ; */

	strings = F_ApiGetStrings(dialogId, itemId, FP_Labels);
	if(!strings.len || !strings.val)
	{
		return;
	}
	
	for (i = 0 ; i < strings.len ; i++)
	{
		if (F_StrCmp(strings.val[i], strDefaultType) == 0)
			break ;
	}

	if (i == strings.len)
	{
		// Didn't find it.
		setSelectedLabelIndex(dialogId, itemNum, getDefaultSelection(dialogId, itemNum)) ;

		F_ApiDeallocateStrings(&strings);

		return ;
	}

	F_ApiDeallocateStrings(&strings);

	setSelectedLabelIndex(dialogId, itemNum, i) ;
}

VoidT findAndSelectDefaultElement(docId, dialogId, itemNum)
F_ObjHandleT docId;
F_ObjHandleT dialogId;
IntT itemNum;
{
	IntT i;
	F_StringsT strings;
	StringT s, fmtName;

	F_ObjHandleT itemId = F_ApiDialogItemId(dialogId, itemNum);

/*	if (!docId)
		return ; */

	strings = F_ApiGetStrings(dialogId, itemId, FP_Labels);
	if(!strings.len || !strings.val)
	{
		return;
	}
	
	for (i = 0 ; i < strings.len ; i++)
	{
		if (F_StrCmp(strings.val[i], strElementDefaultType) == 0)
			break ;
	}

	if (i == strings.len)
	{
		// Didn't find it.
		setSelectedLabelIndex(dialogId, itemNum, 1) ;

		F_ApiDeallocateStrings(&strings);

		return ;
	}

	F_ApiDeallocateStrings(&strings);

	setSelectedLabelIndex(dialogId, itemNum, i) ;
}

/* Return the id of a marker's page */
F_ObjHandleT getMarkerPageId(docId, markerId)
F_ObjHandleT docId, markerId;
{
	F_PropValsT markerProps;
	F_ObjHandleT id, pageId, pageId2, frameId, pageFrameId, objId;
	UIntT i;
	F_TextLocT tl;
	F_TextRangeT tr;
	StringT	theName;
	IntT	pageNum;

	markerProps = F_ApiGetProps(docId, markerId);
	for (i = 0; i < markerProps.len; i++)
	{
		if (markerProps.val[i].propIdent.num == FP_TextLoc)
			break;
	}
	if (i >= markerProps.len)
		return(0);
	id = markerProps.val[i].propVal.u.tlval.objId;
	F_ApiDeallocatePropVals(&markerProps);

	id = F_ApiGetId(docId, id, FP_InTextObj);
	pageId = getPageId(docId, id);

/*	tl = F_ApiGetTextLoc(docId, id, FP_TextLoc); 
	tr.end.objId = tr.beg.objId = tl.objId;
	tr.end.offset = tr.beg.offset = tl.offset;

	F_ApiSetTextRange(FV_SessionId, docId, FP_TextSelection, &tr);

	pageId2 = F_ApiGetId(FV_SessionId,docId,FP_TextSelection);

	F_Printf(NULL, (StringT) "pageId = %d, pageId2 = %d\n", pageId, pageId2); */

	return (pageId) ;
}

F_ObjHandleT getPageId(docId, graphicId)
F_ObjHandleT docId, graphicId;
{
	F_ObjHandleT parentId, oldParentId, parentType;

	/* Ascend until the page frame is reached */
	parentId = graphicId;
	parentType = F_ApiGetObjectType(docId, graphicId);
	while (parentType != FO_BodyPage &&
			parentType != FO_MasterPage &&
			parentType != FO_RefPage)
	{
		oldParentId = parentId;
		if (parentType == FO_Cell || parentType == FO_Fn)
			/* For table cell text columns and footnotes, get the parent text column */
			parentId = F_ApiGetId(docId,parentId,FP_InTextObj);
		else
		{
			parentId = F_ApiGetId(docId,parentId,FP_FrameParent);
			/* If no parent, this must be a page frame or an unanchored frame */
			if (!parentId)
				parentId = F_ApiGetId(docId, oldParentId, FP_PageFramePage);
		}
		/* Safety check */
		if (!parentId)
			break;
		parentType = F_ApiGetObjectType(docId,parentId);
	}
	return(parentId);
}

VoidT loadAutoCompleteText(F_ObjHandleT docId)
{
	F_ObjHandleT markerId;
	StringT markerName ;
	UCharT tempString[800] ;
	IntT i, j, k, autoCount ;

	// Get all markers and update the catcher.
	markerId = F_ApiGetId(FV_SessionId, docId, FP_FirstMarkerInDoc);

	if (markerId == 0)
	{
		return ;
	}

	for (autoCount = 0 ; autoComplete[autoCount] [0] != '\0' ; autoCount++) ;

	while(markerId)
	{
		markerName = F_ApiGetString(docId, markerId, FP_MarkerText);

		k = 0 ;

		for (i = 0 ; i < F_StrLen(markerName) ; i++)
		{
			if (!isalnum(markerName[i]))
			{
				if (tempString[0] != '\0')
				{
					if (F_StrLen(tempString) > 4)
					{
						if (autoCount >= ((MAXITEMS * 2) - 1))
						{
							autoComplete[(MAXITEMS * 2) - 1] [0] = '\0' ;
							return ;
						}

						F_StrCpy(autoComplete[autoCount], tempString) ;
//						F_Printf(NULL, (StringT) "autoComplete[%d] = %s\n", autoCount, autoComplete[autoCount]);
						autoCount++ ;
						autoComplete[autoCount+1] [0] = '\0' ;
					}
				}

				j = 0 ;
				k = 0 ;
				tempString[0] = '\0' ;
			}
			else
			{
				tempString[k] = markerName[i] ;
				k++ ;
				tempString[k] = '\0' ;
			}
		}

		F_Free(markerName) ;

		markerId  = F_ApiGetId(docId, markerId, FP_NextMarkerInDoc);
	}

	return ;
}

VoidT addAutoCompleteText(StringT inString)
{
	UCharT tempString[800] ;
	IntT i, j, k, autoCount ;

	if ((inString[0] == '\0') || (F_StrLen(inString) == 0))
		return ;

	for (autoCount = 0 ; autoComplete[autoCount] [0] != '\0' ; autoCount++) ;

	k = 0 ;

	for (i = 0 ; i < F_StrLen(inString) ; i++)
	{
		if (!isalnum(inString[i]))
		{
			if (tempString[0] != '\0')
			{
				if (F_StrLen(tempString) > 4)
				{
					if (autoCount >= ((MAXITEMS * 2) - 1))
					{
						autoComplete[(MAXITEMS * 2) - 1] [0] = '\0' ;
						return ;
					}

					F_StrCpy(autoComplete[autoCount], tempString) ;
//					F_Printf(NULL, (StringT) "autoComplete[%d] = %s\n", autoCount, autoComplete[autoCount]);
					autoCount++ ;
					autoComplete[autoCount+1] [0] = '\0' ;
				}
			}

			j = 0 ;
			k = 0 ;
			tempString[0] = '\0' ;
		}
		else
		{
			tempString[k] = inString[i] ;
			k++ ;
			tempString[k] = '\0' ;
		}
	}

	return ;
}

IntT updateMarkers(docId)
F_ObjHandleT docId ;
{
	F_ObjHandleT markerId;
	F_ObjHandleT pageId;
	IntT pageNum, count, iTemp1, iTemp2 ;
	StringT markerName, pageString ;
	UCharT tempMarkerName[800], temp[800], pageFormat[800] ;
	F_ObjHandleT markerTypeId ;
	StringT fmtName;
	IntT i = 0, k, copyStart, pageStyle ;
	ATOM tempAtom ;
	StringT string = NULL;
	F_TextRangeT tr;
	F_TextLocT tl;
	BoolT isNoPage = False, isStartRange = False, isEndRange = False ;

	updateReferenceFormat(docId) ;

//	F_Printf(NULL, (StringT) "referenceFormat = %s\n", referenceFormat);

	if (itemCount > itemLimit)
		return(1) ;

	// Get all markers and update the catcher.
	markerId = F_ApiGetId(FV_SessionId, docId, FP_FirstMarkerInDoc);

	if (markerId == 0)
	{
		return (-1) ;
	}

	count = 0 ;

	// Wait for the window to appear.
	while (count < 100)
	{
		if (FindWindow(NULL, szWindowTitle) != NULL)
			break ;

		F_ApiUSleep(50) ;
		count++ ;
	}

	F_ApiUSleep(500) ;

	while(markerId)
	{
		if (itemCount > itemLimit)
			return(1) ;

		for (i = 0 ; (fmtName = F_StrListGet(markerTypes, i)) != NULL ; i++)
		{
			markerTypeId = F_ApiGetNamedObject(docId,FO_MarkerType, fmtName);

			if (F_ApiGetId(docId, markerId, FP_MarkerTypeId) == markerTypeId)
				break ;
		}

		if ((fmtName != NULL) && (fmtName[0] != '\0'))
		{
			pageId = getMarkerPageId(docId, markerId);
			pageString = F_ApiGetString(docId, pageId, FP_PageNumString);
			pageNum = F_StrAlphaToInt(pageString) ;

			if (pageNum == 0)
			{
				pageStyle = F_ApiGetInt(FV_SessionId,docId,FP_PageNumStyle) ;

				if ((pageStyle == FV_PAGE_NUM_ROMAN_UC) || (pageStyle == FV_PAGE_NUM_ROMAN_LC))
				{
					pageNum = convertFromRoman(pageString, pageStyle) ;
				}

				if ((pageStyle == FV_PAGE_NUM_ALPHA_UC) || (pageStyle == FV_PAGE_NUM_ALPHA_LC))
				{
					pageNum = convertFromAlpha(pageString, pageStyle) ;
				}
			}

			// Cope with conditional text
			if ((pageString == NULL) && (pageNum == 0))
			{
				markerId  = F_ApiGetId(docId, markerId, FP_NextMarkerInDoc);
				continue ;
			}

//			F_Printf(NULL, (StringT) "pageString = %s, pageNum = %d\n", pageString, pageNum);

			parseReferenceText(docId, pageString, referenceFormat, temp) ;
			F_StrCpy(pageFormat, temp) ;

			markerName = F_ApiGetString(docId, markerId, FP_MarkerText);

			F_Free(pageString) ;

			itemCount++ ;

			// Don't include IXGEN stuff or empty markers
			if (/*(F_StrICmpN(markerName, (StringT)"!IXGEN", 6) != 0) && */ (F_StrLen(markerName) != 0))
			{
				copyStart = 0 ;

				isNoPage = False ;
				isStartRange = False ;
				isEndRange = False ;

				/* Split up markers separated by ;'s */
				for (i = 0 ; i < F_StrLen(markerName) ; i++)
				{
					if ((markerName[i] == ';') && (isEscapeSequence(markerName, i) == False))
					{
						F_StrCpyN(tempMarkerName, &markerName[copyStart], i - copyStart + 1) ;
						tempMarkerName[i - copyStart + 1] = '\0' ;

						F_StrStripLeadingSpaces(tempMarkerName) ;

						if ((iTemp1 = F_StrSubString(tempMarkerName, "<$nopage>")) != -1)
							isNoPage = True ; 

						if ((iTemp2 = F_StrSubString(tempMarkerName, "<$singlepage>")) != -1)
						{
							if ((iTemp1 == -1) || (iTemp2 > iTemp1))
							{
								isNoPage = False ;
							}
						}

						if (F_StrSubString(tempMarkerName, "<$startrange>") != -1)
						{
							isStartRange = True ;
						}
						else
						{
							if (isStartRange == True)
							{
								F_Sprintf(temp, "<$startrange>%s", tempMarkerName) ;
								F_StrCpy(tempMarkerName, temp) ;
							}
						}

						if (F_StrSubString(tempMarkerName, "<$endrange>") != -1)
						{
							isEndRange = True ;
						}
						else
						{
							if (isEndRange == True)
							{
								F_Sprintf(temp, "<$endrange>%s", tempMarkerName) ;
								F_StrCpy(tempMarkerName, temp) ;
							}
						}

/*						F_Sprintf(temp, "%s%s", pageFormat, tempMarkerName) ;
						F_StrCpy(tempMarkerName, temp) ; */

						F_Sprintf(temp, "%s%d", pageFormat, pageNum) ;

//						F_Printf(NULL, (StringT) "NONBLANK tempMarkerName = %s\n", tempMarkerName);

						while ((tempAtom = GlobalFindAtom(tempMarkerName)) != 0)
							GlobalDeleteAtom(tempAtom) ;

						while ((tempAtom = GlobalFindAtom(temp)) != 0)
							GlobalDeleteAtom(tempAtom) ;

						// Send the text and current page to the catcher.
						if (isNoPage == True)
							SendMessage(FindWindow(NULL, szWindowTitle), ITEMLIST_UPDATE, (WPARAM)-1, (LPARAM)GlobalAddAtom(tempMarkerName)) ;
						else
							SendMessage(FindWindow(NULL, szWindowTitle), ITEMLIST_UPDATE, (WPARAM)GlobalAddAtom(temp), (LPARAM)GlobalAddAtom(tempMarkerName)) ;

						copyStart = i+1 ;
					}
				}

				F_StrCpyN(tempMarkerName, &markerName[copyStart], i - copyStart + 1) ;
				tempMarkerName[i - copyStart + 1] = '\0' ;

				F_StrStripLeadingSpaces(tempMarkerName) ;

				if ((iTemp1 = F_StrSubString(tempMarkerName, "<$nopage>")) != -1)
					isNoPage = True ;

				if ((iTemp2 = F_StrSubString(tempMarkerName, "<$singlepage>")) != -1)
				{
					if ((iTemp1 == -1) || (iTemp2 > iTemp1))
					{
						isNoPage = False ;
					}
				}

				if (F_StrSubString(tempMarkerName, "<$startrange>") != -1)
				{
					isStartRange = True ;
				}
				else
				{
					if (isStartRange == True)
					{
						F_Sprintf(temp, "<$startrange>%s", tempMarkerName) ;
						F_StrCpy(tempMarkerName, temp) ;
					}
				}

				if (F_StrSubString(tempMarkerName, "<$endrange>") != -1)
				{
					isEndRange = True ;
				}
				else
				{
					if (isEndRange == True)
					{
						F_Sprintf(temp, "<$endrange>%s", tempMarkerName) ;
						F_StrCpy(tempMarkerName, temp) ;
					}
				}

/*				F_Sprintf(temp, "%s%s", pageFormat, tempMarkerName) ;
				F_StrCpy(tempMarkerName, temp) ; */

				F_Sprintf(temp, "%s%d", pageFormat, pageNum) ;

//				F_Printf(NULL, (StringT) "tempMarkerName = %s\n", tempMarkerName);

				while ((tempAtom = GlobalFindAtom(tempMarkerName)) != 0)
					GlobalDeleteAtom(tempAtom) ;

				while ((tempAtom = GlobalFindAtom(temp)) != 0)
					GlobalDeleteAtom(tempAtom) ;

				// Send the text and current page to the catcher.
				if (isNoPage == True)
					SendMessage(FindWindow(NULL, szWindowTitle), ITEMLIST_UPDATE, (WPARAM)-1, (LPARAM)GlobalAddAtom(tempMarkerName)) ;
				else
					SendMessage(FindWindow(NULL, szWindowTitle), ITEMLIST_UPDATE, (WPARAM)GlobalAddAtom(temp), (LPARAM)GlobalAddAtom(tempMarkerName)) ;
			}
			else
			{
				// Blank marker, get the text to place in the marker.
				if (getBlankMarkerText(docId, markerId, tempMarkerName) != -1)
				{

/*					tl = F_ApiGetTextLoc(docId, markerId, FP_TextLoc);

					tr.end.objId = tr.beg.objId = tl.objId;
					tr.end.offset = tr.beg.offset = tl.offset;
					tr.end.offset += 100 ;

					textItems = F_ApiGetTextForRange(docId, &tr, FTI_String);
					string = (StringT)CreateStringFromTextItems(textItems);

					for (i = 0 ; i < F_StrLen(string) ; i++)
					{
						if (string[i] != ' ')
						{
							copyStart = i ;
							break ;
						}
					}

					for ( ; i < F_StrLen(string) ; i++)
					{
						if (string[i] == ' ')
						{
							break ;
						}
					}

					F_StrCpyN(tempMarkerName, &string[copyStart], i-copyStart+1) ;
					tempMarkerName[i-copyStart+1] = '\0' ; */

					while ((tempAtom = GlobalFindAtom(tempMarkerName)) != 0)
						GlobalDeleteAtom(tempAtom) ;

					while ((tempAtom = GlobalFindAtom(temp)) != 0)
						GlobalDeleteAtom(tempAtom) ;

/*					if(string != NULL)
						F_Free(string); */

/*					F_Sprintf(temp, "%s%s", pageFormat, tempMarkerName) ;
					F_StrCpy(tempMarkerName, temp) ; */

					F_Sprintf(temp, "%s%d", pageFormat, pageNum) ;

//					F_Printf(NULL, (StringT) "BLANK tempMarkerName = %s\n", tempMarkerName);

					// Send the text and current page to the catcher.
					SendMessage(FindWindow(NULL, szWindowTitle), ITEMLIST_UPDATE, (WPARAM)GlobalAddAtom(temp), (LPARAM)GlobalAddAtom(tempMarkerName)) ;
				}
			}

			F_Free(markerName) ;
		}

		markerId  = F_ApiGetId(docId, markerId, FP_NextMarkerInDoc);
	}

	return (1) ;
}

IntT updateAllMarkers(docId)
F_ObjHandleT docId ;
{
	F_ObjHandleT markerId, openId, bookId;
	F_ObjHandleT pageId;
	IntT pageNum, count, retCode ;
	StringT markerName, pageString ;
	F_ObjHandleT markerTypeId ;
	StringT fmtName;
	IntT i = 0, extRet ;
	ATOM tempAtom ;
	F_PropValsT script, *returnp = NULL;
	F_ObjHandleT compId; /* book component ID */
	F_ObjHandleT tmpdocId; /*  document ID */
	StringT compName = NULL; /* name of book component */
	BoolT alreadyOpened ;

	extRet = getExtension(docId) ;

/*	if (extRet == UNKNOWN)
		return (-1) ; */

	// First delete the existing item list.
	SendMessage(FindWindow(NULL, szWindowTitle), ITEMLIST_DELETE, (WPARAM)0, (LPARAM)0) ;

	itemCount = 0 ;

//	if (extRet == DOC)
	if (extRet != BOOK)
	{
		retCode = updateMarkers(docId) ;

		if (retCode == -1)
		{
			// Make sure that the emDEX window is updated when no more markers are left.
			SendMessage(FindWindow(NULL, szWindowTitle), ITEMLIST_CLEAR, (WPARAM)0, (LPARAM)0) ;
			SendMessage(FindWindow(NULL, szWindowTitle), LISTBOX_UPDATE, (WPARAM)0, (LPARAM)0) ;
		}

		return (retCode) ;
	}

	compId = F_ApiGetId(FV_SessionId, docId, FP_FirstComponentInBook);

	while(compId)
	{
		compName = F_ApiGetString(docId, compId, FP_Name);

		tmpdocId = F_ApiGetId(0, FV_SessionId, FP_FirstOpenDoc);

		while (tmpdocId)
		{
			if (F_StrICmp(F_ApiGetString(tmpdocId, tmpdocId, FP_Name), compName) == 0)
				break ;

			tmpdocId = F_ApiGetId(FV_SessionId, tmpdocId,	FP_NextOpenDocInSession);
		}

		if (tmpdocId)
		{
			updateMarkers(tmpdocId) ;
		}

		/* get next component in book */
		compId = F_ApiGetId(docId, compId, FP_NextComponentInBook);
	}

	F_Free(compName) ;

	return (1) ;
}

VoidT updateDocName(docId)
F_ObjHandleT docId ;
{
	StringT docName ;

	docName = F_ApiGetString(docId, docId, FP_Name) ;

	F_Sprintf(szWindowTitle, "emDEX - \"%s\"", docName) ;

	F_Free(docName) ;
}

NativeIntT fn(ConStringT *s1, ConStringT *s2)
{
	return (F_StrICmp(*s1, *s2));
}

IntT doDelete(F_ObjHandleT docId, StringT s1, StringT s2)
{
	F_ObjHandleT mrkrId, prvmrkrId;
	StringT markerName, docName ;
	UCharT fileName[800], option[2], tempText[800], tempMarkerName[800] ;
	BoolT isProcessed = False ;
	IntT i, j, startPos ;
	F_TextLocT tl;
	F_TextRangeT tr;
	F_TextItemsT textItems;
	StringT string = NULL;
	IntT copyStart ;
	F_ObjHandleT markerTypeId ;

	// If the delete key is kept depressed, it can get confused
	if ((F_StrLen(s1) >= 800) || (F_StrLen(s2) >= 800))
		return(-1) ;

	/* Get first marker, then traverse markers and delete them. */
	if (g_currentMarkerId != 0)
		mrkrId = g_currentMarkerId ;
	else
		mrkrId = F_ApiGetId(FV_SessionId, docId, FP_FirstMarkerInDoc);

	while ((mrkrId) && (isProcessed == False))
	{
		/* As each marker is deleted, its FP_NextMarkerInDoc property
		** becomes invalid, so it is necessary to get the next marker
		** before deleting the current one.
		*/
		prvmrkrId = mrkrId;
		mrkrId = F_ApiGetId(docId, prvmrkrId, FP_NextMarkerInDoc);

		markerName = F_ApiGetString(docId, prvmrkrId, FP_MarkerText);

		startPos = 0 ;

		// Cope with markers containing ;'s to separate multiple entries.
		for (i = 0 ; i < F_StrLen(markerName) ; i++)
		{
			if ((markerName[i] == ';') && (isEscapeSequence(markerName, i) == False))
			{
				F_StrCpyN(tempText, &markerName[startPos], i - startPos + 1) ;
				tempText[i - startPos + 1] = '\0' ;

				F_StrStripLeadingSpaces(tempText) ;

				startPos = i+1 ;

				// Make sure that the marker type is included.
				for (j = 0 ; IncludeTypes[j][0] != '\0' ; j++)
				{
					markerTypeId = F_ApiGetNamedObject(docId, FO_MarkerType, IncludeTypes[j]);

					if (F_ApiGetId(docId, prvmrkrId, FP_MarkerTypeId) == markerTypeId)
						break ;
				}

//				F_Printf(NULL, (StringT) "tempText = %s s1 = %s  s2 = %s\n", tempText, s1, s2);

/*				if (((F_StrCmp(tempText, s1) == 0) || (F_StrCmp(tempText, s2) == 0)) &&
					(F_StrLen(tempText) == F_StrLen(s1))) */
				if ((IncludeTypes[j][0] != '\0') &&
					(noTagCompare(tempText, s1) == 0) || (noTagCompare(tempText, s2) == 0))
				{
					isProcessed = True ;

					if (markerName != NULL)
						F_Free(markerName) ;

					// Check if contained within an inset
					if (F_ApiGetId(docId, docId, FP_FirstSelectedTiInDoc) != 0)
					{
						if (F_ApiAlert((StringT)"If the marker is contained within the highlighted text inset,\ndeleting will remove the entire inset.\n\nContinue ?", FF_ALERT_NO_DEFAULT) == -1)
						{
							return (-1) ;
						}
					}

					F_ApiDelete(docId, prvmrkrId);

					updateDocName(docId) ;

					if (FindWindow(NULL, szWindowTitle) != NULL)
					{
						SendMessage(FindWindow(NULL, szWindowTitle), ITEMLIST_DELETE, (WPARAM)0, (LPARAM)0) ;

						if (updateAllMarkers(docId) != -1)
						{
							// Update the listbox
							SendMessage(FindWindow(NULL, szWindowTitle), LISTBOX_UPDATE, (WPARAM)0, (LPARAM)0) ;
						}
					}

					docName = F_ApiGetString(docId, docId, FP_Name) ;

					if ((docId = getContainedInBook(docName)) != 0)
					{
						updateDocName(docId) ;

						if (FindWindow(NULL, szWindowTitle) != NULL)
						{
							if (updateAllMarkers(docId) == -1)
							{
								if (docName != NULL)
									F_Free(docName) ;

								break ;
							}

							// Update the listbox
							SendMessage(FindWindow(NULL, szWindowTitle), LISTBOX_UPDATE, (WPARAM)0, (LPARAM)0) ;
						}
					}

					if (docName != NULL)
						F_Free(docName) ;

					break ;
				}
			}
		}

		// No ;'s found
		if ((i == F_StrLen(markerName)) || (F_StrLen(markerName) == 0))
		{
			if (F_StrLen(markerName) == 0)
			{
				// Blank marker, get the text to place in the marker.
				if (getBlankMarkerText(docId, prvmrkrId, tempMarkerName) == -1)
					continue ;

/*				tl = F_ApiGetTextLoc(docId, prvmrkrId, FP_TextLoc);

				tr.end.objId = tr.beg.objId = tl.objId;
				tr.end.offset = tr.beg.offset = tl.offset;
				tr.end.offset += 100 ;

				textItems = F_ApiGetTextForRange(docId, &tr, FTI_String);
				string = (StringT)CreateStringFromTextItems(textItems);

				for (i = 0 ; i < F_StrLen(string) ; i++)
				{
					if (string[i] != ' ')
					{
						copyStart = i ;
						break ;
					}
				}

				for ( ; i < F_StrLen(string) ; i++)
				{
					if (string[i] == ' ')
					{
						break ;
					}
				}

				if (F_StrLen(string) == 0)
					continue ;

				F_StrCpyN(tempMarkerName, &string[copyStart], i-copyStart+1) ;
				tempMarkerName[i-copyStart+1] = '\0' ; */

				markerName = F_StrNew(F_StrLen(tempMarkerName)) ;

				F_StrCpy(tempText, tempMarkerName) ;
			}
			else
			{
				F_StrCpyN(tempText, &markerName[startPos], i - startPos + 1) ;
				tempText[i - startPos + 1] = '\0' ;
			}

			F_StrStripLeadingSpaces(tempText) ;

			// Make sure that the marker type is included.
			for (j = 0 ; IncludeTypes[j][0] != '\0' ; j++)
			{
				markerTypeId = F_ApiGetNamedObject(docId, FO_MarkerType, IncludeTypes[j]);

				if (F_ApiGetId(docId, prvmrkrId, FP_MarkerTypeId) == markerTypeId)
					break ;
			}

//			F_Printf(NULL, (StringT) "tempText = %s s1 = %s  s2 = %s\n", tempText, s1, s2);

/*			if (((F_StrCmp(tempText, s1) == 0) || (F_StrCmp(tempText, s2) == 0)) &&
				(F_StrLen(tempText) == F_StrLen(s1))) */
			if ((IncludeTypes[j][0] != '\0') &&
				(noTagCompare(tempText, s1) == 0) || (noTagCompare(tempText, s2) == 0))
			{
				isProcessed = True ;

				if (markerName != NULL)
					F_Free(markerName) ;

				// Check if contained within an inset
				if (F_ApiGetId(docId, docId, FP_FirstSelectedTiInDoc) != 0)
				{
					if (F_ApiAlert((StringT)"If the marker is contained within the highlighted text inset,\ndeleting will remove the entire inset.\n\nContinue ?", FF_ALERT_NO_DEFAULT) == -1)
					{
						return (-1) ;
					}
				}

				F_ApiDelete(docId, prvmrkrId);

				updateDocName(docId) ;

				if (FindWindow(NULL, szWindowTitle) != NULL)
				{
					SendMessage(FindWindow(NULL, szWindowTitle), ITEMLIST_DELETE, (WPARAM)0, (LPARAM)0) ;

					if (updateAllMarkers(docId) != -1)
					{
						// Update the listbox
						SendMessage(FindWindow(NULL, szWindowTitle), LISTBOX_UPDATE, (WPARAM)0, (LPARAM)0) ;
					}
				}

				docName = F_ApiGetString(docId, docId, FP_Name) ;

				if ((docId = getContainedInBook(docName)) != 0)
				{
					updateDocName(docId) ;

					if (FindWindow(NULL, szWindowTitle) != NULL)
					{
						if (updateAllMarkers(docId) == -1)
						{
							if (docName != NULL)
								F_Free(docName) ;

							break ;
						}

						// Update the listbox
						SendMessage(FindWindow(NULL, szWindowTitle), LISTBOX_UPDATE, (WPARAM)0, (LPARAM)0) ;
					}
				}

				if (docName != NULL)
					F_Free(docName) ;

				break ;
			}
			else
			{
				if (markerName != NULL)
					F_Free(markerName) ;
			}
		}
	}

	if (isProcessed == True)
		return (0) ;
	else
		return (-1) ;
}

VoidT doUpdate(F_ObjHandleT docId)
{
	StringT docName;

	updateDocName(docId) ;

	if (FindWindow(NULL, szWindowTitle) != NULL)
	{
		if (updateAllMarkers(docId) != -1)
		{
			// Update the listbox
			SendMessage(FindWindow(NULL, szWindowTitle), LISTBOX_UPDATE, (WPARAM)0, (LPARAM)0) ;
		}
	}

	docName = F_ApiGetString(docId, docId, FP_Name) ;

	if ((docId = getContainedInBook(docName)) != 0)
	{
		updateDocName(docId) ;

		if (FindWindow(NULL, szWindowTitle) != NULL)
		{
			if (updateAllMarkers(docId) == -1)
			{
				if (docName != NULL)
					F_Free(docName) ;

				return ;
			}

			// Update the listbox
			SendMessage(FindWindow(NULL, szWindowTitle), LISTBOX_UPDATE, (WPARAM)0, (LPARAM)0) ;
		}
	}

	if (docName != NULL)
		F_Free(docName) ;
}

VoidT doCheckSpelling(F_ObjHandleT docId)
{
	IntT fcodes[1] ;

	fcodes[0] = KBD_CHECKDOC ;
	F_ApiFcodes(sizeof(fcodes)/sizeof(IntT), fcodes);

	fcodes[0] = KBD_SPELLING ;
	F_ApiFcodes(sizeof(fcodes)/sizeof(IntT), fcodes);

	return ;
}

IntT doFind(F_ObjHandleT docId, StringT s1, StringT s2, INT pageNum, StringT refText)
{
	F_ObjHandleT mrkrId, prvmrkrId;
	StringT docName ;
	UCharT fileName[800], option[2], tempText[800], tempMarkerName[800], temp[800] ;
	F_TextRangeT trFound;
	F_TextLocT tl;
	F_TextRangeT tr;
	F_ObjHandleT pageId;
	IntT markerPageNum, count ;
	StringT pageString ;
	IntT i, startPos ;
	StringT string = NULL;
	F_TextItemsT textItems;
	IntT copyStart, j ;
	BoolT blankMarker = False, semiColonFound = False, foundMarker = False ;
	IntT fcodes[1] ;
	UCharT s3[800] ;
	F_ObjHandleT markerTypeId ;
	IntT pageStyle, retCode ;
	HWND hEditWindow ;
	StringT fmtName ;
	F_ObjHandleT currentTypeId, currentTypeName ;

	F_TextRangeT trSel;
	F_PropValsT findParams;

	g_currentMarkerId = 0 ;
	F_StrCpy(g_markerName, "") ;

//	F_Printf(NULL, (StringT) "Searching for %s on page %d\n", s1, pageNum);

	/* Get first marker, then traverse markers and find the required one. */
	mrkrId = F_ApiGetId(FV_SessionId, docId, FP_FirstMarkerInDoc);

	while (mrkrId)
	{
		prvmrkrId = mrkrId;
		mrkrId = F_ApiGetId(docId, prvmrkrId, FP_NextMarkerInDoc);

		g_markerName = F_ApiGetString(docId, prvmrkrId, FP_MarkerText);

//		F_Printf(NULL, (StringT) "markerName = %s\n", g_markerName);

		startPos = 0 ;
		semiColonFound = False ;
	
		// Cope with markers containing ;'s to separate multiple entries.
		for (i = 0 ; i < F_StrLen(g_markerName) ; i++)
		{
			if ((g_markerName[i] == ';') && (isEscapeSequence(g_markerName, i) == False))
			{
				semiColonFound = True ;

				F_StrCpyN(tempText, &g_markerName[startPos], i - startPos + 1) ;
				tempText[i - startPos + 1] = '\0' ;

				F_StrStripLeadingSpaces(tempText) ;

				startPos = i+1 ;

				// Make sure that the marker type is included.
				for (j = 0 ; IncludeTypes[j][0] != '\0' ; j++)
				{
					markerTypeId = F_ApiGetNamedObject(docId, FO_MarkerType, IncludeTypes[j]);

					if (F_ApiGetId(docId, prvmrkrId, FP_MarkerTypeId) == markerTypeId)
						break ;
				}

//				F_Printf(NULL, (StringT) "tempText =  %s\n", tempText);

/*				if (((F_StrCmp(tempText, s1) == 0) || (F_StrCmp(tempText, s2) == 0)) &&
					(F_StrLen(tempText) == F_StrLen(s1))) */
				if ((IncludeTypes[j][0] != '\0') &&
					(noTagCompare(tempText, s1) == 0) || (noTagCompare(tempText, s2) == 0))
				{
					// If also searching for a particular page, make sure that this is the one.
					if (pageNum != -1)
					{
						pageId = getMarkerPageId(docId, prvmrkrId);
						pageString = F_ApiGetString(docId, pageId, FP_PageNumString);
						markerPageNum = F_StrAlphaToInt(pageString) ; 

						if (markerPageNum == 0)
						{
							pageStyle = F_ApiGetInt(FV_SessionId,docId,FP_PageNumStyle) ;

							if ((pageStyle == FV_PAGE_NUM_ROMAN_UC) || (pageStyle == FV_PAGE_NUM_ROMAN_LC))
							{
								markerPageNum = convertFromRoman(pageString, pageStyle) ;
							}

							if ((pageStyle == FV_PAGE_NUM_ALPHA_UC) || (pageStyle == FV_PAGE_NUM_ALPHA_LC))
							{
								markerPageNum = convertFromAlpha(pageString, pageStyle) ;
							}
						}

						parseReferenceText(docId, pageString, referenceFormat, temp) ;
						temp[F_StrLen(temp)-1] = '\0' ;
					}

//					F_Printf(NULL, (StringT) "String compares, pageNum = %d, markerPageNum = %d\n", pageNum, markerPageNum);

//					F_Printf(NULL, (StringT) "String compares, refText = %s, temp = %s\n", refText, temp);

					if ((pageNum == -1) || ((pageNum != -1) && /*(markerPageNum == pageNum)*/F_StrCmp(refText, temp) == 0))
					{
//						F_Printf(NULL, (StringT) "Found it on page %d\n", pageNum);

						foundMarker = True ;

						tl = F_ApiGetTextLoc(docId, prvmrkrId, FP_TextLoc);

						tr.end.objId = tr.beg.objId = tl.objId;
						tr.end.offset = tr.beg.offset = tl.offset;
//						F_ApiSetTextRange(FV_SessionId, docId, FP_TextSelection, &tr);

						g_currentMarkerId = prvmrkrId ;

//						F_Free(markerName) ;

						break ;
					}
				}
			}
		}

		if ((semiColonFound == True) && (i < F_StrLen(g_markerName)))
			break ;

		// No ;'s found
		if ((i == F_StrLen(g_markerName)) || (F_StrLen(g_markerName) == 0))
		{
			if (F_StrLen(g_markerName) == 0)
			{
				// Blank marker, get the text to place in the marker.
				blankMarker = True ;

				if (getBlankMarkerText(docId, prvmrkrId, tempMarkerName) == -1)
					continue ;
/*				tl = F_ApiGetTextLoc(docId, prvmrkrId, FP_TextLoc);

				tr.end.objId = tr.beg.objId = tl.objId;
				tr.end.offset = tr.beg.offset = tl.offset;
				tr.end.offset += 100 ;

				textItems = F_ApiGetTextForRange(docId, &tr, FTI_String);
				string = (StringT)CreateStringFromTextItems(textItems);

				for (i = 0 ; i < F_StrLen(string) ; i++)
				{
					if (string[i] != ' ')
					{
						copyStart = i ;
						break ;
					}
				}

				for ( ; i < F_StrLen(string) ; i++)
				{
					if (string[i] == ' ')
					{
						break ;
					}
				}

				F_StrCpyN(tempMarkerName, &string[copyStart], i-copyStart+1) ;
				tempMarkerName[i-copyStart+1] = '\0' ; */

				if (g_markerName != NULL)
					F_Free(g_markerName) ;

				g_markerName = F_StrNew(F_StrLen(tempMarkerName)) ;

				F_StrCpy(tempText, tempMarkerName) ;
			}
			else
			{
				blankMarker = False ;

				F_StrCpyN(tempText, &g_markerName[startPos], i - startPos + 1) ;
				tempText[i - startPos + 1] = '\0' ;
			}

			F_StrStripLeadingSpaces(tempText) ;

			// Make sure that the marker type is included.
			for (j = 0 ; IncludeTypes[j][0] != '\0' ; j++)
			{
				markerTypeId = F_ApiGetNamedObject(docId, FO_MarkerType, IncludeTypes[j]);

				if (F_ApiGetId(docId, prvmrkrId, FP_MarkerTypeId) == markerTypeId)
					break ;
			}

/*			if (((F_StrCmp(tempText, s1) == 0) || (F_StrCmp(tempText, s2) == 0)) &&
				(F_StrLen(tempText) == F_StrLen(s1))) */

/*			F_Printf(NULL, (StringT) "IncludeTypes[j] = %s\n", IncludeTypes[j]) ; */
//			F_Printf(NULL, (StringT) "tempText = %s, s1 = %s, s2 = %s\n", tempText, s1, s2);

			if ((IncludeTypes[j][0] != '\0') &&
				(noTagCompare(tempText, s1) == 0) || (noTagCompare(tempText, s2) == 0))
			{
//				F_Printf(NULL, (StringT) "String compares, refText = %s, temp = %s, pagenum = %d\n", refText, temp, pageNum);

				// If also searching for a particular page, make sure that this is the one.
				if (pageNum != -1)
				{
					pageId = getMarkerPageId(docId, prvmrkrId);
					pageString = F_ApiGetString(docId, pageId, FP_PageNumString);
					markerPageNum = F_StrAlphaToInt(pageString) ;
					
//					F_Printf(NULL, (StringT) "markerPageNum = %d\n", markerPageNum);

					if (markerPageNum == 0)
					{
						pageStyle = F_ApiGetInt(FV_SessionId,docId,FP_PageNumStyle) ;

						if ((pageStyle == FV_PAGE_NUM_ROMAN_UC) || (pageStyle == FV_PAGE_NUM_ROMAN_LC))
						{
							markerPageNum = convertFromRoman(pageString, pageStyle) ;
						}

						if ((pageStyle == FV_PAGE_NUM_ALPHA_UC) || (pageStyle == FV_PAGE_NUM_ALPHA_LC))
						{
							markerPageNum = convertFromAlpha(pageString, pageStyle) ;
						}
					}

					parseReferenceText(docId, pageString, referenceFormat, temp) ;
					temp[F_StrLen(temp)-1] = '\0' ;

//					F_Printf(NULL, (StringT) "markerPageNum = %d\n", markerPageNum);
				}

				if ((pageNum == -1) || ((pageNum != -1) && /*(markerPageNum == pageNum)*/F_StrCmp(refText, temp) == 0))
				{
					foundMarker = True ;

//					F_Printf(NULL, (StringT) "Found it on page %d\n", pageNum);

					tl = F_ApiGetTextLoc(docId, prvmrkrId, FP_TextLoc);

					tr.end.objId = tr.beg.objId = tl.objId;
					tr.end.offset = tr.beg.offset = tl.offset;
//					F_ApiSetTextRange(FV_SessionId, docId, FP_TextSelection, &tr);

					g_currentMarkerId = prvmrkrId ;

//					F_Free(markerName) ;

/*					if (blankMarker == True)
					{
						/* Scroll to it. */
/*						tr.end.offset = (tl.offset + F_StrLen(tempText)+1) ;
						F_ApiScrollToText(docId, &tr);

						// Bring up the markers dialog box.
						fcodes[0] = KBD_MARKERS ;
						F_ApiFcodes(sizeof(fcodes)/sizeof(IntT), fcodes);

						return (0) ;
					} */

					break ;
				}
			}

//			F_Free(markerName) ;
		}
	}

	if (foundMarker == False)
	{
//		F_Printf(NULL, (StringT) "Didn't find it\n");

		// Didn't find it in this document.
//		setEditorText() ;

		return (-1) ;
	}

//	F_Printf(NULL, (StringT) "Found it\n");

	/* Scroll to it. */
	if (blankMarker == True)
		tr.end.offset = (tl.offset + F_StrLen(tempText)+1) ;
	else
		tr.end.offset += 1 ;

	retCode = F_ApiScrollToText(docId, &tr) ;

/*	if (retCode != FE_Success)
		F_Printf(NULL, (StringT) "retCode = %d\n", retCode); */

/*	fcodes[0] = CSR_BOE ;
	F_ApiFcodes(sizeof(fcodes)/sizeof(IntT), fcodes); */

	// This fixes a problem with focus in Structured FM
	if (FindWindow(NULL, "Marker") != NULL)
	{
		fcodes[0] = KBD_MARKERS ;
		F_ApiFcodes(sizeof(fcodes)/sizeof(IntT), fcodes);

		// Set the focus back to the catcher window.
		SendMessage(FindWindow(NULL, szWindowTitle), SETFOCUS, (WPARAM)0, (LPARAM)0) ;
	}

	hEditWindow = FindWindow(NULL, "emDEX Marker") ;
	if (hEditWindow == NULL)
		hEditWindow = FindWindow(NULL, "emDEX Range Marker") ;

//	setEditorText() ;

/*	if ((g_currentMarkerId != 0) && (g_CurrentEditor == MARKER_WINDOW))
	{
		F_ApiSetString(dialogId, rangeTextId, FP_Text, g_markerName);
		SendMessage(GetDlgItem(hEditWindow, 102), WM_SETTEXT, 0, "Edit Marker") ;

		EnableWindow(GetDlgItem(hEditWindow, 103), TRUE) ;

		currentTypeId = F_ApiGetId(docId, g_currentMarkerId, FP_MarkerTypeId) ;

		findAndSelectLabel(docId, dialogId, INCLUDE_BOX, currentTypeId) ;
	}
	else
	{
		if (g_CurrentEditor == MARKER_WINDOW)
			F_ApiSetString(dialogId, rangeTextId, FP_Text, "");

		SendMessage(GetDlgItem(hEditWindow, 102), WM_SETTEXT, 0, "New Marker") ;
		EnableWindow(GetDlgItem(hEditWindow, 103), FALSE) ;
	} */

	return (0) ;
}

IntT doReplace(F_ObjHandleT docId, StringT findString, StringT replaceString, BoolT isCaseSensitive)
{
	F_ObjHandleT mrkrId, prvmrkrId;
	StringT markerName ;
	UCharT fileName[800], option[2], replaceMarker[800] ;
	IntT i, j ;
	F_ObjHandleT markerTypeId ;
	BoolT hasUpdated = False ;

	F_TextRangeT trSel;
	F_PropValsT findParams;

	/* Get first marker, then traverse markers and find the required one. */
	mrkrId = F_ApiGetId(FV_SessionId, docId, FP_FirstMarkerInDoc);

	while (mrkrId)
	{
		prvmrkrId = mrkrId;
		mrkrId = F_ApiGetId(docId, prvmrkrId, FP_NextMarkerInDoc);

		markerName = F_ApiGetString(docId, prvmrkrId, FP_MarkerText);

		// Make sure that the marker type is included.
		for (j = 0 ; IncludeTypes[j][0] != '\0' ; j++)
		{
			markerTypeId = F_ApiGetNamedObject(docId, FO_MarkerType, IncludeTypes[j]);

			if (F_ApiGetId(docId, prvmrkrId, FP_MarkerTypeId) == markerTypeId)
				break ;
		}

		if (IncludeTypes[j][0] != '\0')
		{
			replaceMarker[0] = '\0' ;

			for (i = 0, j = 0 ; i < F_StrLen(markerName) ; )
			{
				if (((isCaseSensitive == True) && (F_StrCmpN(&markerName[i], findString, F_StrLen(findString)) == 0)) ||
					((isCaseSensitive == False) && (F_StrICmpN(&markerName[i], findString, F_StrLen(findString)) == 0)))
				{
					F_StrCat(&replaceMarker[j], replaceString) ;
					j += F_StrLen(replaceString) ;
					i += F_StrLen(findString) ;

					hasUpdated = True ;
				}
				else
				{
					replaceMarker[j] = markerName[i] ;
					replaceMarker[j+1] = '\0' ;
					j++ ;
					i++ ;
				}
			}

			if (hasUpdated == True)
			{
				F_ApiSetString(docId, prvmrkrId, FP_MarkerText, replaceMarker);
			}

			hasUpdated = False ;
		}

		F_Free(markerName) ;
	}

	return (0) ;
}

IntT getExtension(F_ObjHandleT docId)
{
	StringT docName ;
	IntT i ;

	docName = F_ApiGetString(docId, docId, FP_Name) ;

	// Find the documents extension to determine it's type.
	for (i = F_StrLen(docName)-1 ; i >= 0 ; i--)
	{
		if (docName[i] == '.')
			break ;
	}

	if ((F_ApiGetId(FV_SessionId, docId, FP_FirstComponentInBook) != 0) || 
		(F_StrICmp(&docName[i+1], "book") == 0))
	{
		return (BOOK) ;
	}
	else
	{
		return (DOC) ;
	}

/*	docName = F_ApiGetString(docId, docId, FP_Name) ;

	// Find the documents extension to determine it's type.
	for (i = F_StrLen(docName)-1 ; i >= 0 ; i--)
	{
		if (docName[i] == '.')
			break ;
	}

	if (F_StrICmp(&docName[i+1], "book") == 0)
	{
		F_Free(docName) ;
		return (BOOK) ;
	}
	else
	{
		if (F_StrICmp(&docName[i+1], "fm") == 0)
		{
			F_Free(docName) ;
			return (DOC) ;
		}
		else
		{
			F_Free(docName) ;
			return (UNKNOWN) ;
		}
	} */
}

F_ObjHandleT getContainedInBook(StringT docName)
{
	F_ObjHandleT bookId ;
	F_ObjHandleT compId; /* book component ID */
	StringT compName = NULL; /* name of book component */

	bookId = F_ApiGetId(FV_SessionId, FV_SessionId, FP_FirstOpenBook);

	while (bookId)
	{
		compId = F_ApiGetId(FV_SessionId, bookId, FP_FirstComponentInBook);

		while(compId)
		{
			/* get component name */
			compName = F_ApiGetString(bookId, compId, FP_Name);

			if (F_StrCmp(compName, docName) == 0)
			{
				if(compName != NULL)
					F_Free(compName);

				return (bookId) ;
			}

			if(compName != NULL)
				F_Free(compName);

			/* get next component in book */
			compId = F_ApiGetId(bookId, compId, FP_NextComponentInBook);
		}

		bookId = F_ApiGetId(FV_SessionId, bookId, FP_NextOpenBookInSession);
	}

	return bookId ;
}

VoidT openAllDocs(F_ObjHandleT docId)
{
	F_ObjHandleT markerId, openId;
	F_ObjHandleT pageId;
	IntT pageNum, count ;
	StringT markerName, pageString ;
	F_ObjHandleT markerTypeId ;
	StringT fmtName;
	IntT i = 0, extRet ;
	ATOM tempAtom ;
	F_PropValsT script, *returnp = NULL;
	F_ObjHandleT compId; /* book component ID */
	F_ObjHandleT tmpdocId; /*  document ID */
	StringT compName = NULL; /* name of book component */
	BoolT alreadyOpened ;
	IntT fcodes[1] ;

	/* create open script */
	script = F_ApiGetOpenDefaultParams();
	i = F_ApiGetPropIndex(&script, FS_MakeVisible);
	script.val[i].propVal.u.ival = True ;
	i = F_ApiGetPropIndex(&script, FS_RefFileNotFound);
	script.val[i].propVal.u.ival = FV_AllowAllRefFilesUnFindable;
	i = F_ApiGetPropIndex(&script, FS_FileIsOldVersion);
	script.val[i].propVal.u.ival = FV_DoOK;
	i = F_ApiGetPropIndex(&script, FS_FontChangedMetric);
	script.val[i].propVal.u.ival = FV_DoOK;
	i = F_ApiGetPropIndex(&script, FS_FontNotFoundInCatalog);
	script.val[i].propVal.u.ival = FV_DoOK;
	i = F_ApiGetPropIndex(&script, FS_FontNotFoundInDoc);
	script.val[i].propVal.u.ival = FV_DoOK;
	i = F_ApiGetPropIndex(&script, FS_LanguageNotAvailable);
	script.val[i].propVal.u.ival = FV_DoOK;

	i = F_ApiGetPropIndex(&script, FS_LockCantBeReset);
	script.val[i].propVal.u.ival = FV_DoOK;
	i = F_ApiGetPropIndex(&script, FS_UpdateTextReferences);
	script.val[i].propVal.u.ival = FV_DoNo;
	i = F_ApiGetPropIndex(&script, FS_UpdateXRefs);
	script.val[i].propVal.u.ival = FV_DoNo;
	i = F_ApiGetPropIndex(&script, FS_UseAutoSaveFile);
	script.val[i].propVal.u.ival = FV_DoNo;
	i = F_ApiGetPropIndex(&script, FS_UseRecoverFile);
	script.val[i].propVal.u.ival = FV_DoNo;
	i = F_ApiGetPropIndex(&script, FS_AlertUserAboutFailure);
	script.val[i].propVal.u.ival = False;
	i = F_ApiGetPropIndex(&script, FS_BeefyDoc);
	script.val[i].propVal.u.ival = FV_DoOK;
	i = F_ApiGetPropIndex(&script, FS_FileIsInUse);
	script.val[i].propVal.u.ival = FV_OpenViewOnly;
	i = F_ApiGetPropIndex(&script, FS_FileIsStructured);
	script.val[i].propVal.u.ival = FV_OpenViewOnly;
	i = F_ApiGetPropIndex(&script, FS_OpenFileNotWritable);
	script.val[i].propVal.u.ival = FV_DoOK;

	compId = F_ApiGetId(FV_SessionId, docId, FP_FirstComponentInBook);

	while(compId)
	{
		/* get component name */
		compName = F_ApiGetString(docId, compId, FP_Name);

		/* open the document with the component name */
		tmpdocId = F_ApiOpen(compName, &script, &returnp);

		// Make sure that the document is restored.
		F_ApiSetId(FV_SessionId, FV_SessionId, FP_ActiveDoc, tmpdocId);

		F_ApiSetInt(tmpdocId, tmpdocId, FP_IsIconified, 0);
/*		fcodes[0] = KBD_RESTORE ;
		F_ApiFcodes(sizeof(fcodes)/sizeof(IntT), fcodes);*/

		/* check outcome of open for FV_ReferencedFilesWerentFound */
		if(F_ApiCheckStatus(returnp, FV_ReferencedFilesWerentFound))
		{
			F_Printf(NULL, (StringT) "%s contains graphic files that could not be found.\n", compName);
		}

		/* check status of FV_UnresolvedTextInsets */
		if(F_ApiCheckStatus(returnp, FV_UnresolvedTextInsets))
		{
			F_Printf(NULL, (StringT) "%s contains imported text files that could not be found.\n", compName);
		}

		/* deallocate propVals */
		F_ApiDeallocatePropVals(returnp);

		if(compName != NULL)
			F_Free(compName);

		loadAutoCompleteText(tmpdocId) ;

		/* get next component in book */
		compId = F_ApiGetId(docId, compId, FP_NextComponentInBook);
	}

	F_ApiDeallocatePropVals(&script);

	return ;
}

F_ObjHandleT InsertMarker(F_ObjHandleT docId, F_TextLocT loc, StringT markername)
{
	F_ObjHandleT markerId, markerTypeId;

	// Insert a new marker at the specified location.
	markerId = F_ApiNewAnchoredObject(docId, FO_Marker, &loc);
	markerTypeId = F_ApiGetNamedObject(docId, FO_MarkerType, markername);
	if(!markerTypeId)
		markerTypeId = F_ApiNewNamedObject(docId, FO_MarkerType, markername);

	F_ApiSetId(docId, markerId, FP_MarkerTypeId, markerTypeId);
	return(markerId);
}

BoolT isEscapeSequence(StringT text, IntT pos)
{
	IntT count = 0, i ;

	for (i = (pos-1) ; i >= 0 ; i--)
	{
		if (text[i] != '\\')
			break ;
		else
			count++ ;
	}

	if ((count % 2) == 1)
		return (True) ;
	else
		return (False) ;
}

VoidT getAddText(StringT inText, StringT outText)
{
	INT i, j ;

	// Get the text without any [] tags.
	for (i = 0, j = 0 ; i < (INT)F_StrLen(inText) ; i++)
	{
		if (inText[i] == '[')
		{
			while ((inText[i] != ']') && (i < (INT)F_StrLen(inText)))
			{
				i++ ;
			}

			i++ ;
		}

		outText[j] = inText[i] ;
		outText[j+1] = '\0' ;
		j++ ;
	}

	return ;
}

VoidT removeTagText(StringT inText, StringT outText)
{
	INT i, j ;

	// Remove any tags.
	for (i = 0, j = 0 ; i < (INT)F_StrLen(inText) ; i++)
	{
		if ((inText[i] == '<') && (inText[i+1] == '$'))
		{
			while ((inText[i] != '>') && (i < (INT)F_StrLen(inText)))
			{
				i++ ;
			}
		}
		else
		{
			outText[j] = inText[i] ;
			outText[j+1] = '\0' ;
			j++ ;
		}
	}

	return ;
}

IntT noTagCompare(StringT inText1, StringT inText2)
{
	UCharT s1[800], s2[800], s3[800], s4[800] ;
	IntT	i, iTagEnd ;

	removeTagText(inText1, s1) ;
	removeTagText(inText2, s2) ;

	getAddText(s1, s3) ;
	getAddText(s2, s4) ;

	// Remove any formatting tags for the page numbers.
	for (i = (F_StrLen(s3)-1) ; i >= 0 ; i--)
	{
		while (s3[i] == ' ')
			i-- ;

		if (s3[i] != '>')
			break ;
		else
			iTagEnd = i ;

		while (s3[i] != '<')
			i-- ;

		if (i < 0)
			break ;
	}

	if (i >= 0)
		s3[i+1] = '\0' ;
	else
		s3[iTagEnd+1] = '\0' ;

	for (i = (F_StrLen(s4)-1) ; i >= 0 ; i--)
	{
		while (s4[i] == ' ')
			i-- ;

		if (s4[i] != '>')
			break ;
		else
			iTagEnd = i ;

		while (s4[i] != '<')
			i-- ;

		if (i < 0)
			break ;
	}

	if (i >= 0)
		s4[i+1] = '\0' ;
	else
		s4[iTagEnd+1] = '\0' ;

	// Remove spaces around the indent characters (':')
	removeIndentSpaces(s3, s1) ;
	removeIndentSpaces(s4, s2) ;

	F_StrStripLeadingSpaces(s1) ;
	F_StrStripLeadingSpaces(s2) ;

//	F_Printf(NULL, (StringT) "s1 = %s  s2 = %s\n", s1, s2);

	return (F_StrCmp(s1, s2)) ;
}

VoidT removeFormatting(StringT inText, StringT outText)
{
	UCharT s1[800] ;
	IntT	i ;

	removeTagText(inText, s1) ;

	getAddText(s1, outText) ;

	for (i = (F_StrLen(outText)-1) ; i >= 0 ; i--)
	{
		while (outText[i] == ' ')
			i-- ;

		if (outText[i] != '>')
			break ;

		while (outText[i] != '<')
			i-- ;

		if (i < 0)
			break ;
	}

	outText[i+1] = '\0' ;

	return ;
}

VoidT removeIndentSpaces(StringT inText, StringT outText)
{
	IntT	i, j, k ;

	// Remove all spaces around indent characters (':')
	for (i = 0, j = 0 ; i < (INT)F_StrLen(inText) ; i++)
	{
		outText[j] = inText[i] ;
		outText[j+1] = '\0' ;

		if ((inText[i] == ':') && (isEscapeSequence(inText, i) == False))
		{
			// Remove spaces before the ':'
			for (k = (j-1) ; k >= 0 ; k--)
			{
				if (outText[k] == ' ')
				{
					outText[k] = ':' ;
					outText[k+1] = '\0' ;
					j = k ;
				}
				else
				{
					break ;
				}
			}

			i++ ;

			// Skip spaces after the ':'
			while (inText[i] == ' ')
				i++ ;

			i-- ;
		}

		j++ ;
	}

//	F_Printf(NULL, (StringT) "inText = %s outText = %s\n", inText, outText);

	return ;
}

IntT getBlankMarkerText(F_ObjHandleT docId, F_ObjHandleT markerId, StringT tempMarkerName)
{
	StringT string = NULL;
	F_TextRangeT tr;
	F_TextItemsT textItems;
	F_TextLocT tl;
	IntT	i, copyStart ;
	BoolT	isSpaces = True ;

	tl = F_ApiGetTextLoc(docId, markerId, FP_TextLoc);

	tr.end.objId = tr.beg.objId = tl.objId;
	tr.end.offset = tr.beg.offset = tl.offset;
	tr.end.offset += 100 ;

	textItems = F_ApiGetTextForRange(docId, &tr, FTI_String);

	// Cope with blank markers near the end of the doc.
	while ((textItems.len == 0) && (tr.end.offset > tr.beg.offset))
	{
		tr.end.offset -- ;

		textItems = F_ApiGetTextForRange(docId, &tr, FTI_String);
	}

/*	for(i = 0; i < textItems.len; i++)
		F_Printf(NULL, (StringT) "textItems[%d] = %s\n", i, (StringT)textItems.val[i].u.sdata); */

//	string = (StringT)CreateStringFromFirstTextItem(textItems);
	string = (StringT)CreateStringFromTextItems(textItems);

	F_ApiDeallocateTextItems(&textItems);

//	F_Printf(NULL, (StringT) "string = <%s>\n", string);

	for (i = 0 ; i < F_StrLen(string) ; i++)
	{
		if (string[i] != ' ')
		{
			isSpaces = False ;
			break ;
		}
	}

	if ((string == NULL) || (isSpaces == True))
	{
		return (-1) ;
	}

	if (F_StrLen(string) == 0)
	{
		if(string != NULL)
			F_Free(string);

		return (-1) ;
	}

	for (i = 0 ; i < F_StrLen(string) ; i++)
	{
		if (string[i] != ' ')
		{
			copyStart = i ;
			break ;
		}
	}

	if (!isalnum(string[copyStart]))
	{
		i++ ;
	}
	else
	{
		for ( ; i < F_StrLen(string) ; i++)
		{
//			F_Printf(NULL, (StringT) "string[%d] = %d\n", i, string[i]);

			if ((string[i] == ' ') || !isalnum(string[i]))
			{
				break ;
			}
		}
	}

	F_StrCpyN(tempMarkerName, &string[copyStart], i-copyStart+1) ;
	tempMarkerName[i-copyStart+1] = '\0' ;

	if(string != NULL)
		F_Free(string);

	return (0) ;
}

IntT convertFromRoman(StringT pageString, IntT pageStyle)
{
	IntT i, outNum = 0 ;

	for (i = 0 ; i < F_StrLen(pageString) ; i++)
	{
		if (F_CharToLower(pageString[i]) == 'm')
		{
			outNum += 1000 ;
			continue ;
		}

		if ((F_CharToLower(pageString[i]) == 'c') && (F_CharToLower(pageString[i+1]) == 'm'))
		{
			outNum += 900 ;
			i++ ;
			continue ;
		}

		if (F_CharToLower(pageString[i]) == 'd')
		{
			outNum += 500 ;
			continue ;
		}
		
		if ((F_CharToLower(pageString[i]) == 'c') && (F_CharToLower(pageString[i+1]) == 'd'))
		{
			outNum += 400 ;
			i++ ;
			continue ;
		}

		if (F_CharToLower(pageString[i]) == 'c')
		{
			outNum += 100 ;
			continue ;
		}

		if ((F_CharToLower(pageString[i]) == 'x') && (F_CharToLower(pageString[i+1]) == 'c'))
		{
			outNum += 90 ;
			i++ ;
			continue ;
		}

		if (F_CharToLower(pageString[i]) == 'l')
		{
			outNum += 50 ;
			continue ;
		}

		if ((F_CharToLower(pageString[i]) == 'x') && (F_CharToLower(pageString[i+1]) == 'l'))
		{
			outNum += 40 ;
			i++ ;
			continue ;
		}

		if (F_CharToLower(pageString[i]) == 'x')
		{
			outNum += 10 ;
			continue ;
		}

		if ((F_CharToLower(pageString[i]) == 'i') && (F_CharToLower(pageString[i+1]) == 'x'))
		{
			outNum += 9 ;
			i++ ;
			continue ;
		}

		if (F_CharToLower(pageString[i]) == 'v')
		{
			outNum += 5 ;
			continue ;
		}

		if ((F_CharToLower(pageString[i]) == 'i') && (F_CharToLower(pageString[i+1]) == 'v'))
		{
			outNum += 4 ;
			i++ ;
			continue ;
		}

		if (F_CharToLower(pageString[i]) == 'i')
		{
			outNum += 1 ;
			continue ;
		}
	}

	return (outNum) ;
}

IntT convertFromAlpha(StringT pageString, IntT pageStyle)
{
	IntT i, len, outNum = 0 ;

//	F_Printf(NULL, (StringT) "pageString = %s\n", pageString);

	len = F_StrLen(pageString) ;

	for (i = 0 ; i < len ; i++)
	{
//		F_Printf(NULL, (StringT) "pageString[%d] = %d, len = %d\n", i, pageString[i], len);

		switch (len-(i+1))
		{
			case 0 :
				outNum += F_CharToLower(pageString[i]) - 96 ;
				break ;

			case 1 :
				outNum += (F_CharToLower(pageString[i]) - 96) * 26  ;
				break ;

			case 2 :
				outNum += (F_CharToLower(pageString[i]) - 96) * 676 ;
				break ;

			case 3 :
				outNum += (F_CharToLower(pageString[i]) - 96) * 17576 ;
				break ;
		}

//		F_Printf(NULL, (StringT) "outNum = %d\n", outNum);
	}

//	F_Printf(NULL, (StringT) "outNum = %d\n", outNum);

	return (outNum) ;
}

IntT GetPageType(docId, objectId)
F_ObjHandleT docId, objectId;
{
	F_ObjHandleT parentId, oldParentId;
	IntT parentType;

	parentId = objectId;
	parentType = FO_TextFrame;
	while (parentType != FO_BodyPage && 
			parentType != FO_MasterPage &&
				parentType != FO_RefPage && parentType != FO_HiddenPage)
	{
		oldParentId = parentId;
		parentId = F_ApiGetId(docId,parentId,FP_FrameParent);
		if (!parentId)
		{
			parentId = F_ApiGetId(docId,
								oldParentId,FP_PageFramePage);
			if (!parentId)
				break;
		}
		parentType = F_ApiGetObjectType(docId,parentId);
	}

	return parentType;
}

VoidT parseReferenceText(F_ObjHandleT docId, StringT pageString, StringT referenceText, StringT outText)
{
	IntT	i, j = 0, style, num ;
	UCharT temp[800] ;
	StringT strTemp ;

	outText[0] = '\0' ;

	if (referenceText[F_StrLen(referenceText)-1] == '@')
	{
		F_StrCpy(outText, referenceText) ;
		return ;
	}

//	F_Printf(NULL, (StringT) "referenceText = %s\n", referenceText);

	for (i = 0 ; i < F_StrLen(referenceText) ; )
	{
		temp[0] = '\0' ;

		if (F_StrCmpN(&referenceText[i], (StringT)"<$volnum>", 9) == 0)
		{
			num = F_ApiGetInt(FV_SessionId,docId,FP_VolumeNumber) ;

			F_Sprintf(temp, "%d", num) ;

			style = F_ApiGetInt(FV_SessionId,docId,FP_VolumeNumStyle) ;

			if (style == FV_NUMSTYLE_ROMAN_UC)
				toRoman(num, FV_NUMSTYLE_ROMAN_UC, temp) ;

			if (style == FV_NUMSTYLE_ROMAN_LC)
				toRoman(num, FV_NUMSTYLE_ROMAN_LC, temp) ;

			if (style == FV_NUMSTYLE_ALPHA_UC)
				toAlpha(num, FV_NUMSTYLE_ALPHA_UC, temp) ;

			if (style == FV_NUMSTYLE_ALPHA_LC)
				toAlpha(num, FV_NUMSTYLE_ALPHA_LC, temp) ;
			
			F_StrCat(&outText[j], temp) ;
			outText[j+F_StrLen(temp)] = '\0' ;
			i += 9 ;
			j += F_StrLen(temp) ;
			outText[j] = '\0' ;
		}
		else
		{
			if (F_StrCmpN(&referenceText[i], (StringT)"<$chapnum>", 10) == 0)
			{
				num = F_ApiGetInt(FV_SessionId,docId,FP_ChapterNumber) ;

				F_Sprintf(temp, "%d", num) ;

				style = F_ApiGetInt(FV_SessionId,docId,FP_ChapterNumStyle) ;

				if (style == FV_NUMSTYLE_ROMAN_UC)
					toRoman(num, FV_NUMSTYLE_ROMAN_UC, temp) ;

				if (style == FV_NUMSTYLE_ROMAN_LC)
					toRoman(num, FV_NUMSTYLE_ROMAN_LC, temp) ;

				if (style == FV_NUMSTYLE_ALPHA_UC)
					toAlpha(num, FV_NUMSTYLE_ALPHA_UC, temp) ;

				if (style == FV_NUMSTYLE_ALPHA_LC)
					toAlpha(num, FV_NUMSTYLE_ALPHA_LC, temp) ;
			
				F_StrCat(&outText[j], temp) ;
				outText[j+F_StrLen(temp)] = '\0' ;
				i += 10 ;
				j += F_StrLen(temp) ;
				outText[j] = '\0' ;
			}
			else
			{
				if (F_StrCmpN(&referenceText[i], (StringT)"<$paranum>", 10) == 0)
				{
					strTemp = F_ApiGetString(FV_SessionId,docId,FP_PgfNumber) ;
	
					F_StrCat(&outText[j], strTemp) ;
					outText[j+F_StrLen(strTemp)] = '\0' ;
					i += 10 ;
					j += F_StrLen(strTemp) ;
					outText[j] = '\0' ;

					F_Free(strTemp) ;
				}
				else
				{
					if (F_StrCmpN(&referenceText[i], (StringT)"<$pagenum>", 10) == 0)
					{
						F_StrCpy(temp, pageString) ;
						F_StrCat(&outText[j], temp) ;
						outText[j+F_StrLen(temp)] = '\0' ;
						i+= 10 ;
						j += F_StrLen(temp) ;
						outText[j] = '\0' ;
					}
					else
					{
						// Ignore other formatting tags.
						if (referenceText[i] == '<')
						{
							while (i < F_StrLen(referenceText))
							{
								i++ ;

								if (referenceText[i] == '>')
								{
									i++ ;
									break ;
								}
	
								if (referenceText[i] == '<')
									break ;
							}
						}
						else
						{
							if (isprint(referenceText[i]) != 0)
							{
								outText[j] = referenceText[i] ;

								j++ ;
								outText[j] = '\0' ;
							}

							i++ ;
						}
					}
				}
			}
		}
	}

	outText[j] = '@' ;
	outText[j+1] = '\0' ;

	if (F_StrLen(outText) > REFLEN)
	{
		outText[REFLEN-2] = '@' ;
		outText[REFLEN-1] = '\0' ;
	}

//	F_Printf(NULL, (StringT) "outText = %s\n", outText);
}

VoidT toRoman(IntT inNum, IntT inNumbering, StringT outString)
{
	IntT i ;

	outString[0] = '\0' ;

    while( inNum >= 1000 )
    {
        F_StrCat(outString, "m") ;
        inNum -= 1000;
    }

    if( inNum >= 900 )
    {
        F_StrCat(outString, "cm");
        inNum -= 900;
    }

    while( inNum >= 500 )
    {
        F_StrCat(outString, "d") ;
        inNum -= 500;
    }

    if( inNum >= 400 )
    {
        F_StrCat(outString, "cd") ;
        inNum -= 400;
    }

    while( inNum >= 100 )
    {
        F_StrCat(outString, "c");
        inNum -= 100;
    }

    if( inNum >= 90 )
    {
        F_StrCat(outString, "xc") ;
        inNum -= 90;
    }

    while( inNum >= 50 )
    {
        F_StrCat(outString, "l") ;
        inNum -= 50;
    }

    if( inNum >= 40 )
    {
        F_StrCat(outString, "xl");
        inNum -= 40;
    }

    while( inNum >= 10 )
    {
        F_StrCat(outString, "x") ;
        inNum -= 10;
    }

    if( inNum >= 9 )
    {
        F_StrCat(outString, "ix") ;
        inNum -= 9;
    }
    while( inNum >= 5 )
    {
        F_StrCat(outString, "v");
        inNum -= 5;
    }

    if( inNum >= 4 )
    {
        F_StrCat(outString, "iv");
        inNum -= 4;
    }

    while( inNum > 0 )
    {
        F_StrCat(outString, "i") ;
        inNum--;
    }

	if (inNumbering == FV_NUMSTYLE_ROMAN_UC)
		for (i = 0 ; i < F_StrLen(outString) ; i++)
			outString[i] = F_CharToUpper(outString[i]) ;
}

VoidT toAlpha(IntT inNum, IntT inNumbering, StringT outString)
{
	IntT i ;
	UCharT strTemp[2] ;

	outString[0] = '\0' ;

	i = 1 ;
	while ((i * 17576) <= (inNum-703))
		i++ ;
	i-- ;

	if (i > 0)
	{
		strTemp[0] = (i + 96) ;
		strTemp[1] = '\0' ;
		F_StrCat(outString, strTemp) ;
	}

	inNum -= (i * 17576) ;

	i = 1 ;
	while ((i * 676) <= (inNum-27))
		i++ ;
	i-- ;

	if (i > 0)
	{
		strTemp[0] = (i + 96) ;
		strTemp[1] = '\0' ;
		F_StrCat(outString, strTemp) ;
	}

	inNum -= (i * 676) ;

	i = 1 ;
	while ((i * 26) <= (inNum-1))
		i++ ;
	i-- ;

	if (i > 0)
	{
		strTemp[0] = (i + 96) ;
		strTemp[1] = '\0' ;
		F_StrCat(outString, strTemp) ;
	}

	inNum -= (i * 26) ;

	i = inNum ;

	strTemp[0] = (i + 96) ;
	strTemp[1] = '\0' ;
	F_StrCat(outString, strTemp) ;

	if (inNumbering == FV_NUMSTYLE_ALPHA_UC)
		for (i = 0 ; i < F_StrLen(outString) ; i++)
			outString[i] = F_CharToUpper(outString[i]) ;
}

VoidT updateReferenceFormat(F_ObjHandleT docId)
{
	F_ObjHandleT pgId, paraId, pageFrameId ;
	F_ObjHandleT pgfId,  flowId, objectId, textframeId;
	IntT pageType, i ;
	F_TextItemsT textItems;

	pgId = F_ApiGetId(FV_SessionId, docId, FP_FirstRefPageInDoc);

//	F_Printf(NULL, (StringT) "RefName = %s\n", F_ApiGetString(docId, pgId, FP_Name));

	referenceFormat[0] = '\0' ;

	while (pgId)
	{
		if ((F_StrCmp(F_ApiGetString(docId, pgId, FP_Name), (StringT)"IX") == 0) ||
			(F_StrCmp(F_ApiGetString(docId, pgId, FP_Name), (StringT)"Index") == 0))
		{
//			F_Printf(NULL, (StringT) "rPg1Id = %d\n", rPg1Id);

			pageFrameId = F_ApiGetId(docId,pgId,FP_PageFrame);

//			F_Printf(NULL, (StringT) "pageFrameId = %d, Frame name = %s\n", pageFrameId, F_ApiGetString(pageFrameId, pageFrameId, FP_Name));

			textframeId = F_ApiGetId(docId, pageFrameId, FP_FirstGraphicInFrame);

			flowId = F_ApiGetId(docId, textframeId, FP_Flow);

//			F_Printf(NULL, (StringT) "flowId = %d, flow name = %s\n", flowId, F_ApiGetString(docId, flowId, FP_Name)) ;

			textItems = F_ApiGetText(docId, flowId, FTI_String);

//			F_Printf(NULL, (StringT) "textItems.len = %d\n", textItems.len);

			for (i = 0; i < (IntT) textItems.len; i++)
			{
				if ((F_StrSubString(textItems.val[i].u.sdata, (StringT)"<$pagenum>") != -1) ||
					(F_StrSubString(textItems.val[i].u.sdata, (StringT)"<$paranum>") != -1) ||
					(F_StrSubString(textItems.val[i].u.sdata, (StringT)"<$chapnum>") != -1) ||
					(F_StrSubString(textItems.val[i].u.sdata, (StringT)"<$volnum>") != -1))
				{
//					F_Printf(NULL, (StringT) "textItem[%d] = %s\n", i, textItems.val[i].u.sdata);
					F_StrCpy(referenceFormat, textItems.val[i].u.sdata);
					break ;
				}
			}

			break ;
		}
		else
		{
			pgId = F_ApiGetId(docId, pgId, FP_PageNext);

//			F_Printf(NULL, (StringT) "RefName = %s\n", F_ApiGetString(docId, pgId, FP_Name));
		}
	}

	if (referenceFormat[0] == '\0')
		F_StrCpy(referenceFormat, (StringT)"<$pagenum>") ;
}

VoidT openEditorWindow(StringT title)
{
	F_ObjHandleT docId = 0 ;
	F_StringsT apiRangeList;
	IntT	i ;
	HWND	hwnd ;
	HWND	hEditWindow ;
	HWND	hTextEntry ;
	StringT fmtName ;
	F_ObjHandleT markerTypeId, currentTypeId ;
	UCharT posX[800], posY[800], size[80], szSaveOption[800] ;
	LONG style ;

	if ((g_CurrentEditor == MARKER_WINDOW) && (F_StrCmp(title, "emDEX Range Marker") == 0))
	{
		F_ApiAlert((StringT) "Close the emDEX Marker window before opening the range marker window.\n", FF_ALERT_CONTINUE_WARN);
		return ;
	}

	if ((g_CurrentEditor == RANGE_WINDOW) && (F_StrCmp(title, "emDEX Marker") == 0))
	{
		F_ApiAlert((StringT) "Close the emDEX Range Marker window before opening the marker window.\n", FF_ALERT_CONTINUE_WARN);
		return ;
	}

	if (FindWindow(NULL, "emDEX Marker Buttons") != NULL)
	{
		F_ApiAlert((StringT) "Close the emDEX Marker Buttons window before opening the marker window.\n", FF_ALERT_CONTINUE_WARN);
		return ;
	}

	// Only allow one editor window at a time.
	if (g_CurrentEditor != 0)
	{
		hEditWindow = FindWindow(NULL, title) ;

		hTextEntry = GetDlgItem(hEditWindow, 101) ;
		SetFocus(hTextEntry) ;

		return ;
	}

	docId = F_ApiGetId(FV_SessionId, FV_SessionId, FP_ActiveDoc);

	/* Open the dialog box resource */
	if (F_StrCmp(title, "emDEX Marker") == 0)
		dialogId = F_ApiOpenResource(FO_DialogResource, MARKER_DLG);
	else
		dialogId = F_ApiOpenResource(FO_DialogResource, RANGE_DLG);

	// Set the marker text
	if (g_currentMarkerId != 0)
		F_ApiSetString(dialogId, rangeTextId, FP_Text, g_markerName);
	else
		F_ApiSetString(dialogId, rangeTextId, FP_Text, "");

	// Set the button text
	GetPrivateProfileString("Options", "Button0Text", "&Bold", buttons[0].text, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button0Action", "<Bold>", buttons[0].action, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button1Text", "&Emphasis", buttons[1].text, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button1Action", "<Emphasis>", buttons[1].action, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button2Text", "&Default", buttons[2].text, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button2Action", "</>", buttons[2].action, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button3Text", "&No Page", buttons[3].text, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button3Action", "<$nopage>", buttons[3].action, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button4Text", "&Page", buttons[4].text, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button4Action", "<$singlepage>", buttons[4].action, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button5Text", "See &Also", buttons[5].text, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button5Action", "<$nopage><Emphasis>see also </>", buttons[5].action, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button6Text", "&See", buttons[6].text, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button6Action", "<$nopage><Emphasis>see </>", buttons[6].action, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button7Text", "s&tartrange", buttons[7].text, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button7Action", "<$startrange>", buttons[7].action, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button8Text", "end&range", buttons[8].text, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button8Action", "<$endrange>", buttons[8].action, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button9Text", "", buttons[9].text, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button9Action", "", buttons[9].action, 800, "emDEX.ini") ;

	if (F_StrCmp(title, "emDEX Marker") == 0)
	{
		g_CurrentEditor = MARKER_WINDOW ;
		F_ApiModelessDialog(MARKER_DIALOG, dialogId) ;
	}
	else
	{
		g_CurrentEditor = RANGE_WINDOW ;
		F_ApiModelessDialog(RANGE_DIALOG, dialogId) ;
		F_StrCpy(g_markerName, "") ;
		g_currentMarkerId = 0 ;
	}

	hEditWindow = FindWindow(NULL, title) ;

	if (F_StrCmp(productName, (StringT)"FrameMaker+SGML") != 0)
	{
		// Not structured FM, rearrange the dialog accordingly.
		SetWindowPos(GetDlgItem(hEditWindow, 121), HWND_BOTTOM, 0, 0, 0, 0, SWP_HIDEWINDOW) ;
		SetWindowPos(GetDlgItem(hEditWindow, 122), HWND_BOTTOM, 0, 0, 0, 0, SWP_HIDEWINDOW) ;
		SetWindowPos(GetDlgItem(hEditWindow, 108), HWND_BOTTOM, 8, 3, 0, 0, SWP_NOSIZE) ;
		SetWindowPos(GetDlgItem(hEditWindow, 104), HWND_BOTTOM, 8, 20, 278, 33, SWP_SHOWWINDOW) ;

		// Fix up the Z-order
		SetWindowPos(GetDlgItem(hEditWindow, 104), GetDlgItem(hEditWindow, 103), 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE) ;
	}

	// Set the position of the window
	GetPrivateProfileString("Options", "EditorXPos", "100", posX, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "EditorYPos", "100", posY, 800, "emDEX.ini") ;

	SetWindowPos(hEditWindow, HWND_TOP, atoi(posX), atoi(posY), 0, 0, SWP_NOSIZE) ;

	SetFocus(hEditWindow) ;
	SetForegroundWindow(hEditWindow) ;

	SendMessage(GetDlgItem(hEditWindow, 111), WM_SETTEXT, (WPARAM)0, (LPARAM)buttons[0].text) ;
	SendMessage(GetDlgItem(hEditWindow, 112), WM_SETTEXT, (WPARAM)0, (LPARAM)buttons[1].text) ;
	SendMessage(GetDlgItem(hEditWindow, 113), WM_SETTEXT, (WPARAM)0, (LPARAM)buttons[2].text) ;
	SendMessage(GetDlgItem(hEditWindow, 114), WM_SETTEXT, (WPARAM)0, (LPARAM)buttons[3].text) ;
	SendMessage(GetDlgItem(hEditWindow, 115), WM_SETTEXT, (WPARAM)0, (LPARAM)buttons[4].text) ;
	SendMessage(GetDlgItem(hEditWindow, 116), WM_SETTEXT, (WPARAM)0, (LPARAM)buttons[5].text) ;
	SendMessage(GetDlgItem(hEditWindow, 117), WM_SETTEXT, (WPARAM)0, (LPARAM)buttons[6].text) ;
	SendMessage(GetDlgItem(hEditWindow, 118), WM_SETTEXT, (WPARAM)0, (LPARAM)buttons[7].text) ;
	SendMessage(GetDlgItem(hEditWindow, 119), WM_SETTEXT, (WPARAM)0, (LPARAM)buttons[8].text) ;
	SendMessage(GetDlgItem(hEditWindow, 120), WM_SETTEXT, (WPARAM)0, (LPARAM)buttons[9].text) ;

	if ((g_currentMarkerId != 0) && (g_CurrentEditor == MARKER_WINDOW))
	{
		SendMessage(GetDlgItem(hEditWindow, 102), WM_SETTEXT, (WPARAM)0, (LPARAM)"Edit Marker") ;

		EnableWindow(GetDlgItem(hEditWindow, 103), TRUE) ;

		currentTypeId = F_ApiGetId(docId, g_currentMarkerId, FP_MarkerTypeId) ;

		findAndSelectLabel(docId, dialogId, INCLUDE_BOX, currentTypeId) ;
	}
	else
	{
		F_ApiSetString(dialogId, rangeTextId, FP_Text, "");
		SendMessage(GetDlgItem(hEditWindow, 102), WM_SETTEXT, (WPARAM)0, (LPARAM)"New Marker") ;
		EnableWindow(GetDlgItem(hEditWindow, 103), FALSE) ;
	}

	// No text selection
	SendMessage(GetDlgItem(hEditWindow, 101), EM_SETSEL, (WPARAM)0, (LPARAM)0) ;

/*	style = GetWindowLong(hEditWindow,GWL_STYLE);
	style &= (WS_DISABLED) ;
	SetWindowLong(hEditWindow,GWL_STYLE,style); */

	hf = CreateFontIndirect(&lf);

	SendMessage(GetDlgItem(hEditWindow, 101), WM_SETFONT, (WPARAM)hf, MAKELPARAM(1, 0)) ;

	setEditorText() ;

	GetPrivateProfileString("Options", "EditorSize", "2", size, 800, "emDEX.ini") ;
	setEditorSize(atoi(size)) ;

	if (g_CurrentEditor == RANGE_WINDOW)
	{
		rangeListBoxId = F_ApiDialogItemId(dialogId, INCLUDE_BOX);
//		F_ApiSetInt(dialogId, rangeListBoxId, FP_State, defaultType);
		findAndSelectDefaultType(docId, dialogId, INCLUDE_BOX) ;
	}

	F_ApiNotification(FA_Note_BackToUser, True);
}

VoidT setEditorText()
{
	HWND hEditWindow, hTextEntry ;
	StringT string = NULL;
	F_TextLocT tl;
	F_TextRangeT tr;
	F_TextItemsT textItems, rangeTextItems;
	F_ObjHandleT selMarkerId ;
	F_ObjHandleT currentPageId, pageId;
	F_ObjHandleT docId ;
	F_ObjHandleT currentTypeId ;
	F_StringsT apiRangeList ;
	StringListT elementTagList ;
	F_ElementCatalogEntriesT validEntries ;
	IntT	i ;
	F_ObjHandleT elementTagId, refTagId ;
	F_AttributesT attributes;
	F_ElementRangeT er;

	if (g_CurrentEditor == 0)
		return ;

//	F_Printf(NULL, (StringT) "entering setEditorText()\n");

	F_StrCpy(g_markerName, "") ;
	g_currentMarkerId = 0 ;

	docId = F_ApiGetId(FV_SessionId, FV_SessionId, FP_ActiveDoc);

	rangeListBoxId = F_ApiDialogItemId(dialogId, INCLUDE_BOX);
	rangeTextId = F_ApiDialogItemId(dialogId, EXCLUDE_BOX);
	elementTagId = F_ApiDialogItemId(dialogId, 22);

	// Fill in the types combo box
	rangeList = getListOfAllTypes(docId);

	if (F_StrListLen(rangeList) > 1)
	{
		// For combo boxes, the first item is the title (displays in UNIX only)
		F_StrListInsert(rangeList, (StringT)"title", 0);
		F_StrListAppend(rangeList, "Edit ...");

		apiRangeList = convertToApiList(rangeList);
		if(apiRangeList.val == NULL)
		{
			F_ApiAlert((StringT) "Could not convert list: possible memory problem", FF_ALERT_CONTINUE_WARN);
			return;
		}

		/* stuff the listbox with the list of currently define paragraph formats */
		F_ApiSetStrings(dialogId, rangeListBoxId,  FP_Labels, &apiRangeList);
		F_ApiDeallocateStrings(&apiRangeList);
	}

	if (F_StrCmp(productName, (StringT)"FrameMaker+SGML") == 0)
	{
		// Fill in the element tag combo box
		validEntries = F_ApiGetElementCatalog(docId);
		elementTagList = F_StrListNew((UIntT) 1, (UIntT)1);

		tr = F_ApiGetTextRange(FV_SessionId, docId, FP_TextSelection);
		if (tr.beg.objId != 0)
		{

			refTagId = F_ApiGetId(FV_SessionId, docId, FP_FirstElementDefInDoc);
//			F_Printf(NULL, (StringT) "element tag = %s, type = %d\n", F_ApiGetString(docId, elementTagId, FP_Name), F_ApiGetInt(docId, elementTagId, FP_ElementDefType)) ;

			while (refTagId)
			{	
				if (F_ApiGetInt(docId, refTagId, FP_ElementDefType) == FV_FO_MARKER)
				{
					for (i = 0; i < validEntries.len; i++)
					{
						if (F_StrCmp(F_ApiGetString(docId, validEntries.val[i].objId, FP_Name), F_ApiGetString(docId, refTagId, FP_Name)) == 0)
						{
							F_StrListAppend(elementTagList, F_ApiGetString(docId, refTagId, FP_Name));
							break ;
						}
					}
				}

				refTagId = F_ApiGetId(docId, refTagId, FP_NextElementDefInDoc);
//				F_Printf(NULL, (StringT) "element tag = %s, type = %d\n", F_ApiGetString(docId, elementTagId, FP_Name), F_ApiGetInt(docId, elementTagId, FP_ElementDefType)) ;	
			}

/*			for (i = 0; i < validEntries.len; i++)
			{
				refTagId = validEntries.val[i].objId;
	
				if (F_StrCmp(F_ApiGetString(docId, refTagId, FP_Name), "IndexTerm") == 0)
					F_StrListAppend(elementTagList, F_ApiGetString(docId, refTagId, FP_Name));
			} */

			F_StrListSort(elementTagList, &fn);
		}

		// For combo boxes, the first item is the title (displays in UNIX only)
		F_StrListInsert(elementTagList, (StringT)"title", 0);
		F_StrListInsert(elementTagList, (StringT)"<Unstructured>", 1);

		apiRangeList = convertToApiList(elementTagList);

		F_ApiSetStrings(dialogId, elementTagId,  FP_Labels, &apiRangeList);
		F_ApiDeallocateStrings(&apiRangeList);
		F_ApiDeallocateElementCatalogEntries(&validEntries);
	}

	if (g_CurrentEditor == MARKER_WINDOW)
	{
		tr = F_ApiGetTextRange(FV_SessionId, docId, FP_TextSelection);

		textItems = F_ApiGetTextForRange(docId, &tr, FTI_MarkerAnchor);

		if (textItems.len > 0)
		{
			// There are markers in the range selected, find the first one.
			string = F_ApiGetString(docId, textItems.val[0].u.idata, FP_MarkerText) ;

			if (g_markerName != NULL)
				F_Free(g_markerName) ;

			g_markerName = F_StrNew(F_StrLen(string));

			F_StrCpy(g_markerName, string) ;
			g_currentMarkerId = textItems.val[0].u.idata ;

			F_ApiSetString(dialogId, rangeTextId, FP_Text, string);

			if (string != NULL)
				F_Free(string) ;

			F_ApiDeallocateTextItems(&textItems);

			hEditWindow = FindWindow(NULL, "emDEX Marker") ;
			if (hEditWindow == NULL)
				hEditWindow = FindWindow(NULL, "emDEX Range Marker") ;

			SendMessage(GetDlgItem(hEditWindow, 102), WM_SETTEXT, (WPARAM)0, (LPARAM)"Edit Marker") ;

			EnableWindow(GetDlgItem(hEditWindow, 103), TRUE) ;

			currentTypeId = F_ApiGetId(docId, g_currentMarkerId, FP_MarkerTypeId) ;

			findAndSelectLabel(docId, dialogId, INCLUDE_BOX, currentTypeId) ;

			defaultType = getSelectedLabelIndex(dialogId, INCLUDE_BOX) ;

			strElementDefaultType[0] = '\0' ;

			if (F_StrCmp(productName, (StringT)"FrameMaker+SGML") == 0)
			{
				elementTagList = F_StrListNew((UIntT) 1, (UIntT)1);

				// For combo boxes, the first item is the title (displays in UNIX only)
				F_StrListInsert(elementTagList, (StringT)"title", 0);

				if (F_ApiGetId(0, g_currentMarkerId, FP_Element) == 0)
					F_StrListInsert(elementTagList, (StringT)"<Unstructured>", 1);
				else
				{
//					F_StrListInsert(elementTagList, (StringT)"IndexTerm", 1);

					// The ElementDef within the Element contains the name of the current element.
					F_StrListInsert(elementTagList, (StringT)F_ApiGetString(docId, F_ApiGetId(0, F_ApiGetId(0, g_currentMarkerId, FP_Element), FP_ElementDef), FP_Name), 1);
//					F_Printf(NULL, (StringT) "name = %s\n", F_ApiGetString(docId, F_ApiGetId(0, F_ApiGetId(0, g_currentMarkerId, FP_Element), FP_ElementDef), FP_Name)) ;
				}

				F_StrCpy(strElementDefaultType, (StringT)F_ApiGetString(docId, F_ApiGetId(0, F_ApiGetId(0, g_currentMarkerId, FP_Element), FP_ElementDef), FP_Name)) ;

				apiRangeList = convertToApiList(elementTagList);

				F_ApiSetStrings(dialogId, elementTagId,  FP_Labels, &apiRangeList);
				F_ApiDeallocateStrings(&apiRangeList);
			}

			setSelectedLabelIndex(dialogId, ELEMENTTAG_BOX, 1) ;

			return ;
		}
		else
		{
			F_ApiDeallocateTextItems(&textItems);

//			setSelectedLabelIndex(dialogId, ELEMENTTAG_BOX, 1) ;
		}

/*		if (string == NULL)
		{ */
			// No markers in the range.
			selMarkerId = 0 ;

			rangeTextItems = F_ApiGetTextForRange(docId, &tr, FTI_String);

			string = (StringT)CreateStringFromTextItems(rangeTextItems);

			F_ApiDeallocateTextItems(&rangeTextItems);
//		}
/*		else
		{
			F_Printf(NULL, (StringT) "Marker found in range - text = %s\n", string);
		} */
	}

	if (string != NULL)
		F_ApiSetString(dialogId, rangeTextId, FP_Text, string);
	else
		if (g_CurrentEditor == MARKER_WINDOW)
			F_ApiSetString(dialogId, rangeTextId, FP_Text, "");

	hEditWindow = FindWindow(NULL, "emDEX Marker") ;
	if (hEditWindow == NULL)
		hEditWindow = FindWindow(NULL, "emDEX Range Marker") ;

	if (string != NULL)
		F_Free(string) ;

	SendMessage(GetDlgItem(hEditWindow, 102), WM_SETTEXT, (WPARAM)0, (LPARAM)"New Marker") ;
	EnableWindow(GetDlgItem(hEditWindow, 103), FALSE) ;

	// No text selection
	SendMessage(GetDlgItem(hEditWindow, 101), EM_SETSEL, (WPARAM)0, (LPARAM)0) ;

	if (defaultType == -1)
		defaultType = getDefaultSelection(dialogId, INCLUDE_BOX);

	findAndSelectDefaultType(docId, dialogId, INCLUDE_BOX) ;
	findAndSelectDefaultElement(docId, dialogId, ELEMENTTAG_BOX) ;
}

VoidT setEditorSize(IntT size)
{
	HWND hEditWindow ;

	hEditWindow = FindWindow(NULL, "emDEX Marker") ;

	if (hEditWindow == NULL)
		hEditWindow = FindWindow(NULL, "emDEX Range Marker") ;

	if (hEditWindow == NULL)
		return ;

	switch (size)
	{
		case SMALL :
			SetWindowPos(hEditWindow, HWND_TOP, 0, 0, 301, 241, SWP_NOMOVE | SWP_NOZORDER) ;

			SetWindowPos(GetDlgItem(hEditWindow, 101), HWND_TOP, 0, 0, 277, 75, SWP_NOMOVE | SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 102), HWND_TOP, 8, 188, 0, 0, SWP_NOSIZE | SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 103), HWND_TOP, 210, 188, 0, 0, SWP_NOSIZE | SWP_NOZORDER) ;

			SetWindowPos(GetDlgItem(hEditWindow, 111), HWND_TOP, 8, 47, 53, 17,  SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 112), HWND_TOP, 64, 47, 53, 17, SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 113), HWND_TOP, 120, 47, 53, 17, SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 114), HWND_TOP, 176, 47, 53, 17, SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 115), HWND_TOP, 232, 47, 53, 17, SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 116), HWND_TOP, 8, 68, 53, 17, SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 117), HWND_TOP, 64, 68, 53, 17, SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 118), HWND_TOP, 120, 68, 53, 17, SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 119), HWND_TOP, 176, 68, 53, 17, SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 120), HWND_TOP, 232, 68, 53, 17, SWP_NOZORDER) ;

			if (F_StrCmp(productName, (StringT)"FrameMaker+SGML") != 0)
			{
				SetWindowPos(GetDlgItem(hEditWindow, 108), HWND_TOP, 8, 5, 122, 12, SWP_NOZORDER) ;
				SetWindowPos(GetDlgItem(hEditWindow, 104), HWND_TOP, 8, 20, 277, 12, SWP_NOZORDER) ;
			}
			else
			{
				SetWindowPos(GetDlgItem(hEditWindow, 108), HWND_TOP, 150, 5, 135, 12, SWP_NOZORDER) ;
				SetWindowPos(GetDlgItem(hEditWindow, 104), HWND_TOP, 150, 20, 135, 12, SWP_NOZORDER) ;
				SetWindowPos(GetDlgItem(hEditWindow, 122), HWND_TOP, 8, 20, 135, 12, SWP_NOZORDER) ;
			}

			break ;

		case MEDIUM :
			SetWindowPos(hEditWindow, HWND_TOP, 0, 0, 350, 300, SWP_NOMOVE | SWP_NOZORDER) ;

			SetWindowPos(GetDlgItem(hEditWindow, 101), HWND_TOP, 0, 0, 325, 133, SWP_NOMOVE | SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 102), HWND_TOP, 8, 247, 0, 0, SWP_NOSIZE | SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 103), HWND_TOP, 258, 247, 0, 0, SWP_NOSIZE | SWP_NOZORDER) ;

			SetWindowPos(GetDlgItem(hEditWindow, 111), HWND_TOP, 7, 47, 62, 18, SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 112), HWND_TOP, 73, 47, 62, 18, SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 113), HWND_TOP, 139, 47, 62, 18, SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 114), HWND_TOP, 205, 47, 62, 18, SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 115), HWND_TOP, 271, 47, 62, 18, SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 116), HWND_TOP, 7, 68, 62, 18, SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 117), HWND_TOP, 73, 68, 62, 18, SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 118), HWND_TOP, 139, 68, 62, 18, SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 119), HWND_TOP, 205, 68, 62, 18, SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 120), HWND_TOP, 271, 68, 62, 18, SWP_NOZORDER) ;

			if (F_StrCmp(productName, (StringT)"FrameMaker+SGML") != 0)
			{
				SetWindowPos(GetDlgItem(hEditWindow, 108), HWND_TOP, 8, 5, 122, 12, SWP_NOZORDER) ;
				SetWindowPos(GetDlgItem(hEditWindow, 104), HWND_TOP, 8, 20, 325, 12, SWP_NOZORDER) ;
			}
			else
			{
				SetWindowPos(GetDlgItem(hEditWindow, 108), HWND_TOP, 175, 5, 159, 12, SWP_NOZORDER) ;
				SetWindowPos(GetDlgItem(hEditWindow, 104), HWND_TOP, 175, 20, 159, 12, SWP_NOZORDER) ;
				SetWindowPos(GetDlgItem(hEditWindow, 122), HWND_TOP, 8, 20, 159, 12, SWP_NOZORDER) ;
			}

			break ;

		case LARGE :
			SetWindowPos(hEditWindow, HWND_TOP, 0, 0, 399, 350, SWP_NOMOVE | SWP_NOZORDER) ;

			SetWindowPos(GetDlgItem(hEditWindow, 101), HWND_TOP, 0, 0, 374, 185, SWP_NOMOVE | SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 102), HWND_TOP, 8, 297, 0, 0, SWP_NOSIZE | SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 103), HWND_TOP, 307, 297, 0, 0, SWP_NOSIZE | SWP_NOZORDER) ;

			SetWindowPos(GetDlgItem(hEditWindow, 111), HWND_TOP, 9, 46, 71, 19, SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 112), HWND_TOP, 84, 46, 71, 19, SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 113), HWND_TOP, 159, 46, 71, 19, SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 114), HWND_TOP, 234, 46, 71, 19, SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 115), HWND_TOP, 309, 46, 71, 19, SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 116), HWND_TOP, 9, 68, 71, 19, SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 117), HWND_TOP, 84, 68, 71, 19, SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 118), HWND_TOP, 159, 68, 71, 19, SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 119), HWND_TOP, 234, 68, 71, 19, SWP_NOZORDER) ;
			SetWindowPos(GetDlgItem(hEditWindow, 120), HWND_TOP, 309, 68, 71, 19, SWP_NOZORDER) ;

			if (F_StrCmp(productName, (StringT)"FrameMaker+SGML") != 0)
			{
				SetWindowPos(GetDlgItem(hEditWindow, 108), HWND_TOP, 8, 5, 122, 12, SWP_NOZORDER) ;
				SetWindowPos(GetDlgItem(hEditWindow, 104), HWND_TOP, 8, 20, 373, 12, SWP_NOZORDER) ;
			}
			else
			{
				SetWindowPos(GetDlgItem(hEditWindow, 108), HWND_TOP, 198, 5, 159, 12, SWP_NOZORDER) ;
				SetWindowPos(GetDlgItem(hEditWindow, 104), HWND_TOP, 198, 20, 183, 12, SWP_NOZORDER) ;
				SetWindowPos(GetDlgItem(hEditWindow, 122), HWND_TOP, 8, 20, 183, 12, SWP_NOZORDER) ;
			}

			break ;
	}

	// Fix up the Z-order
	SetWindowPos(GetDlgItem(hEditWindow, 122), GetDlgItem(hEditWindow, 103), 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE) ;
}

VoidT saveButtons()
{
	UCharT	writeString[2000] ;
	IntT	Pos ;
	IntT	i ;
	IntT	SaveHandle ;
	UCharT	CharWrite[2] ;
	UCharT	FileName[256] ;	
	OPENFILENAME OpenFileName ;
	BoolT	RetCode ;

	FileName[0] = '\0' ;
	writeString[0] = '\0' ;

	F_StrCpy(FileName, "*.EMB") ;

	OpenFileName.lStructSize = sizeof(OpenFileName);
	OpenFileName.hwndOwner = GetActiveWindow() ;
	OpenFileName.hInstance = NULL;
	OpenFileName.lpstrFilter = "*.*";     // file types
	OpenFileName.lpstrCustomFilter = NULL;
	OpenFileName.nFilterIndex = 0;                  // index of first one
	OpenFileName.nMaxCustFilter = 0;
	OpenFileName.lpstrFile = FileName;         // initial file suggested
	OpenFileName.lpstrFileTitle = FileName ;
	OpenFileName.nMaxFile = sizeof(FileName);
	OpenFileName.nMaxFileTitle = sizeof(FileName);
	OpenFileName.lpstrTitle = "Save Buttons";    // dlg title
	OpenFileName.lpstrInitialDir = RegValue ;   // initial path
	OpenFileName.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT ;
	OpenFileName.lpstrDefExt = NULL;                 // default extension
	OpenFileName.lpfnHook = NULL;

	RetCode = GetSaveFileName((LPOPENFILENAME)&OpenFileName) ;

	if (RetCode == 0)
		/* No file name entered, or Cancel pressed. */
		return ;

	GetPrivateProfileString("Options", "Button0Text", "&Bold", buttons[0].text, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button0Action", "<Bold>", buttons[0].action, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button1Text", "&Emphasis", buttons[1].text, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button1Action", "<Emphasis>", buttons[1].action, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button2Text", "&Default", buttons[2].text, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button2Action", "</>", buttons[2].action, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button3Text", "&No Page", buttons[3].text, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button3Action", "<$nopage>", buttons[3].action, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button4Text", "&Page", buttons[4].text, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button4Action", "<$singlepage>", buttons[4].action, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button5Text", "See &Also", buttons[5].text, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button5Action", "<$nopage><Emphasis>see also </>", buttons[5].action, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button6Text", "&See", buttons[6].text, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button6Action", "<$nopage><Emphasis>see </>", buttons[6].action, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button7Text", "s&tartrange", buttons[7].text, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button7Action", "<$startrange>", buttons[7].action, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button8Text", "end&range", buttons[8].text, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button8Action", "<$endrange>", buttons[8].action, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button9Text", "", buttons[9].text, 800, "emDEX.ini") ;
	GetPrivateProfileString("Options", "Button9Action", "", buttons[9].action, 800, "emDEX.ini") ;

	/* Put an identifier in the file. */
	F_StrCpy(writeString, "emDEXButtons") ;

	Pos = F_StrLen(writeString) ;

	for (i = 0 ; i < 10 ; i++)
	{
		F_StrCpy(&writeString[Pos], buttons[i].text) ;
		Pos += F_StrLen(buttons[i].text) ;
		writeString[Pos] = '@' ;
		writeString[Pos+1] = '!' ;
		Pos += 2 ;
		F_StrCpy(&writeString[Pos], buttons[i].action) ;
		Pos += F_StrLen(buttons[i].action) ;
		writeString[Pos] = '@' ;
		writeString[Pos+1] = '!' ;
		Pos += 2 ;
	}

	/* Delete and rewrite the save file. */
	remove(FileName) ;

	_chmod(FileName, _S_IREAD | _S_IWRITE) ;

	SaveHandle = _open(FileName, _O_CREAT | _O_RDWR) ;

	if (SaveHandle == -1)
	{
		/* Error creating the file. */
		F_ApiAlert((StringT)"Error opening the save file.", FF_ALERT_CONTINUE_WARN) ;
		return ;
	}

	_chmod(FileName, _S_IREAD | _S_IWRITE) ;

	write(SaveHandle, writeString, /*F_StrLen(writeString)*/Pos) ;

	close (SaveHandle) ;
}

VoidT loadButtons()
{
	UCharT	ReadString[2001] ;
	UCharT	TempString[2001] ;
	UCharT	ReadNum[2] ;
	IntT	Pos ;
	IntT	i, j, k ;
	IntT	RestoreHandle ;
	UCharT	FileName[256] ;	
	OPENFILENAME OpenFileName ;
	BoolT	RetCode ;

	FileName[0] = '\0' ;
	ReadString[0] = '\0' ;

	F_StrCpy(FileName, "*.EMB") ;

	OpenFileName.lStructSize = sizeof(OpenFileName);
	OpenFileName.hwndOwner = GetActiveWindow() ;
	OpenFileName.hInstance = NULL;
	OpenFileName.lpstrFilter = "*.*";     // file types
	OpenFileName.lpstrCustomFilter = NULL;
	OpenFileName.nFilterIndex = 0;                  // index of first one
	OpenFileName.nMaxCustFilter = 0;
	OpenFileName.lpstrFile = FileName;         // initial file suggested
	OpenFileName.lpstrFileTitle = FileName ;
	OpenFileName.nMaxFile = sizeof(FileName);
	OpenFileName.nMaxFileTitle = sizeof(FileName);
	OpenFileName.lpstrTitle = "Load Buttons";    // dlg title
	OpenFileName.lpstrInitialDir = RegValue;   // initial path
	OpenFileName.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST;
	OpenFileName.lpstrDefExt = NULL;                 // default extension
	OpenFileName.lpfnHook = NULL;

	RetCode = GetOpenFileName((LPOPENFILENAME)&OpenFileName) ;

	if (RetCode == 0)
		/* No file name entered, or Cancel pressed. */
		return ;

	RestoreHandle = _open(FileName, _O_RDONLY) ;

	if (RestoreHandle == -1)
	{
		/* Error creating the file. */
		F_ApiAlert((StringT)"Error reading the save file.", FF_ALERT_CONTINUE_WARN) ;
		return ;
	}

	_chmod(FileName, _S_IREAD | _S_IWRITE) ;

	k = read(RestoreHandle, ReadString, 800) ;

	close(RestoreHandle) ;

	ReadString[k] = '\0' ;

	/* Check the identifier in the file. */
	if (F_StrCmpN(ReadString, "emDEXButtons", 12) != 0)
	{
		F_ApiAlert((StringT)"Not an emDEX saved file.", FF_ALERT_CONTINUE_WARN) ;
		return ;
	}

	Pos = 12 ;

	/* Load the array from the file. */
	for (i = 0 ; i < 10 ; i++)
	{
		TempString[0] = '\0' ;

		for (j = 0 ; ; j++)
		{
			if ((ReadString[Pos] == '@') && (ReadString[Pos+1] == '!'))
			{
				F_StrCpy(buttons[i].text, TempString) ;

				Pos += 2 ;
				break ;
			}

			TempString[j] = ReadString[Pos] ;
			TempString[j+1] = '\0' ;
			Pos++ ;
		}

		TempString[0] = '\0' ;

		for (j = 0 ; ; j++)
		{
			if ((ReadString[Pos] == '@') && (ReadString[Pos+1] == '!'))
			{
				F_StrCpy(buttons[i].action, TempString) ;

				Pos += 2 ;
				break ;
			}

			TempString[j] = ReadString[Pos] ;
			TempString[j+1] = '\0' ;
			Pos++ ;
		}

	}

	WritePrivateProfileString("Options", "Button0Text", buttons[0].text, "emDEX.ini");
	WritePrivateProfileString("Options", "Button0Action", buttons[0].action, "emDEX.ini");
	WritePrivateProfileString("Options", "Button1Text", buttons[1].text, "emDEX.ini");
	WritePrivateProfileString("Options", "Button1Action", buttons[1].action, "emDEX.ini");
	WritePrivateProfileString("Options", "Button2Text", buttons[2].text, "emDEX.ini");
	WritePrivateProfileString("Options", "Button2Action", buttons[2].action, "emDEX.ini");
	WritePrivateProfileString("Options", "Button3Text", buttons[3].text, "emDEX.ini");
	WritePrivateProfileString("Options", "Button3Action", buttons[3].action, "emDEX.ini");
	WritePrivateProfileString("Options", "Button4Text", buttons[4].text, "emDEX.ini");
	WritePrivateProfileString("Options", "Button4Action", buttons[4].action, "emDEX.ini");
	WritePrivateProfileString("Options", "Button5Text", buttons[5].text, "emDEX.ini");
	WritePrivateProfileString("Options", "Button5Action", buttons[5].action, "emDEX.ini");
	WritePrivateProfileString("Options", "Button6Text", buttons[6].text, "emDEX.ini");
	WritePrivateProfileString("Options", "Button6Action", buttons[6].action, "emDEX.ini");
	WritePrivateProfileString("Options", "Button7Text", buttons[7].text, "emDEX.ini");
	WritePrivateProfileString("Options", "Button7Action", buttons[7].action, "emDEX.ini");
	WritePrivateProfileString("Options", "Button8Text", buttons[8].text, "emDEX.ini");
	WritePrivateProfileString("Options", "Button8Action", buttons[8].action, "emDEX.ini");
	WritePrivateProfileString("Options", "Button9Text", buttons[9].text, "emDEX.ini");
	WritePrivateProfileString("Options", "Button9Action", buttons[9].action, "emDEX.ini");
}

// End of File