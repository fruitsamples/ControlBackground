/*	File:		ControlBackground.c	Contains:	ControlBackground, a quickie sample which shows how to				affect the background color of a control when drawing it.	Written by: Pete Gontier		Copyright:	Copyright � 1997-1999 by Apple Computer, Inc., All Rights Reserved.				You may incorporate this Apple sample source code into your program(s) without				restriction. This Apple sample source code has been provided "AS IS" and the				responsibility for its operation is yours. You are not permitted to redistribute				this Apple sample source code as "Apple sample source code" after having made				changes. If you're going to re-distribute the source, we require that you make				it clear in the source that the code was descended from Apple sample source				code, but that you've made changes.	Change History (most recent first):				7/19/1999	Karl Groethe	Updated for Metrowerks Codewarror Pro 2.1				*/#include <Fonts.h>#include <Dialogs.h>#include <QDOffscreen.h>#include <Gestalt.h>#include <Appearance.h>#include <Sound.h>#include <LowMem.h>#include "MoveableModalDialog.h"enum{	kDialogItemIndex_DrawButton = 3,	kDialogItemIndex_CheckBox};static GrafPtr LMGetWMgrCPort (void) { return *(GrafPtr*)0x0D2C; }#define IsColorGrafPort(port) (((port)->portBits.rowBytes & 0xC000) == 0xC000)static pascal OSErr InitMac (void){	MaxApplZone ( );	InitGraf (&(qd.thePort));	InitFonts ( );	InitWindows ( );	InitMenus ( );	TEInit ( );	InitDialogs (nil);	return noErr;}static pascal OSStatus IsAppearancePresent (Boolean *haveAppearance){	OSStatus err = noErr;	long response;	if (!(err = Gestalt (gestaltAppearanceAttr,&response)))		*haveAppearance = response & (1 << gestaltAppearanceExists);	else if (err == gestaltUndefSelectorErr)	{		*haveAppearance = false;		err = noErr;	}	return err;}static pascal ControlRef NewUserPaneControl	(WindowPtr window, const Rect *bounds, Boolean visible, UInt16 featureFlags){	return NewControl (window,bounds,"\p",visible,featureFlags,0,0,kControlUserPaneProc,0);}static pascal OSErr AtLeastOneParentHasSpecialBackground (ControlRef control, Boolean *alophsb){	//	//	This function walks up the control hierarchy looking for	//	a control which provides a special background. We need	//	to know this in order to decide whether the only way for	//	us to affect the background color is to go to the trouble	//	of creating a user pane control.	//	OSErr err = noErr;	ControlHandle root;	*alophsb = false;	if (!(err = GetRootControl ((**control).contrlOwner, &root)))	{		if (!root)			err = errNoRootControl;		else		{			ControlRef scan = control;			while (scan != root)			{				UInt32 features;				err = GetSuperControl (scan,&scan);				if (err) break;				err = GetControlFeatures (scan,&features);				if (err) break;				if (features & kControlHasSpecialBackground)				{					//					//	In at least 8.6 and earlier (and perhaps later),					//	CreateRootControl spuriously sets					//	kControlHasSpecialBackground. [Radar 2324373]					//	We work around this weirdness here by checking					//	to see if a proc has been assigned. If the bug					//	gets fixed, this code will be a little less					//	than optimally efficient but will still be					//	correct (I hope).					//					if (scan != root)					{						*alophsb = true;						break;					}					else					{						ControlUserPaneBackgroundUPP	upp;						Size							actualSize;						if (!(err = GetControlData (scan,kControlNoPart,							kControlUserPaneBackgroundProcTag,sizeof(upp),(Ptr)&upp,&actualSize)))						{							if (sizeof (upp) != actualSize)								err = paramErr;							else if (upp)							{								*alophsb = true;								break;							}						}					}				}			}		}	}	return noErr;}static pascal void ControlUserPaneBackgroundProc (ControlHandle control, ControlBackgroundPtr){	//	//	This function is called by the user pane CDEF. Its	//	job is to setup the background of the current graphics	//	port in some "special" way. All we want is to set the color.	//	The appearance-savvy thing to do when setting the color is	//	to also set the pattern (and vice versa).	//	BackPat (&(qd.white));	RGBBackColor ((const RGBColor *) GetControlReference (control));}#pragma mark -static pascal OSErr Draw1ControlWithBackgroundColorViaWindowColorTable	(ControlRef control, const RGBColor *rgb){	//	//	Walk the color table of the given control's	//	window looking for the content color.	//	AuxWinHandle	auxWinHandle;	WindowRef		contrlOwner					= (**control).contrlOwner;	Boolean			oldColorTableWasDefault		= GetAuxWin (contrlOwner,&auxWinHandle);	WCTabHandle		winCTabHandle				= (WCTabHandle) ((**auxWinHandle).awCTable);	short			ctIndex						= (**winCTabHandle).ctSize;	while (ctIndex > -1)	{		ColorSpecPtr rgbScan = ctIndex + (**winCTabHandle).ctTable;		if (rgbScan->value == wContentColor)		{			RGBColor savedRGB = rgbScan->rgb;			rgbScan->rgb = *rgb;			CTabChanged ((CTabHandle) winCTabHandle);			Draw1Control (control);			// assume memory has moved and rgbScan has become stale			(**winCTabHandle).ctTable [ctIndex].rgb = savedRGB;			CTabChanged ((CTabHandle) winCTabHandle);			return noErr; // bail out of function without exiting loop		}		--ctIndex;	}	//	//	If the content color was found, the rest of this function	//	will not execute; there is a return statement inside the	//	loop, above.	//	//	We take a lack of a content color to mean that we've been	//	passed a control which lives in a bogus window, and we claim	//	this is a parameter error. However, it's conceivable that	//	we could deal with this by either creating a color table	//	for this window or adding an entry to the table which is	//	already there. This kind of thing is beyond the scope of	//	this sample. You only have to worry about this issue if	//	you are creating your own window color tables and don't	//	always include a content color. Resource editors generally	//	create an entry for the content color even if you	//	only customized some other color in the table.	//	return paramErr;}static pascal OSErr Draw1ControlWithBackgroundColorViaOwningGrafPort	(ControlRef control, const RGBColor *rgb){	//	//	Our job here is simple; set the background color of the	//	graphics port into which the control will be drawn and	//	then draw the control. The interesting wrinkle is that	//	if the control's owning port is monochrome, it will draw	//	into the Window Manager's color port. Nasty! The only	//	way to do the right thing here is to muck with low memory.	//	There is not even an LM accessor for what we need to do!	//	This is actively Carbon-hostile, which means this sample	//	will have to be updated again for Carbon. Sigh...	//	RGBColor	preservedBackColor;	GrafPtr		preservedPort			= qd.thePort;	GrafPtr		targetPort				= (**control).contrlOwner;	if (!IsColorGrafPort (targetPort))		targetPort = LMGetWMgrCPort ( );	preservedBackColor = ((CGrafPtr) targetPort)->rgbBkColor;	SetPort (targetPort);	RGBBackColor (rgb);	Draw1Control (control);	RGBBackColor (&preservedBackColor);	return noErr;}static pascal OSErr Draw1ControlWithBackgroundColorViaUserPane	(ControlRef child, const RGBColor *rgb){	//	//	This is the most complicated part of this sample.	//	We call the control which we want to draw "the child".	//	We create a user pane control which will be the parent of	//	the child. We associate a special background setup proc	//	with the user pane. The user pane is considered to be	//	"behind" the child for purposes of the special background	//	proc, so the user pane gets to dictate the background for	//	the child. After we're done drawing the child, we restore	//	it to its original parent and blow away the user pane.	//	OSErr err = noErr;	ControlRef parent;	if (!(err = GetSuperControl (child,&parent)))	{				Rect		userPaneBounds	= (**child).contrlRect;				GrafPtr		userPaneOwner	= (**child).contrlOwner;		const	UInt32		userPaneFF		= kControlSupportsEmbedding | kControlHasSpecialBackground;				ControlRef	userPane		= NewUserPaneControl (userPaneOwner,&userPaneBounds,true,userPaneFF);		if (!userPane)			err = nilHandleErr;		else		{			OSErr err2;			if (!(err = EmbedControl (userPane,parent)))			if (!(err = EmbedControl (child,userPane)))			{				ControlUserPaneBackgroundUPP upp =					NewControlUserPaneBackgroundProc (ControlUserPaneBackgroundProc);				if (!upp)					err = nilHandleErr;				else				{					if (!(err = SetControlData (userPane,kControlNoPart,						kControlUserPaneBackgroundProcTag,sizeof(upp),(Ptr)&upp)))					{						SetControlReference (userPane,(long)rgb);						Draw1Control (child);					}					DisposeRoutineDescriptor (upp);				}				err2 = EmbedControl (child,parent);				if (!err) err = err2;			}			err2 = SetControlVisibility (userPane,false,false);			if (!err) err = err2;			DisposeControl (userPane);		}	}	return noErr;}static pascal OSErr Draw1ControlWithBackgroundColor (ControlRef control, const RGBColor *rgb){	//	//	This function decides which technique to use for drawing	//	the control with the appropriate background based on what	//	APIs are available and the state of the control's owning	//	window.	//	OSErr err = noErr;	Boolean haveAppearance;	if (!(err = IsAppearancePresent (&haveAppearance)))	{		if (!haveAppearance)			err = Draw1ControlWithBackgroundColorViaWindowColorTable (control,rgb);		else		{			ControlHandle rootControl;			err = GetRootControl ((**control).contrlOwner, &rootControl);			if (err == errNoRootControl || !rootControl)				err = Draw1ControlWithBackgroundColorViaOwningGrafPort (control,rgb);			else			{				Boolean special;				if (!(err = AtLeastOneParentHasSpecialBackground (control,&special)))				{					if (!special)						err = Draw1ControlWithBackgroundColorViaOwningGrafPort (control,rgb);					else						err = Draw1ControlWithBackgroundColorViaUserPane (control,rgb);				}			}		}	}	return err;}#pragma mark -void main (void){	if (InitMac ( ))		SysBeep (10);	else	{		DialogRef dlgRef = GetNewDialog (129,nil,(WindowRef)-1);		if (dlgRef)		{			short itemHit;			SetDialogDefaultItem (dlgRef,kStdOkItemIndex);			ShowWindow (dlgRef);			InitCursor ( );			do			{				MoveableModalDialog (NewModalFilterProc(StdFilterProc),&itemHit);				if (itemHit == kDialogItemIndex_DrawButton)				{					short iType; Handle iHandle; Rect iRect;					RGBColor periwinkle = { 0x5555,0x5555,0xFFFF };					GetDialogItem (dlgRef,kDialogItemIndex_CheckBox,&iType,&iHandle,&iRect);					if (Draw1ControlWithBackgroundColor ((ControlRef)iHandle, &periwinkle))						SysBeep (10);				}			}			while (itemHit != kStdOkItemIndex);			DisposeDialog (dlgRef);		}	}}