/* Copyright (C) 2001 by Alex Kompel <shurikk@pacbell.net> */
// winhack.cpp : Defines the entry point for the application.
//

#include "winMS.h"
#include "hack.h"
#include "dlb.h"
#include "resource.h"
#include "mhmain.h"
#include "mhmap.h"

#ifdef OVL0
#define SHARED_DCL
#else
#define SHARED_DCL extern
#endif

SHARED_DCL char orgdir[PATHLEN];	/* also used in pcsys.c, amidos.c */

extern void FDECL(nethack_exit,(int));
static TCHAR* _get_cmd_arg(TCHAR* pCmdLine);

// Global Variables:
NHWinApp _nethack_app;

// Foward declarations of functions included in this code module:
BOOL				InitInstance(HINSTANCE, int);

static void win_hack_init(int, char **);
static void __cdecl mswin_moveloop(void *);
static void	run_recover();
static BOOL setMapTiles(const char* fname);

extern void FDECL(pcmain, (int,char **));

#define MAX_CMDLINE_PARAM 255

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPWSTR     lpCmdLine,
                     int       nCmdShow)
{
	INITCOMMONCONTROLSEX InitCtrls;
	HWND nethackWnd;
	int argc;
	char* argv[MAX_CMDLINE_PARAM];
	size_t len;
	TCHAR* p;
	TCHAR wbuf[NHSTR_BUFSIZE];
	char buf[NHSTR_BUFSIZE];

	/* ensure that we don't access violate on a panic() */
	windowprocs.win_raw_print = mswin_raw_print;
	windowprocs.win_raw_print_bold = mswin_raw_print_bold;

	/* init applicatio structure */
	_nethack_app.hApp = hInstance;
	_nethack_app.nCmdShow = nCmdShow;
	_nethack_app.hAccelTable = LoadAccelerators(hInstance, (LPCTSTR)IDC_WINHACK);
	_nethack_app.hMainWnd = NULL;
	_nethack_app.hPopupWnd = NULL;
	_nethack_app.hMenuBar = NULL;
	_nethack_app.bmpTiles = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_TILES));
	if( _nethack_app.bmpTiles==NULL ) panic("cannot load tiles bitmap");
	_nethack_app.bmpPetMark = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_PETMARK));
	if( _nethack_app.bmpPetMark==NULL ) panic("cannot load pet mark bitmap");
	_nethack_app.bmpMapTiles = _nethack_app.bmpTiles;
	_nethack_app.mapTile_X = TILE_X;
	_nethack_app.mapTile_Y = TILE_Y;
	_nethack_app.mapTilesPerLine = TILES_PER_LINE;
	_nethack_app.bNoHScroll = FALSE;
	_nethack_app.bNoVScroll = FALSE;
#ifndef WIN_CE_2xx
	_nethack_app.bCmdPad = TRUE;
#else
	_nethack_app.bCmdPad = FALSE;
#endif

	// check for running nethack programs
	nethackWnd = FindWindow(szMainWindowClass, NULL);
	if( nethackWnd ) {
		// bring on top
		SetForegroundWindow(nethackWnd);
		return FALSE;
	}

	// init controls
	ZeroMemory(&InitCtrls, sizeof(InitCtrls));
	InitCtrls.dwSize = sizeof(InitCtrls);
	InitCtrls.dwICC = ICC_LISTVIEW_CLASSES;
	if( !InitCommonControlsEx(&InitCtrls) ) {
		MessageBox(NULL, TEXT("Cannot init common controls"), TEXT("ERROR"), MB_OK | MB_ICONSTOP);
		return FALSE;
	}

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow)) 
	{
		return FALSE;
	}

	/* get command line parameters */	
	p = _get_cmd_arg(
#ifdef WIN_CE_2xx
		lpCmdLine
#else
		GetCommandLine()
#endif
		);
	for( argc = 1; p && argc<MAX_CMDLINE_PARAM; argc++ ) {
		len = _tcslen(p);
		if( len>0 ) {
			argv[argc] = _strdup( NH_W2A(p, buf, BUFSZ) );
		} else {
			argv[argc] = "";
		}
		p = _get_cmd_arg(NULL);
	}
	GetModuleFileName(NULL, wbuf, BUFSZ);
	argv[0] = _strdup(NH_W2A(wbuf, buf, BUFSZ));

	pcmain(argc,argv);

	moveloop();

	return 0;
}

//
//   FUNCTION: InitInstance(HANDLE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   return TRUE;
}

PNHWinApp GetNHApp()
{
	return &_nethack_app;
}

static int
eraseoldlocks()
{
	register int i;

	/* cannot use maxledgerno() here, because we need to find a lock name
	 * before starting everything (including the dungeon initialization
	 * that sets astral_level, needed for maxledgerno()) up
	 */
	for(i = 1; i <= MAXDUNGEON*MAXLEVEL + 1; i++) {
		/* try to remove all */
		set_levelfile_name(lock, i);
		(void) unlink(fqname(lock, LEVELPREFIX, 0));
	}
	set_levelfile_name(lock, 0);
	if(unlink(fqname(lock, LEVELPREFIX, 0)))
		return 0;				/* cannot remove it */
	return(1);					/* success! */
}

void
getlock()
{
	const char *fq_lock;
	char tbuf[BUFSZ];
	TCHAR wbuf[BUFSZ];
	HANDLE f;
	int fd;
	int choice;

	/* regularize(lock); */ /* already done in pcmain */
	Sprintf(tbuf,fqname(lock, LEVELPREFIX, 0));
	set_levelfile_name(lock, 0);
	fq_lock = fqname(lock, LEVELPREFIX, 1);

	f = CreateFile(
			NH_A2W(fq_lock, wbuf, BUFSZ),
			GENERIC_READ,
			0,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL);
	if( f==INVALID_HANDLE_VALUE ) {
		if(GetLastError()==ERROR_FILE_NOT_FOUND) goto gotlock;    /* no such file */
		error("Cannot open %s", fq_lock);
	}

	CloseHandle(f);

	/* prompt user that the game alredy exist */
	choice = MessageBox(
		GetNHApp()->hMainWnd,
		TEXT("There is already a game in progress under your name.\n")
		TEXT("If this is unexpected, you may be able to use \n")
		TEXT("\"recover\" to get it back.")
		TEXT("\nDo you want to recover the old game?"),
		TEXT("Nethack"),
		MB_YESNOCANCEL );
	switch(choice) {
	case IDYES:
		run_recover();
		break;

	case IDNO: 
		if(eraseoldlocks()) {
			goto gotlock;
		} else {
			error("Couldn't destroy old game.");
		}
		break;

	case IDCANCEL:
		SetLastError(0);
		error("%s", "Good-bye.");
		break;
	};

gotlock:
	fd = creat(fq_lock, FCMASK);
	if(fd == -1) {
		error("cannot creat lock file (%s.)", fq_lock);
	} else {
		if(write(fd, (char *) &hackpid, sizeof(hackpid))
		    != sizeof(hackpid)){
			error("cannot write lock (%s)", fq_lock);
		}
		if(close(fd) == -1) {
			error("cannot close lock (%s)", fq_lock);
		}
	}
}

/* misc functions */
void
error VA_DECL(const char *,s)
	TCHAR  wbuf[1024];
	char   buf[1024];
	DWORD last_error = GetLastError();

	VA_START(s);
	VA_INIT(s, const char *);
	/* error() may get called before tty is initialized */
	if (iflags.window_inited) end_screen();
	
	vsprintf(buf, s, VA_ARGS);
	NH_A2W(buf, wbuf, sizeof(wbuf)/sizeof(wbuf[0]));
	if( last_error>0 ) {
		LPVOID lpMsgBuf;
		if( FormatMessage( 
				FORMAT_MESSAGE_ALLOCATE_BUFFER | 
				FORMAT_MESSAGE_FROM_SYSTEM | 
				FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL,
				last_error,
				0, // Default language
				(LPTSTR) &lpMsgBuf,
				0,
				NULL 
			)
			)
		{
			
			_tcsncat(wbuf, TEXT("\nSystem Error: "), sizeof(wbuf)/sizeof(wbuf[0]) - _tcslen(wbuf) );
			_tcsncat(wbuf, lpMsgBuf, sizeof(wbuf)/sizeof(wbuf[0]) - _tcslen(wbuf) );

			// Free the buffer.
			LocalFree( lpMsgBuf );
		}
	}
	VA_END();

	MessageBox( NULL, wbuf, TEXT("Error"), MB_OK | MB_ICONERROR );
	
	exit(EXIT_FAILURE);
}

TCHAR* _get_cmd_arg(TCHAR* pCmdLine)
{
    static TCHAR* pArgs = NULL;
    TCHAR  *pRetArg;
    BOOL   bQuoted;

    if( !pCmdLine && !pArgs ) return NULL;
    if( !pArgs ) pArgs = pCmdLine;

    /* skip whitespace */
    for(pRetArg = pArgs; *pRetArg && _istspace(*pRetArg); pRetArg = CharNext(pRetArg));
	if( !*pRetArg ) {
		pArgs = NULL;
		return NULL;
	}

    /* check for quote */
    if( *pRetArg==TEXT('"') ) {
            bQuoted = TRUE;
            pRetArg = CharNext(pRetArg);
			pArgs = _tcschr(pRetArg, TEXT('"'));
	} else {
		/* skip to whitespace */
		for(pArgs = pRetArg; *pArgs && !_istspace(*pArgs); pArgs = CharNext(pArgs));
	}
	
	if( pArgs && *pArgs ) {
		TCHAR* p;
		p = pArgs;
		pArgs = CharNext(pArgs);
		*p = (TCHAR)0;
	} else {
		pArgs = NULL;
	}

	return pRetArg;
}

static void	run_recover()
{
	TCHAR wbuf[BUFSZ];
	TCHAR cmd_name[BUFSZ];
	char  cmd_line[BUFSZ];
	DWORD ret;
	PROCESS_INFORMATION pi;

	_stprintf(cmd_name, TEXT("%s\\recover.exe"), NH_A2W(hackdir, wbuf, BUFSZ) );
	sprintf(cmd_line, "-d \"%s\" \"%s-%s\"", 
		hackdir,
		get_username(0),
		plname
	);
	ret = CreateProcess(
		cmd_name,
		NH_A2W(cmd_line, wbuf, BUFSZ),
		NULL,
		NULL,
		0,
		0,
		NULL,
		NULL,
		NULL,
		&pi );
	if( ret==0 ) {
		error( "cannot run recover.exe" );
	}

	WaitForSingleObject(pi.hProcess, INFINITE);
	GetExitCodeProcess(pi.hProcess, &ret );
	/* do something with exit status? */
}

/* 
 * Strip out troublesome file system characters.
 */

void
nt_regularize(s)	/* normalize file name */
register char *s;
{
	register unsigned char *lp;

	for (lp = s; *lp; lp++)
	    if ( *lp == '?' || *lp == '"' || *lp == '\\' ||
		 *lp == '/' || *lp == '>' || *lp == '<'  ||
		 *lp == '*' || *lp == '|' || *lp == ':'  || (*lp > 127))
			*lp = '_';
}

void win32_abort()
{

#ifdef WIZARD
   	if (wizard)
		DebugBreak();
#endif
	abort();
}


