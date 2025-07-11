/* FCE Ultra - NES/Famicom Emulator
 *
 * Copyright notice for this file:
 *  Copyright (C) 2020 mjbudd77
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
// ConsoleWindow.cpp
//
#if defined(__linux__) || defined(__unix__)
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <pthread.h>
#endif

#include <fstream>
#include <iostream>
#include <cstdlib>

#include <QPixmap>
#include <QWindow>
#include <QScreen>
#include <QSettings>
#include <QHeaderView>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QDesktopServices>
#include <QStatusBar>
#include <QStyleFactory>
#include <QApplication>
#include <QActionGroup>
#include <QShortcut>
#include <QUrl>
#include <QDir>

#include "../../fceu.h"
#include "../../fds.h"
#include "../../file.h"
#include "../../input.h"
#include "../../movie.h"
#include "../../wave.h"
#include "../../state.h"
#include "../../cheat.h"
#include "../../profiler.h"
#include "../../version.h"
#include "common/os_utils.h"
#include "utils/timeStamp.h"

#ifdef _S9XLUA_H
#include "../../fceulua.h"
#endif

#include "Qt/main.h"
#include "Qt/dface.h"
#include "Qt/input.h"
#include "Qt/throttle.h"
#include "Qt/ColorMenu.h"
#include "Qt/ConsoleWindow.h"
#include "Qt/InputConf.h"
#include "Qt/GamePadConf.h"
#include "Qt/FamilyKeyboard.h"
#include "Qt/HotKeyConf.h"
#include "Qt/PaletteConf.h"
#include "Qt/PaletteEditor.h"
#include "Qt/HelpPages.h"
#include "Qt/GuiConf.h"
#include "Qt/AviRecord.h"
#include "Qt/AviRiffViewer.h"
#include "Qt/MoviePlay.h"
#include "Qt/MovieRecord.h"
#include "Qt/MovieOptions.h"
#include "Qt/StateRecorderConf.h"
#include "Qt/TimingConf.h"
#include "Qt/FrameTimingStats.h"
#include "Qt/LuaControl.h"
#include "Qt/QtScriptManager.h"
#include "Qt/CheatsConf.h"
#include "Qt/GameGenie.h"
#include "Qt/HexEditor.h"
#include "Qt/TraceLogger.h"
#include "Qt/CodeDataLogger.h"
#include "Qt/ConsoleDebugger.h"
#include "Qt/ConsoleUtilities.h"
#include "Qt/ConsoleSoundConf.h"
#include "Qt/ConsoleVideoConf.h"
#include "Qt/MsgLogViewer.h"
#include "Qt/AboutWindow.h"
#include "Qt/fceuWrapper.h"
#include "Qt/ppuViewer.h"
#include "Qt/NameTableViewer.h"
#include "Qt/iNesHeaderEditor.h"
#include "Qt/RamWatch.h"
#include "Qt/RamSearch.h"
#include "Qt/keyscan.h"
#include "Qt/nes_shm.h"
#include "Qt/NetPlay.h"
#include "Qt/TasEditor/TasEditorWindow.h"

#ifdef __FCEU_REST_API_ENABLE__
#include "Qt/RestApi/FceuxApiServer.h"
#endif

#ifdef __APPLE__
void qt_set_sequence_auto_mnemonic(bool enable);
#endif

consoleWin_t::consoleWin_t(QWidget *parent)
	: QMainWindow( parent )
{
	int opt, xWinPos = -1, yWinPos = -1, xWinSize = 256, yWinSize = 240;
	int videoDriver = 0;
	int setFullScreen = false;

	//QString libpath = QLibraryInfo::location(QLibraryInfo::PluginsPath);
	//printf("LibPath: '%s'\n", libpath.toLocal8Bit().constData() );

	tempDir = new QTemporaryDir();
	if (tempDir->isValid())
	{
		printf("Temp Folder: %s\n", tempDir->path().toLocal8Bit().constData());
	}

#ifdef __APPLE__
	qt_set_sequence_auto_mnemonic(true);
#endif

	printf("Running on Platform: %s\n", QGuiApplication::platformName().toLocal8Bit().constData() );

	QThread *thread = QThread::currentThread();

	if (thread)
	{
		thread->setObjectName( QString("MainThread") );
	}

	QApplication::setStyle( new fceuStyle() );

	initHotKeys();

	firstResize    = true;
	closeRequested = false;
	errorMsgValid  = false;
	viewport_GL      = NULL;
	viewport_SDL     = NULL;
	viewport_QWidget = NULL;
	viewport_Interface = NULL;

	contextMenuEnable      = false;
	soundUseGlobalFocus    = false;
	mainMenuEmuPauseSet    = false;
	mainMenuEmuWasPaused   = false;
	mainMenuPauseWhenActv  = false;
	autoHideMenuFullscreen = false;
	redrawVideoRequest     = false;

	createMainMenu();

	g_config->getOption( "SDL.PauseOnMainMenuAccess", &mainMenuPauseWhenActv );
	g_config->getOption( "SDL.AutoHideMenuFullsreen", &autoHideMenuFullscreen );
	g_config->getOption( "SDL.ContextMenuEnable", &contextMenuEnable );
	g_config->getOption( "SDL.Sound.UseGlobalFocus", &soundUseGlobalFocus );
	g_config->getOption ("SDL.VideoDriver", &videoDriver);

	loadVideoDriver( videoDriver );

	setWindowTitle( tr(FCEU_NAME_AND_VERSION) );
	setWindowIcon(QIcon(":fceux1.png"));
	setAcceptDrops(true);

	gameTimer  = new QTimer( this );
	emulatorThread = new emulatorThread_t(this);

	connect(emulatorThread, &QThread::finished, emulatorThread, &QObject::deleteLater);
	connect(emulatorThread, SIGNAL(frameFinished(void)), this, SLOT(emuFrameFinish(void)) );
	connect(emulatorThread, SIGNAL(loadRomRequest(QString)), this, SLOT(loadRomRequestCB(QString)) );

	connect( gameTimer, &QTimer::timeout, this, &consoleWin_t::updatePeriodic );

	gameTimer->setTimerType( Qt::PreciseTimer );
	gameTimer->start( 8 ); // 120hz

#ifdef __FCEU_QSCRIPT_ENABLE__
	QtScriptManager::create(nullptr);
#endif

	emulatorThread->start();

	g_config->getOption( "SDL.SetSchedParam", &opt );

	if ( opt )
	{
		#ifndef WIN32
		int policy, prio, nice;

		g_config->getOption( "SDL.GuiSchedPolicy", &policy );
		g_config->getOption( "SDL.GuiSchedPrioRt", &prio   );
		g_config->getOption( "SDL.GuiSchedNice"  , &nice   );

		setNicePriority( nice );

		setSchedParam( policy, prio );
		#endif
	}


	SDL_DisplayMode mode;
	int sdl_err = SDL_GetCurrentDisplayMode(0,&mode);
	g_config->getOption( "SDL.Fullscreen", &setFullScreen );
	if( (sdl_err == 0) && setFullScreen )
	{
	        xWinPos = 0;
	        yWinPos = 0;
	        xWinSize = mode.w;
	        yWinSize = mode.h;
	}
	else
	{
	        g_config->getOption( "SDL.WinPosX" , &xWinPos );
	        g_config->getOption( "SDL.WinPosY" , &yWinPos );
	        g_config->getOption( "SDL.WinSizeX", &xWinSize );
	        g_config->getOption( "SDL.WinSizeY", &yWinSize );
	}

	if ( (xWinSize >= 256) && (yWinSize >= 224) )
	{
		this->resize( xWinSize, yWinSize );

		if ( (xWinPos >= 0) && (yWinPos >= 0) )
		{
			this->move( xWinPos, yWinPos );
		}
	}
	else
	{
		QSize reqSize = calcRequiredSize();

		// Since the height of menu is unknown until Qt has shows the window
		// Set the minimum viewport sizes to exactly what we need so that 
		// the window is resized appropriately. On the first resize event,
		// we will set the minimum viewport size back to 1x values that the
		// window can be shrunk by dragging lower corner.
		if ( viewport_Interface != NULL )
		{
			viewport_Interface->setMinimumSize( reqSize );
		}
		//this->resize( reqSize );
	}

	g_config->getOption( "SDL.Fullscreen", &setFullScreen );
	g_config->setOption( "SDL.Fullscreen", 0 ); // Reset full screen config parameter to false so it is never saved this way

	if ( setFullScreen )
	{
		if ( autoHideMenuFullscreen )
		{
			menubar->setVisible(false);
		}
		this->showFullScreen();
	}

	refreshRate = 0.0;
	updateCounter = 0;
	recentRomMenuReset = false;
	helpWin = 0;

	// Viewport Cursor Type and Visibility
	loadCursor();

	// Create AVI Recording Disk Thread
	aviDiskThread = new AviRecordDiskThread_t(this);

	scrHandlerConnected = false;

	// Register State Load Callback with Emulation Core
	auto stateLoadCallback = []( bool loadSuccess )
	{
		//printf("State Loaded: %i \n", loadSuccess );
		if (loadSuccess && (consoleWindow != nullptr) )
		{
			emit consoleWindow->stateLoaded();
		}
	};
	FCEUSS_SetLoadCallback( stateLoadCallback );

	// Register Cheat Change Callback
	auto cheatChangeCallback = []( void* userData )
	{
		FCEU_UNUSED(userData);

		//printf("Cheats Changed Event!\n");
		if (consoleWindow != nullptr)
		{
			emit consoleWindow->cheatsChanged();
		}
	};
	FCEU_SetCheatChangeEventCallback( cheatChangeCallback, this );

#ifdef __FCEU_REST_API_ENABLE__
	// Initialize REST API server
	apiServer = new FceuxApiServer(this);
	
	// Connect signals
	connect(apiServer, &RestApiServer::errorOccurred, this, &consoleWin_t::onRestApiServerError);
	connect(apiServer, &RestApiServer::serverStarted, this, &consoleWin_t::onRestApiServerStarted);
	connect(apiServer, &RestApiServer::serverStopped, this, &consoleWin_t::onRestApiServerStopped);
	
	// Start server if enabled in configuration
	bool restApiEnabled = false;
	g_config->getOption("SDL.RestApiEnabled", &restApiEnabled);
	
	if (restApiEnabled) {
		// Load configuration and start server
		RestApiConfig apiConfig = loadRestApiConfig();
		apiServer->setConfig(apiConfig);
		apiServer->start();
	}
#endif
}

consoleWin_t::~consoleWin_t(void)
{
	QSize w;
	QClipboard *clipboard;

	// Save window size and image scaling parameters at app exit.
	w = size();

	// Only Save window size if not fullscreen and not maximized
	if ( !isFullScreen() && !isMaximized() )
	{
		// Scaling is only saved when applying video settings
		g_config->setOption( "SDL.WinPosX" , pos().x() );
		g_config->setOption( "SDL.WinPosY" , pos().y() );
		g_config->setOption( "SDL.WinSizeX", w.width() );
		g_config->setOption( "SDL.WinSizeY", w.height() );
	}
	else
	{
		QRect rect = normalGeometry();

		if ( rect.isValid() )
		{
			g_config->setOption( "SDL.WinPosX" , rect.x() );
			g_config->setOption( "SDL.WinPosY" , rect.y() );
			g_config->setOption( "SDL.WinSizeX", rect.width() );
			g_config->setOption( "SDL.WinSizeY", rect.height() );
		}
	}
	g_config->save();

	// Signal Emulator Thread to Stop
	nes_shm->runEmulator = 0;

	gameTimer->stop(); 

	closeGamePadConfWindow();

#ifdef __FCEU_QSCRIPT_ENABLE__
	QtScriptManager::destroy();
#endif

	NetPlayCloseSession();

#ifdef __FCEU_REST_API_ENABLE__
	// Stop REST API server
	if (apiServer) {
		apiServer->stop();
		delete apiServer;
		apiServer = nullptr;
	}
#endif

	// The closeApp function call stops all threads.
	// Calling quit on threads should not happen here. 
	//printf("Thread Finished: %i \n", emulatorThread->isFinished() );
	//emulatorThread->quit();
	//emulatorThread->wait( 1000 );

	//aviDiskThread->requestInterruption();
	//aviDiskThread->quit();
	//aviDiskThread->wait( 10000 );

	//FCEU_WRAPPER_LOCK();
	//fceuWrapperClose();
	//FCEU_WRAPPER_UNLOCK();

	unloadVideoDriver();

	// LoadGame() checks for an IP and if it finds one begins a network session
	// clear the NetworkIP field so this doesn't happen unintentionally
	//g_config->setOption ("SDL.NetworkIP", "");
	//g_config->save ();

	// Clear Clipboard Contents on Program Exit
	clipboard = QGuiApplication::clipboard();

	if ( clipboard->ownsClipboard() )
	{
		clipboard->clear( QClipboard::Clipboard );
	}
	if ( clipboard->ownsSelection() )
	{
		clipboard->clear( QClipboard::Selection );
	}

	clearRomList();

	if ( this == consoleWindow )
	{
		consoleWindow = NULL;
	}

	if (tempDir != nullptr)
	{
		delete tempDir;
		tempDir = nullptr;
	}
}

int consoleWin_t::videoInit(void)
{
	int ret = 0;

	if (viewport_Interface)
	{
		ret = viewport_Interface->init();
	}
	return ret;
}

void consoleWin_t::videoReset(void)
{
	if (viewport_Interface)
	{
		viewport_Interface->reset();
	}
	return;
}

void consoleWin_t::initScreenHandler(void)
{
	if ( !scrHandlerConnected )
	{
		QWidget *w;
		
		w = this->window();

		// This needs to be scheduled after window creation.
		if ( w != NULL)
		{
			QWindow *hdl = w->windowHandle();

			if (hdl != NULL)
			{
				//printf("Connecting to screenChanged Signal\n");
				connect( hdl, SIGNAL(screenChanged(QScreen*)), this, SLOT(winScreenChanged(QScreen*)) );
				scrHandlerConnected = true;

				winScreenChanged( hdl->screen() );

				connect( hdl, SIGNAL(activeChanged(void)), this, SLOT(winActiveChanged(void)) );
			}
		}
	}

}

void consoleWin_t::winScreenChanged(QScreen *scr)
{
	if ( scr == NULL )
	{
		return;
	}
	refreshRate = scr->refreshRate();
	printf("Screen Refresh Rate: %f\n", scr->refreshRate() );

	//printf("Screen Changed: %p\n", scr );
	if ( viewport_GL != NULL )
	{
		viewport_GL->screenChanged( scr );
	}
}

void consoleWin_t::winActiveChanged(void)
{
	QWidget *w;
	bool muteWindow = false;

	w = this->window();

	//printf("Active Changed\n");

	if ( w != NULL)
	{
		QWindow *hdl = w->windowHandle();

		if (hdl != NULL)
		{
			if ( !soundUseGlobalFocus )
			{
				if ( hdl->isActive() )
				{
					muteWindow = false;
				}
				else
				{
					muteWindow = true;
				}
			}
		}
	}
	FCEUD_MuteSoundWindow(muteWindow);
}

QSize consoleWin_t::calcRequiredSize(void)
{
	QSize out( GL_NES_WIDTH, GL_NES_HEIGHT );

	QSize w, v;
	double xscale = 1.0, yscale = 1.0, aspectRatio = 1.0;
	int texture_width = GL_NES_WIDTH;
	int texture_height = GL_NES_HEIGHT;
	int l=0, r=texture_width;
	int t=0, b=texture_height;
	int dw=0, dh=0, rw, rh;
	bool forceAspect = true;

	CalcVideoDimensions();

	texture_width  = nes_shm->video.ncol;
	texture_height = nes_shm->video.nrow;

	l=0, r=texture_width;
	t=0, b=texture_height;

	w = size();

	if ( viewport_Interface )
	{
		v = viewport_Interface->size();
		forceAspect = viewport_Interface->getForceAspectOpt();
		aspectRatio = viewport_Interface->getAspectRatio();
		xscale = viewport_Interface->getScaleX();
		yscale = viewport_Interface->getScaleY();
	}

	dw = 0;
	dh = 0;

	if ( forceAspect )
	{
		yscale = xscale * (double)nes_shm->video.xyRatio;
	}
	rw=(int)((r-l)*xscale);
	rh=(int)((b-t)*yscale);

	//printf("view %i x %i \n", rw, rh );

	if ( forceAspect )
	{
		double rr;

		rr = (double)rh / (double)rw;

		if ( rr > aspectRatio )
		{
			rw = (int)( (((double)rh) / aspectRatio) + 0.50);
		}
		else
		{
			rh = (int)( (((double)rw) * aspectRatio) + 0.50);
		}
	}

	out.setWidth( rw + dw );
	out.setHeight( rh + dh );

	//printf("Win %i x %i \n", rw + dw, rh + dh );

	return out;
}

void consoleWin_t::setViewportAspect(void)
{
	int aspectSel;
	double x,y;

	g_config->getOption ("SDL.AspectSelect", &aspectSel);

	switch ( aspectSel )
	{
		default:
		case 0:
			x =  1.0; y = 1.0;
		break;
		case 1:
			x =  8.0; y = 7.0;
		break;
		case 2:
			x = 11.0; y = 8.0;
		break;
		case 3:
			x =  4.0; y = 3.0;
		break;
		case 4:
			x = 16.0; y = 9.0;
		break;
		case 5:
		{
			x = 1.0; y = 1.0;
		}
		break;
	}

	if (viewport_Interface)
	{
		viewport_Interface->setAspectXY( x, y );
	}
}

void consoleWin_t::setMenuAccessPauseEnable( bool enable )
{
	mainMenuPauseWhenActv = enable;
}

void consoleWin_t::setContextMenuEnable( bool enable )
{
	contextMenuEnable = enable;
}

void consoleWin_t::setSoundUseGlobalFocus( bool enable )
{
	soundUseGlobalFocus = enable;

	winActiveChanged();
}

void consoleWin_t::loadCursor(void)
{
	int cursorVis;

	// Viewport Cursor Type and Visibility
	g_config->getOption("SDL.CursorVis", &cursorVis );

	if ( cursorVis )
	{
		int cursorType;

		g_config->getOption("SDL.CursorType", &cursorType );

		switch ( cursorType )
		{
			case 4:
			{
				QPixmap reticle(":/icons/reticle.png");

				setViewerCursor( QCursor(reticle.scaled(64,64)) );
			}
			break;
			case 3:
			{
				QPixmap reticle(":/icons/reticle.png");

				setViewerCursor( QCursor(reticle.scaled(32,32)) );
			}
			break;
			case 2:
				setViewerCursor( Qt::BlankCursor );
			break;
			case 1:
				setViewerCursor( Qt::CrossCursor );
			break;
			default:
			case 0:
				setViewerCursor( Qt::ArrowCursor );
			break;
		}
	}
	else
	{
		setViewerCursor( Qt::BlankCursor );
	}
}

void consoleWin_t::setViewerCursor( QCursor s )
{
	if (viewport_Interface)
	{
		viewport_Interface->setCursor(s);
	}
}

void consoleWin_t::setViewerCursor( Qt::CursorShape s )
{
	if (viewport_Interface)
	{
		viewport_Interface->setCursor(s);
	}
}

Qt::CursorShape consoleWin_t::getViewerCursor(void)
{
	Qt::CursorShape s = Qt::ArrowCursor;

	if (viewport_Interface)
	{
		s = viewport_Interface->cursor().shape();
	}
	return s;
}

void consoleWin_t::resizeEvent(QResizeEvent *event)
{
	if ( firstResize )
	{
		// We are assuming that window has been exposed and all sizing of menu is finished
		// Restore minimum sizes to 1x values after first resize event so that
		// window is still able to be shrunk by dragging lower corners.
		if (viewport_Interface)
		{
			viewport_Interface->setMinimumSize( QSize( 256, 224 ) );
		}

		firstResize = false;
	}
	//printf("%i x %i \n", event->size().width(), event->size().height() );
}

void consoleWin_t::setCyclePeriodms( int ms )
{
	// If timer is already running, it will be restarted.
	gameTimer->start( ms );
   
	//printf("Period Set to: %i ms \n", ms );
}

void consoleWin_t::showErrorMsgWindow()
{
	QMessageBox msgBox(this);

	FCEU_WRAPPER_LOCK();
	msgBox.resize( this->size() );
	msgBox.setIcon( QMessageBox::Critical );
	msgBox.setText( tr(errorMsg.c_str()) );
	errorMsg.clear();
	FCEU_WRAPPER_UNLOCK();
	//msgBox.show();
	msgBox.exec();
}

void consoleWin_t::QueueErrorMsgWindow( const char *msg )
{
	errorMsg.append( msg );
	errorMsg.append("\n");
	errorMsgValid = true;
}

void consoleWin_t::closeEvent(QCloseEvent *event)
{
	//printf("Main Window Close Event\n");
	closeGamePadConfWindow();

	event->accept();

	closeApp();
}

void consoleWin_t::requestClose(void)
{
	closeRequested = true;
}

void consoleWin_t::keyPressEvent(QKeyEvent *event)
{
	//printf("Key Press: 0x%x \n", event->key() );
	pushKeyEvent( event, 1 );

	event->accept();
}

void consoleWin_t::keyReleaseEvent(QKeyEvent *event)
{
	//printf("Key Release: 0x%x \n", event->key() );
	pushKeyEvent( event, 0 );

	event->accept();
}

void consoleWin_t::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasUrls() )
	{
		event->acceptProposedAction();
	}
}

void consoleWin_t::dropEvent(QDropEvent *event)
{
	if (event->mimeData()->hasUrls() )
	{
		QList<QUrl> urls = event->mimeData()->urls();

		QString filename = urls[0].toString( QUrl::PreferLocalFile );

		QFileInfo fi( filename );
		QString suffix = fi.suffix();

		bool isStateSaveFile = (suffix.size() == 3) && 
						(suffix[0] == 'f') && (suffix[1] == 'c') &&
							( (suffix[2] == 's') || suffix[2].isDigit() );

		//printf("DragNDrop Suffix: %s\n", suffix.toLocal8Bit().constData() );

		if (isStateSaveFile)
		{
			FCEU_WRAPPER_LOCK();
			FCEUI_LoadState( filename.toLocal8Bit().constData() );
			FCEU_WRAPPER_UNLOCK();

			event->accept();
		}
		else if ( suffix.compare("lua", Qt::CaseInsensitive) == 0 )
		{
			int luaLoadSuccess;

			FCEU_WRAPPER_LOCK();
			luaLoadSuccess = FCEU_LoadLuaCode( filename.toLocal8Bit().constData() );
			FCEU_WRAPPER_UNLOCK();

			if (luaLoadSuccess)
			{
				g_config->setOption("SDL.LastLoadLua", filename.toLocal8Bit().constData());
			}
			event->accept();
		}
		else
		{
			int romLoadSuccess;

			FCEU_WRAPPER_LOCK();
			romLoadSuccess = LoadGame( filename.toLocal8Bit().constData() );
			FCEU_WRAPPER_UNLOCK();

			if (!romLoadSuccess)
			{
				printf("DragNDrop ROM Load Failed for %s\n", filename.toLocal8Bit().constData() );
			}
			event->accept();
		}
	}
}

void consoleWin_t::showEvent(QShowEvent *event)
{
	//printf("Main Window Show Event\n");
	initScreenHandler();
}

void consoleWin_t::contextMenuEvent(QContextMenuEvent *event)
{
	QAction *act;
	QMenu menu(this);

	if ( !contextMenuEnable )
	{
		return;
	}

	act = new QAction(tr("Open ROM"), &menu);
	connect( act, SIGNAL(triggered(void)), this, SLOT(openROMFile(void)) );

	menu.addAction( act );

	act = new QAction(tr("Last ROM Used"), &menu);
	act->setEnabled( romList.size() > 0 );
	connect( act, SIGNAL(triggered(void)), this, SLOT(loadMostRecentROM(void)) );

	menu.addAction( act );

	menu.addSeparator();

	act = new QAction(tr("Online Help"), &menu);
	connect( act, SIGNAL(triggered(void)), this, SLOT(openOnlineDocs(void)) );

	menu.addAction( act );

	menu.addSeparator();

	act = new QAction(tr("Disable Context Menu via Options -> GUI Config"), &menu);
	connect( act, SIGNAL(triggered(void)), this, SLOT(openGuiConfWin(void)) );

	menu.addAction( act );

	menu.addSeparator();

	menu.exec(event->globalPos());

	event->accept();
}
//---------------------------------------------------------------------------
void consoleWin_t::initHotKeys(void)
{
	for (int i = 0; i < HK_MAX; i++)
	{
		Hotkeys[i].init( this );
	}

	for (int i = 0; i < HK_MAX; i++)
	{
		QShortcut *shortcut = Hotkeys[i].getShortcut();

		// Use Lambda Function to set callback
		connect( shortcut, &QShortcut::activatedAmbiguously, [ this, shortcut ] { warnAmbiguousShortcut( shortcut ); } );
	}

	// Frame Advance uses key state directly, disable shortcut events
	Hotkeys[HK_FRAME_ADVANCE].getShortcut()->setEnabled(false);
	Hotkeys[HK_TURBO        ].getShortcut()->setEnabled(false);

	connect( Hotkeys[ HK_SPEED_QUARTER ].getShortcut(), &QShortcut::activated, [] { CustomEmulationSpeed(  25); } );
	connect( Hotkeys[ HK_SPEED_HALF    ].getShortcut(), &QShortcut::activated, [] { CustomEmulationSpeed(  50); } );
	connect( Hotkeys[ HK_SPEED_NORMAL  ].getShortcut(), &QShortcut::activated, [] { CustomEmulationSpeed( 100); } );
	connect( Hotkeys[ HK_SPEED_2X      ].getShortcut(), &QShortcut::activated, [] { CustomEmulationSpeed( 200); } );
	connect( Hotkeys[ HK_SPEED_4X      ].getShortcut(), &QShortcut::activated, [] { CustomEmulationSpeed( 400); } );
	connect( Hotkeys[ HK_SPEED_8X      ].getShortcut(), &QShortcut::activated, [] { CustomEmulationSpeed( 800); } );
	connect( Hotkeys[ HK_SPEED_16X     ].getShortcut(), &QShortcut::activated, [] { CustomEmulationSpeed(1600); } );

	connect( Hotkeys[ HK_VOLUME_MUTE ].getShortcut(), SIGNAL(activated()), this, SLOT(muteSoundVolume(void)) );
	connect( Hotkeys[ HK_VOLUME_DOWN ].getShortcut(), SIGNAL(activated()), this, SLOT(decrSoundVolume(void)) );
	connect( Hotkeys[ HK_VOLUME_UP   ].getShortcut(), SIGNAL(activated()), this, SLOT(incrSoundVolume(void)) );

	connect( Hotkeys[ HK_LAG_COUNTER_DISPLAY  ].getShortcut(), SIGNAL(activated()), this, SLOT(toggleLagCounterDisplay(void)) );
	connect( Hotkeys[ HK_FA_LAG_SKIP          ].getShortcut(), SIGNAL(activated()), this, SLOT(toggleFrameAdvLagSkip(void))   );
	connect( Hotkeys[ HK_BIND_STATE           ].getShortcut(), SIGNAL(activated()), this, SLOT(toggleMovieBindSaveState(void)));
	connect( Hotkeys[ HK_TOGGLE_FRAME_DISPLAY ].getShortcut(), SIGNAL(activated()), this, SLOT(toggleMovieFrameDisplay(void)) );
	connect( Hotkeys[ HK_MOVIE_TOGGLE_RW      ].getShortcut(), SIGNAL(activated()), this, SLOT(toggleMovieReadWrite(void))    );
	connect( Hotkeys[ HK_TOGGLE_INPUT_DISPLAY ].getShortcut(), SIGNAL(activated()), this, SLOT(toggleInputDisplay(void))      );
	connect( Hotkeys[ HK_TOGGLE_BG            ].getShortcut(), SIGNAL(activated()), this, SLOT(toggleBackground(void))        );
	connect( Hotkeys[ HK_TOGGLE_FG            ].getShortcut(), SIGNAL(activated()), this, SLOT(toggleForeground(void))        );
	connect( Hotkeys[ HK_FKB_ENABLE           ].getShortcut(), SIGNAL(activated()), this, SLOT(toggleFamKeyBrdEnable(void))   );
	connect( Hotkeys[ HK_TOGGLE_ALL_CHEATS    ].getShortcut(), SIGNAL(activated()), this, SLOT(toggleGlobalCheatEnable(void)) );

	connect( Hotkeys[ HK_SAVE_STATE_0         ].getShortcut(), SIGNAL(activated()), this, SLOT(saveState0(void))        );
	connect( Hotkeys[ HK_SAVE_STATE_1         ].getShortcut(), SIGNAL(activated()), this, SLOT(saveState1(void))        );
	connect( Hotkeys[ HK_SAVE_STATE_2         ].getShortcut(), SIGNAL(activated()), this, SLOT(saveState2(void))        );
	connect( Hotkeys[ HK_SAVE_STATE_3         ].getShortcut(), SIGNAL(activated()), this, SLOT(saveState3(void))        );
	connect( Hotkeys[ HK_SAVE_STATE_4         ].getShortcut(), SIGNAL(activated()), this, SLOT(saveState4(void))        );
	connect( Hotkeys[ HK_SAVE_STATE_5         ].getShortcut(), SIGNAL(activated()), this, SLOT(saveState5(void))        );
	connect( Hotkeys[ HK_SAVE_STATE_6         ].getShortcut(), SIGNAL(activated()), this, SLOT(saveState6(void))        );
	connect( Hotkeys[ HK_SAVE_STATE_7         ].getShortcut(), SIGNAL(activated()), this, SLOT(saveState7(void))        );
	connect( Hotkeys[ HK_SAVE_STATE_8         ].getShortcut(), SIGNAL(activated()), this, SLOT(saveState8(void))        );
	connect( Hotkeys[ HK_SAVE_STATE_9         ].getShortcut(), SIGNAL(activated()), this, SLOT(saveState9(void))        );

	connect( Hotkeys[ HK_LOAD_STATE_0         ].getShortcut(), SIGNAL(activated()), this, SLOT(loadState0(void))        );
	connect( Hotkeys[ HK_LOAD_STATE_1         ].getShortcut(), SIGNAL(activated()), this, SLOT(loadState1(void))        );
	connect( Hotkeys[ HK_LOAD_STATE_2         ].getShortcut(), SIGNAL(activated()), this, SLOT(loadState2(void))        );
	connect( Hotkeys[ HK_LOAD_STATE_3         ].getShortcut(), SIGNAL(activated()), this, SLOT(loadState3(void))        );
	connect( Hotkeys[ HK_LOAD_STATE_4         ].getShortcut(), SIGNAL(activated()), this, SLOT(loadState4(void))        );
	connect( Hotkeys[ HK_LOAD_STATE_5         ].getShortcut(), SIGNAL(activated()), this, SLOT(loadState5(void))        );
	connect( Hotkeys[ HK_LOAD_STATE_6         ].getShortcut(), SIGNAL(activated()), this, SLOT(loadState6(void))        );
	connect( Hotkeys[ HK_LOAD_STATE_7         ].getShortcut(), SIGNAL(activated()), this, SLOT(loadState7(void))        );
	connect( Hotkeys[ HK_LOAD_STATE_8         ].getShortcut(), SIGNAL(activated()), this, SLOT(loadState8(void))        );
	connect( Hotkeys[ HK_LOAD_STATE_9         ].getShortcut(), SIGNAL(activated()), this, SLOT(loadState9(void))        );

	connect( Hotkeys[ HK_LOAD_PREV_STATE      ].getShortcut(), SIGNAL(activated()), this, SLOT(loadPrevState(void))     );
	connect( Hotkeys[ HK_LOAD_NEXT_STATE      ].getShortcut(), SIGNAL(activated()), this, SLOT(loadNextState(void))     );
}
//---------------------------------------------------------------------------
void consoleWin_t::createMainMenu(void)
{
	QAction *act;
	QMenu *subMenu;
	QActionGroup *group;
	int useNativeMenuBar;
	int customAutofireOnFrames, customAutofireOffFrames;
	//QShortcut *shortcut;

	menubar = new consoleMenuBar(this);

	this->setMenuBar(menubar);

	// This is needed for menu bar to show up on MacOS
	g_config->getOption( "SDL.UseNativeMenuBar", &useNativeMenuBar );

	menubar->setNativeMenuBar( useNativeMenuBar ? true : false );

	// Top Level Menu Iterms
	fileMenu    = menubar->addMenu(tr("&File"));
	movieMenu   = menubar->addMenu(tr("&Movie"));
	optMenu     = menubar->addMenu(tr("&Options"));
	emuMenu     = menubar->addMenu(tr("&Emulation"));
	toolsMenu   = menubar->addMenu(tr("&Tools"));
	debugMenu   = menubar->addMenu(tr("&Debug"));
	netPlayMenu = menubar->addMenu(tr("&NetPlay"));
	helpMenu    = menubar->addMenu(tr("&Help"));

	//-----------------------------------------------------------------------
	// File
	
	connect( fileMenu, SIGNAL(aboutToShow(void)), this, SLOT(mainMenuOpen(void)) );
	connect( fileMenu, SIGNAL(aboutToHide(void)), this, SLOT(mainMenuClose(void)) );

	// File -> Open ROM
	openROM = new QAction(tr("&Open ROM"), this);
	//openROM->setShortcuts(QKeySequence::Open);
	openROM->setStatusTip(tr("Open ROM File"));
	//openROM->setIcon( QIcon(":icons/rom.png") );
	//openROM->setIcon( style->standardIcon( QStyle::SP_FileIcon ) );
	openROM->setIcon( style()->standardIcon( QStyle::SP_FileDialogStart ) );
	connect(openROM, SIGNAL(triggered()), this, SLOT(openROMFile(void)) );

	Hotkeys[ HK_OPEN_ROM ].setAction( openROM );
	connect( Hotkeys[ HK_OPEN_ROM ].getShortcut(), SIGNAL(activated()), this, SLOT(openROMFile(void)) );
	
	fileMenu->addAction(openROM);

	// File -> Close ROM
	closeROM = new QAction(tr("&Close ROM"), this);
	//closeROM->setShortcut( QKeySequence(tr("Ctrl+C")));
	closeROM->setStatusTip(tr("Close Loaded ROM"));
	closeROM->setIcon( style()->standardIcon( QStyle::SP_BrowserStop ) );
	connect(closeROM, SIGNAL(triggered()), this, SLOT(closeROMCB(void)) );
	
	Hotkeys[ HK_CLOSE_ROM ].setAction( closeROM );
	connect( Hotkeys[ HK_CLOSE_ROM ].getShortcut(), SIGNAL(activated()), this, SLOT(closeROMCB(void)) );
	
	fileMenu->addAction(closeROM);
	
	// File -> Recent ROMs
	recentRomMenu = fileMenu->addMenu( tr("&Recent ROMs") );

	buildRecentRomMenu();

	fileMenu->addSeparator();

	// File -> Play NSF
	playNSF = new QAction(tr("Play &NSF"), this);
	//playNSF->setShortcut( QKeySequence(tr("Ctrl+N")));
	playNSF->setStatusTip(tr("Play NSF"));
	connect(playNSF, SIGNAL(triggered()), this, SLOT(loadNSF(void)) );
	
	fileMenu->addAction(playNSF);
	
	fileMenu->addSeparator();

	// File -> Load State From
	loadStateAct = new QAction(tr("Load State &From"), this);
	//loadStateAct->setShortcut( QKeySequence(tr("Ctrl+N")));
	loadStateAct->setStatusTip(tr("Load State From"));
	loadStateAct->setIcon( style()->standardIcon( QStyle::SP_FileDialogStart ) );
	connect(loadStateAct, SIGNAL(triggered()), this, SLOT(loadStateFrom(void)) );
	
	fileMenu->addAction(loadStateAct);

	// File -> Save State As
	saveStateAct = new QAction(tr("Save State &As"), this);
	//loadStateAct->setShortcut( QKeySequence(tr("Ctrl+N")));
	saveStateAct->setStatusTip(tr("Save State As"));
	saveStateAct->setIcon( style()->standardIcon( QStyle::SP_DialogSaveButton ) );
	connect(saveStateAct, SIGNAL(triggered()), this, SLOT(saveStateAs(void)) );
	
	fileMenu->addAction(saveStateAct);

	// File -> Quick Load
	quickLoadAct = new QAction(tr("Quick &Load"), this);
	//quickLoadAct->setShortcut( QKeySequence(tr("Shift+I")));
	quickLoadAct->setStatusTip(tr("Quick Load"));
	connect(quickLoadAct, SIGNAL(triggered()), this, SLOT(quickLoad(void)) );
	
	fileMenu->addAction(quickLoadAct);

	Hotkeys[ HK_LOAD_STATE ].setAction( quickLoadAct );
	connect( Hotkeys[ HK_LOAD_STATE ].getShortcut(), SIGNAL(activated()), this, SLOT(quickLoad(void)) );
	
	// File -> Quick Save
	quickSaveAct = new QAction(tr("Quick &Save"), this);
	//quickSaveAct->setShortcut( QKeySequence(tr("F5")));
	quickSaveAct->setStatusTip(tr("Quick Save"));
	connect(quickSaveAct, SIGNAL(triggered()), this, SLOT(quickSave(void)) );
	
	fileMenu->addAction(quickSaveAct);

	Hotkeys[ HK_SAVE_STATE ].setAction( quickSaveAct );
	connect( Hotkeys[ HK_SAVE_STATE ].getShortcut(), SIGNAL(activated()), this, SLOT(quickSave(void)) );
	
	// File -> Change State Slot
	subMenu = fileMenu->addMenu(tr("Change &State Slot"));
	group   = new QActionGroup(this);

	group->setExclusive(true);

	for (int i=0; i<10; i++)
	{
	        char stmp[8];

	        snprintf( stmp, sizeof(stmp), "Slot &%i", i );

	        state[i] = new QAction(tr(stmp), this);
	        state[i]->setCheckable(true);

	        group->addAction(state[i]);
		subMenu->addAction(state[i]);
	}
	state[0]->setChecked(true);

	connect(state[0], SIGNAL(triggered()), this, SLOT(changeState0(void)) );
	connect(state[1], SIGNAL(triggered()), this, SLOT(changeState1(void)) );
	connect(state[2], SIGNAL(triggered()), this, SLOT(changeState2(void)) );
	connect(state[3], SIGNAL(triggered()), this, SLOT(changeState3(void)) );
	connect(state[4], SIGNAL(triggered()), this, SLOT(changeState4(void)) );
	connect(state[5], SIGNAL(triggered()), this, SLOT(changeState5(void)) );
	connect(state[6], SIGNAL(triggered()), this, SLOT(changeState6(void)) );
	connect(state[7], SIGNAL(triggered()), this, SLOT(changeState7(void)) );
	connect(state[8], SIGNAL(triggered()), this, SLOT(changeState8(void)) );
	connect(state[9], SIGNAL(triggered()), this, SLOT(changeState9(void)) );
	
	fileMenu->addSeparator();

	Hotkeys[ HK_SELECT_STATE_0 ].setAction( state[0] );
	Hotkeys[ HK_SELECT_STATE_1 ].setAction( state[1] );
	Hotkeys[ HK_SELECT_STATE_2 ].setAction( state[2] );
	Hotkeys[ HK_SELECT_STATE_3 ].setAction( state[3] );
	Hotkeys[ HK_SELECT_STATE_4 ].setAction( state[4] );
	Hotkeys[ HK_SELECT_STATE_5 ].setAction( state[5] );
	Hotkeys[ HK_SELECT_STATE_6 ].setAction( state[6] );
	Hotkeys[ HK_SELECT_STATE_7 ].setAction( state[7] );
	Hotkeys[ HK_SELECT_STATE_8 ].setAction( state[8] );
	Hotkeys[ HK_SELECT_STATE_9 ].setAction( state[9] );

	connect( Hotkeys[ HK_SELECT_STATE_0 ].getShortcut(), SIGNAL(activated()), this, SLOT(changeState0(void)) );
	connect( Hotkeys[ HK_SELECT_STATE_1 ].getShortcut(), SIGNAL(activated()), this, SLOT(changeState1(void)) );
	connect( Hotkeys[ HK_SELECT_STATE_2 ].getShortcut(), SIGNAL(activated()), this, SLOT(changeState2(void)) );
	connect( Hotkeys[ HK_SELECT_STATE_3 ].getShortcut(), SIGNAL(activated()), this, SLOT(changeState3(void)) );
	connect( Hotkeys[ HK_SELECT_STATE_4 ].getShortcut(), SIGNAL(activated()), this, SLOT(changeState4(void)) );
	connect( Hotkeys[ HK_SELECT_STATE_5 ].getShortcut(), SIGNAL(activated()), this, SLOT(changeState5(void)) );
	connect( Hotkeys[ HK_SELECT_STATE_6 ].getShortcut(), SIGNAL(activated()), this, SLOT(changeState6(void)) );
	connect( Hotkeys[ HK_SELECT_STATE_7 ].getShortcut(), SIGNAL(activated()), this, SLOT(changeState7(void)) );
	connect( Hotkeys[ HK_SELECT_STATE_8 ].getShortcut(), SIGNAL(activated()), this, SLOT(changeState8(void)) );
	connect( Hotkeys[ HK_SELECT_STATE_9 ].getShortcut(), SIGNAL(activated()), this, SLOT(changeState9(void)) );
	
	connect( Hotkeys[ HK_SELECT_STATE_PREV ].getShortcut(), SIGNAL(activated()), this, SLOT(decrementState(void)) );
	connect( Hotkeys[ HK_SELECT_STATE_NEXT ].getShortcut(), SIGNAL(activated()), this, SLOT(incrementState(void)) );

#ifdef _S9XLUA_H
	// File -> Load Lua
	loadLuaAct = new QAction(tr("Load &Lua Script"), this);
	//loadLuaAct->setShortcut( QKeySequence(tr("F5")));
	loadLuaAct->setStatusTip(tr("Load Lua Script"));
	//loadLuaAct->setIcon( QIcon(":icons/lua-logo.png") );
	connect(loadLuaAct, SIGNAL(triggered()), this, SLOT(loadLua(void)) );
	
	fileMenu->addAction(loadLuaAct);
	
	fileMenu->addSeparator();
#else
	loadLuaAct = nullptr;
#endif

#ifdef __FCEU_QSCRIPT_ENABLE__
	// File -> Load JavaScript
	loadJsAct = new QAction(tr("Load &JavaScript"), this);
	loadJsAct->setStatusTip(tr("Load JavaScript"));
	connect(loadJsAct, SIGNAL(triggered()), this, SLOT(loadJs(void)) );
	
	fileMenu->addAction(loadJsAct);
	
	fileMenu->addSeparator();
#else
	loadJsAct = NULL;
#endif

	// File -> Screenshot
	scrShotAct = new QAction(tr("Screens&hot"), this);
	//scrShotAct->setShortcut( QKeySequence(tr("F12")));
	scrShotAct->setStatusTip(tr("Screenshot"));
	scrShotAct->setIcon( QIcon(":icons/camera.png") );
	connect(scrShotAct, SIGNAL(triggered()), this, SLOT(prepareScreenShot(void)));
	
	fileMenu->addAction(scrShotAct);

	Hotkeys[ HK_SCREENSHOT ].setAction( scrShotAct );
	connect( Hotkeys[ HK_SCREENSHOT ].getShortcut(), SIGNAL(activated()), this, SLOT(takeScreenShot(void)) );

	// File -> Quit
	quitAct = new QAction(tr("&Quit"), this);
	//quitAct->setShortcut( QKeySequence(tr("Ctrl+Q")));
	quitAct->setStatusTip(tr("Quit the Application"));
	//quitAct->setIcon( style()->standardIcon( QStyle::SP_DialogCloseButton ) );
	quitAct->setIcon( QIcon(":icons/application-exit.png") );
	connect(quitAct, SIGNAL(triggered()), this, SLOT(closeApp()));
	
	fileMenu->addAction(quitAct);

	Hotkeys[ HK_QUIT ].setAction( quitAct );
	connect( Hotkeys[ HK_QUIT ].getShortcut(), SIGNAL(activated()), this, SLOT(closeApp(void)) );

	//-----------------------------------------------------------------------
	// Options

	connect( optMenu, SIGNAL(aboutToShow(void)), this, SLOT(mainMenuOpen(void)) );
	connect( optMenu, SIGNAL(aboutToHide(void)), this, SLOT(mainMenuClose(void)) );

	// Options -> Input Config
	inputConfig = new QAction(tr("&Input Config"), this);
	//inputConfig->setShortcut( QKeySequence(tr("Ctrl+C")));
	inputConfig->setStatusTip(tr("Input Configure"));
	inputConfig->setIcon( QIcon(":icons/input-gaming.png") );
	connect(inputConfig, SIGNAL(triggered()), this, SLOT(openInputConfWin(void)) );
	
	optMenu->addAction(inputConfig);

	// Options -> GamePad Config
	gamePadConfig = new QAction(tr("&GamePad Config"), this);
	//gamePadConfig->setShortcut( QKeySequence(tr("Ctrl+C")));
	gamePadConfig->setStatusTip(tr("GamePad Configure"));
	gamePadConfig->setIcon( QIcon(":icons/input-gaming-symbolic.png") );
	connect(gamePadConfig, SIGNAL(triggered()), this, SLOT(openGamePadConfWin(void)) );
	
	optMenu->addAction(gamePadConfig);

	// Options -> Sound Config
	gameSoundConfig = new QAction(tr("&Sound Config"), this);
	//gameSoundConfig->setShortcut( QKeySequence(tr("Ctrl+C")));
	gameSoundConfig->setStatusTip(tr("Sound Configure"));
	gameSoundConfig->setIcon( style()->standardIcon( QStyle::SP_MediaVolume ) );
	connect(gameSoundConfig, SIGNAL(triggered()), this, SLOT(openGameSndConfWin(void)) );
	
	optMenu->addAction(gameSoundConfig);

	// Options -> Video Config
	gameVideoConfig = new QAction(tr("&Video Config"), this);
	//gameVideoConfig->setShortcut( QKeySequence(tr("Ctrl+C")));
	gameVideoConfig->setStatusTip(tr("Video Preferences"));
	gameVideoConfig->setIcon( style()->standardIcon( QStyle::SP_ComputerIcon ) );
	connect(gameVideoConfig, SIGNAL(triggered()), this, SLOT(openGameVideoConfWin(void)) );
	
	optMenu->addAction(gameVideoConfig);

	// Options -> HotKey Config
	hotkeyConfig = new QAction(tr("Hot&Key Config"), this);
	//hotkeyConfig->setShortcut( QKeySequence(tr("Ctrl+C")));
	hotkeyConfig->setStatusTip(tr("Hotkey Configure"));
	hotkeyConfig->setIcon( QIcon(":icons/input-keyboard.png") );
	connect(hotkeyConfig, SIGNAL(triggered()), this, SLOT(openHotkeyConfWin(void)) );
	
	optMenu->addAction(hotkeyConfig);

	// Options -> Palette Config
	paletteConfig = new QAction(tr("&Palette Config"), this);
	//paletteConfig->setShortcut( QKeySequence(tr("Ctrl+C")));
	paletteConfig->setStatusTip(tr("Palette Configure"));
	paletteConfig->setIcon( QIcon(":icons/graphics-palette.png") );
	connect(paletteConfig, SIGNAL(triggered()), this, SLOT(openPaletteConfWin(void)) );
	
	optMenu->addAction(paletteConfig);

	// Options -> GUI Config
	guiConfig = new QAction(tr("G&UI Config"), this);
	//guiConfig->setShortcut( QKeySequence(tr("Ctrl+C")));
	guiConfig->setStatusTip(tr("GUI Configure"));
	guiConfig->setIcon( style()->standardIcon( QStyle::SP_TitleBarNormalButton ) );
	connect(guiConfig, SIGNAL(triggered()), this, SLOT(openGuiConfWin(void)) );
	
	optMenu->addAction(guiConfig);

	// Options -> Timing Config
	timingConfig = new QAction(tr("&Timing Config"), this);
	//timingConfig->setShortcut( QKeySequence(tr("Ctrl+C")));
	timingConfig->setStatusTip(tr("Timing Configure"));
	timingConfig->setIcon( QIcon(":icons/timer.png") );
	connect(timingConfig, SIGNAL(triggered()), this, SLOT(openTimingConfWin(void)) );
	
	optMenu->addAction(timingConfig);

	// Options -> State Recorder Config
	stateRecordConfig = new QAction(tr("&State Recorder Config"), this);
	//stateRecordConfig->setShortcut( QKeySequence(tr("Ctrl+C")));
	stateRecordConfig->setStatusTip(tr("State Recorder Configure"));
	stateRecordConfig->setIcon( QIcon(":icons/media-record.png") );
	connect(stateRecordConfig, SIGNAL(triggered()), this, SLOT(openStateRecorderConfWin(void)) );
	
	optMenu->addAction(stateRecordConfig);

	// Options -> Movie Options
	movieConfig = new QAction(tr("&Movie Options"), this);
	//movieConfig->setShortcut( QKeySequence(tr("Ctrl+C")));
	movieConfig->setStatusTip(tr("Movie Options"));
	movieConfig->setIcon( QIcon(":icons/movie.png") );
	connect(movieConfig, SIGNAL(triggered()), this, SLOT(openMovieOptWin(void)) );
	
	optMenu->addAction(movieConfig);

	// Options -> Auto-Resume
	autoResume = new QAction(tr("Auto-&Resume Play"), this);
	//autoResume->setShortcut( QKeySequence(tr("Ctrl+C")));
	autoResume->setCheckable(true);
	autoResume->setStatusTip(tr("Auto-Resume Play"));
	syncActionConfig( autoResume, "SDL.AutoResume" );
	connect(autoResume, SIGNAL(triggered()), this, SLOT(toggleAutoResume(void)) );

	optMenu->addAction(autoResume);
	
	optMenu->addSeparator();

	// Options -> Window Resize
	subMenu = optMenu->addMenu( tr("Window Resi&ze") );

	for (int i=0; i<4; i++)
	{
	        char stmp[8];

	        snprintf( stmp, sizeof(stmp), "&%ix", i+1 );

	        winSizeAct[i] = new QAction(tr(stmp), this);

		subMenu->addAction(winSizeAct[i]);

		connect( winSizeAct[i], &QAction::triggered, [ this, i ]{ consoleWin_t::winResizeIx(i+1); } );
	}

	// Options -> Full Screen
	fullscreen = new QAction(tr("&Fullscreen"), this);
	//fullscreen->setShortcut( QKeySequence(tr("Alt+Return")));
	fullscreen->setStatusTip(tr("Fullscreen"));
	fullscreen->setIcon( QIcon(":icons/view-fullscreen.png") );
	connect(fullscreen, SIGNAL(triggered()), this, SLOT(toggleFullscreen(void)) );
	
	optMenu->addAction(fullscreen);

	Hotkeys[ HK_FULLSCREEN ].setAction( fullscreen );
	connect( Hotkeys[ HK_FULLSCREEN ].getShortcut(), SIGNAL(activated()), this, SLOT(toggleFullscreen(void)) );

	// Options -> Hide Menu Screen
	act = new QAction(tr("&Hide Menu"), this);
	//act->setShortcut( QKeySequence(tr("Alt+/")));
	act->setStatusTip(tr("Hide Menu"));
	act->setIcon( style()->standardIcon( QStyle::SP_TitleBarMaxButton ) );
	connect(act, SIGNAL(triggered()), this, SLOT(toggleMenuVis(void)) );
	
	optMenu->addAction(act);

	Hotkeys[ HK_MAIN_MENU_HIDE ].setAction( act );
	connect( Hotkeys[ HK_MAIN_MENU_HIDE ].getShortcut(), SIGNAL(activated()), this, SLOT(toggleMenuVis(void)) );

	// Options -> Auto Hide Menu on Fullscreen
	g_config->getOption( "SDL.AutoHideMenuFullsreen", &autoHideMenuFullscreen );

	act = new QAction(tr("&Auto Hide Menu on Fullscreen"), this);
	//act->setShortcut( QKeySequence(tr("Alt+/")));
	act->setCheckable(true);
	act->setChecked( autoHideMenuFullscreen );
	act->setStatusTip(tr("Auto Hide Menu on Fullscreen"));
	//act->setIcon( style()->standardIcon( QStyle::SP_TitleBarMaxButton ) );
	connect(act, SIGNAL(triggered(bool)), this, SLOT(toggleMenuAutoHide(bool)) );

	optMenu->addAction(act);

	optMenu->addSeparator();

	// Options -> Video BG Color
	fceuLoadConfigColor( "SDL.VideoBgColor", &videoBgColor );

	bgColorMenuItem = new ColorMenuItem( tr("BG Side Panel Color"), "SDL.VideoBgColor", this );
	bgColorMenuItem->connectColor( &videoBgColor );

	optMenu->addAction(bgColorMenuItem);

	connect( bgColorMenuItem, SIGNAL(colorChanged(QColor&)), this, SLOT(videoBgColorChanged(QColor&)) );

	// Options -> Use BG Palette for Video BG Color
	g_config->getOption( "SDL.UseBgPaletteForVideo", &usePaletteForVideoBg );

	act = new QAction(tr("Use BG Palette for Video BG Color"), this);
	//act->setShortcut( QKeySequence(tr("Alt+/")));
	act->setCheckable(true);
	act->setChecked( usePaletteForVideoBg );
	act->setStatusTip(tr("Use BG Palette for Video BG Color"));
	//act->setIcon( style()->standardIcon( QStyle::SP_TitleBarMaxButton ) );
	connect(act, SIGNAL(triggered(bool)), this, SLOT(toggleUseBgPaletteForVideo(bool)) );

	optMenu->addAction(act);

	bgColorMenuItem->setEnabled( !usePaletteForVideoBg );
	//-----------------------------------------------------------------------
	// Emulation

	connect( emuMenu, SIGNAL(aboutToShow(void)), this, SLOT(mainMenuOpen(void)) );
	connect( emuMenu, SIGNAL(aboutToHide(void)), this, SLOT(mainMenuClose(void)) );

	// Emulation -> Power
	powerAct = new QAction(tr("&Power"), this);
	//powerAct->setShortcut( QKeySequence(tr("Ctrl+P")));
	powerAct->setStatusTip(tr("Power On Console"));
	powerAct->setIcon( QIcon(":icons/power.png") );
	connect(powerAct, SIGNAL(triggered()), this, SLOT(powerConsoleCB(void)) );
	
	emuMenu->addAction(powerAct);

	Hotkeys[ HK_POWER ].setAction( powerAct );
	connect( Hotkeys[ HK_POWER ].getShortcut(), SIGNAL(activated()), this, SLOT(powerConsoleCB(void)) );

	// Emulation -> Reset
	resetAct = new QAction(tr("Hard &Reset"), this);
	//resetAct->setShortcut( QKeySequence(tr("Ctrl+R")));
	resetAct->setStatusTip(tr("Hard Reset of Console"));
	resetAct->setIcon( style()->standardIcon( QStyle::SP_DialogResetButton ) );
	connect(resetAct, SIGNAL(triggered()), this, SLOT(consoleHardReset(void)) );
	
	emuMenu->addAction(resetAct);

	Hotkeys[ HK_HARD_RESET ].setAction( resetAct );
	connect( Hotkeys[ HK_HARD_RESET ].getShortcut(), SIGNAL(activated()), this, SLOT(consoleHardReset(void)) );

	// Emulation -> Soft Reset
	sresetAct = new QAction(tr("&Soft Reset"), this);
	//sresetAct->setShortcut( QKeySequence(tr("Ctrl+R")));
	sresetAct->setStatusTip(tr("Soft Reset of Console"));
	sresetAct->setIcon( style()->standardIcon( QStyle::SP_BrowserReload ) );
	connect(sresetAct, SIGNAL(triggered()), this, SLOT(consoleSoftReset(void)) );
	
	emuMenu->addAction(sresetAct);

	Hotkeys[ HK_SOFT_RESET ].setAction( sresetAct );
	connect( Hotkeys[ HK_SOFT_RESET ].getShortcut(), SIGNAL(activated()), this, SLOT(consoleSoftReset(void)) );

	// Emulation -> Pause
	pauseAct = new QAction(tr("&Pause"), this);
	//pauseAct->setShortcut( QKeySequence(tr("Pause")));
	pauseAct->setStatusTip(tr("Pause Console"));
	pauseAct->setIcon( style()->standardIcon( QStyle::SP_MediaPause ) );
	connect(pauseAct, SIGNAL(triggered()), this, SLOT(consolePause(void)) );
	
	emuMenu->addAction(pauseAct);
	
	Hotkeys[ HK_PAUSE ].setAction( pauseAct );
	connect( Hotkeys[ HK_PAUSE ].getShortcut(), SIGNAL(activated()), this, SLOT(consolePause(void)) );

	emuMenu->addSeparator();

	// Emulation -> Region
	subMenu = emuMenu->addMenu(tr("&Region"));
	group   = new QActionGroup(this);

	group->setExclusive(true);

	for (int i=0; i<3; i++)
	{
		const char *txt;

		if ( i == 1 )
		{
			txt = "&PAL";
		}
		else if ( i == 2 )
		{
			txt = "&Dendy";
		}
		else
		{
			txt = "&NTSC";
		}

	        region[i] = new QAction(tr(txt), this);
	        region[i]->setCheckable(true);

	        group->addAction(region[i]);
		subMenu->addAction(region[i]);
	}
	region[ FCEUI_GetRegion() ]->setChecked(true);

	connect( region[0], SIGNAL(triggered(void)), this, SLOT(setRegionNTSC(void)) );
	connect( region[1], SIGNAL(triggered(void)), this, SLOT(setRegionPAL(void)) );
	connect( region[2], SIGNAL(triggered(void)), this, SLOT(setRegionDendy(void)) );

	// Emulation -> RAM Init
	subMenu = emuMenu->addMenu(tr("&RAM Init"));
	group   = new QActionGroup(this);

	group->setExclusive(true);

	for (int i=0; i<4; i++)
	{
		const char *txt;

		switch (i)
		{
			default:
			case 0:
				txt = "&Default";
			break;
			case 1:
				txt = "Fill $&FF";
			break;
			case 2:
				txt = "Fill $&00";
			break;
			case 3:
				txt = "&Random";
			break;
		}

	        ramInit[i] = new QAction(tr(txt), this);
	        ramInit[i]->setCheckable(true);

	        group->addAction(ramInit[i]);
		subMenu->addAction(ramInit[i]);
	}

	g_config->getOption ("SDL.RamInitMethod", &RAMInitOption);

	ramInit[ RAMInitOption ]->setChecked(true);

	connect( ramInit[0], SIGNAL(triggered(void)), this, SLOT(setRamInit0(void)) );
	connect( ramInit[1], SIGNAL(triggered(void)), this, SLOT(setRamInit1(void)) );
	connect( ramInit[2], SIGNAL(triggered(void)), this, SLOT(setRamInit2(void)) );
	connect( ramInit[3], SIGNAL(triggered(void)), this, SLOT(setRamInit3(void)) );

	emuMenu->addSeparator();

	// Emulation -> Enable Game Genie
	gameGenieAct = new QAction(tr("Enable Game &Genie"), this);
	//gameGenieAct->setShortcut( QKeySequence(tr("Ctrl+G")));
	gameGenieAct->setCheckable(true);
	gameGenieAct->setStatusTip(tr("Enable Game Genie"));
	connect(gameGenieAct, SIGNAL(triggered(bool)), this, SLOT(toggleGameGenie(bool)) );

	syncActionConfig( gameGenieAct, "SDL.GameGenie" );

	emuMenu->addAction(gameGenieAct);

	// Emulation -> Load Game Genie ROM
	loadGgROMAct = new QAction(tr("Load Game Genie ROM"), this);
	//loadGgROMAct->setShortcut( QKeySequence(tr("Ctrl+G")));
	loadGgROMAct->setStatusTip(tr("Load Game Genie ROM"));
	connect(loadGgROMAct, SIGNAL(triggered()), this, SLOT(loadGameGenieROM(void)) );
	
	emuMenu->addAction(loadGgROMAct);
	
	emuMenu->addSeparator();

	// Emulation -> Virtual Family Keyboard
	act = new QAction(tr("Virtual Family Keyboard"), this);
	//act->setShortcut( QKeySequence(tr("Ctrl+G")));
	act->setStatusTip(tr("Virtual Family Keyboard"));
	connect(act, SIGNAL(triggered()), this, SLOT(openFamilyKeyboard(void)) );

	emuMenu->addAction(act);

	emuMenu->addSeparator();

	// Emulation -> Insert Coin
	insCoinAct = new QAction(tr("&Insert Coin"), this);
	//insCoinAct->setShortcut( QKeySequence(tr("Ctrl+G")));
	insCoinAct->setStatusTip(tr("Insert Coin"));
	connect(insCoinAct, SIGNAL(triggered()), this, SLOT(insertCoin(void)) );
	
	emuMenu->addAction(insCoinAct);
	
	Hotkeys[ HK_VS_INSERT_COIN ].setAction( insCoinAct );
	connect( Hotkeys[ HK_VS_INSERT_COIN ].getShortcut(), SIGNAL(activated()), this, SLOT(insertCoin(void)) );

	emuMenu->addSeparator();

	// Emulation -> FDS
	subMenu = emuMenu->addMenu(tr("&FDS"));

	// Emulation -> FDS -> Switch Disk
	fdsSwitchAct = new QAction(tr("&Switch Disk"), this);
	//fdsSwitchAct->setShortcut( QKeySequence(tr("Ctrl+G")));
	fdsSwitchAct->setStatusTip(tr("Switch Disk"));
	connect(fdsSwitchAct, SIGNAL(triggered()), this, SLOT(fdsSwitchDisk(void)) );
	
	Hotkeys[ HK_FDS_SELECT ].setAction( fdsSwitchAct );
	connect( Hotkeys[ HK_FDS_SELECT ].getShortcut(), SIGNAL(activated()), this, SLOT(fdsSwitchDisk(void)) );

	subMenu->addAction(fdsSwitchAct);

	// Emulation -> FDS -> Eject Disk
	fdsEjectAct = new QAction(tr("&Eject Disk"), this);
	//fdsEjectAct->setShortcut( QKeySequence(tr("Ctrl+G")));
	fdsEjectAct->setStatusTip(tr("Eject Disk"));
	connect(fdsEjectAct, SIGNAL(triggered()), this, SLOT(fdsEjectDisk(void)) );
	
	Hotkeys[ HK_FDS_EJECT ].setAction( fdsEjectAct );
	connect( Hotkeys[ HK_FDS_EJECT ].getShortcut(), SIGNAL(activated()), this, SLOT(fdsEjectDisk(void)) );

	subMenu->addAction(fdsEjectAct);

	// Emulation -> FDS -> Load BIOS
	fdsLoadBiosAct = new QAction(tr("&Load BIOS"), this);
	//fdsLoadBiosAct->setShortcut( QKeySequence(tr("Ctrl+G")));
	fdsLoadBiosAct->setStatusTip(tr("Load BIOS"));
	connect(fdsLoadBiosAct, SIGNAL(triggered()), this, SLOT(fdsLoadBiosFile(void)) );
	
	subMenu->addAction(fdsLoadBiosAct);
	
	emuMenu->addSeparator();

	// Emulation -> Speed
	subMenu = emuMenu->addMenu(tr("&Speed"));

	// Emulation -> Speed -> Speed Up
	act = new QAction(tr("Speed &Up"), this);
	//act->setShortcut( QKeySequence(tr("=")));
	act->setStatusTip(tr("Speed Up"));
	act->setIcon( style()->standardIcon( QStyle::SP_MediaSeekForward ) );
	connect(act, SIGNAL(triggered()), this, SLOT(emuSpeedUp(void)) );
	
	Hotkeys[ HK_INCREASE_SPEED ].setAction( act );
	connect( Hotkeys[ HK_INCREASE_SPEED ].getShortcut(), SIGNAL(activated()), this, SLOT(emuSpeedUp(void)) );

	subMenu->addAction(act);

	// Emulation -> Speed -> Slow Down
	act = new QAction(tr("Slow &Down"), this);
	//act->setShortcut( QKeySequence(tr("-")));
	act->setStatusTip(tr("Slow Down"));
	act->setIcon( style()->standardIcon( QStyle::SP_MediaSeekBackward ) );
	connect(act, SIGNAL(triggered()), this, SLOT(emuSlowDown(void)) );
	
	Hotkeys[ HK_DECREASE_SPEED ].setAction( act );
	connect( Hotkeys[ HK_DECREASE_SPEED ].getShortcut(), SIGNAL(activated()), this, SLOT(emuSlowDown(void)) );

	subMenu->addAction(act);
	
	subMenu->addSeparator();

	// Emulation -> Speed -> Slowest Speed
	act = new QAction(tr("&Slowest"), this);
	//act->setShortcut( QKeySequence(tr("-")));
	act->setStatusTip(tr("Slowest"));
	act->setIcon( style()->standardIcon( QStyle::SP_MediaSkipBackward ) );
	connect(act, SIGNAL(triggered()), this, SLOT(emuSlowestSpd(void)) );
	
	subMenu->addAction(act);

	// Emulation -> Speed -> Normal Speed
	act = new QAction(tr("&Normal"), this);
	//act->setShortcut( QKeySequence(tr("-")));
	act->setStatusTip(tr("Normal"));
	act->setIcon( style()->standardIcon( QStyle::SP_MediaPlay ) );
	connect(act, SIGNAL(triggered()), this, SLOT(emuNormalSpd(void)) );
	
	subMenu->addAction(act);
	
	// Emulation -> Speed -> Fastest Speed
	act = new QAction(tr("&Turbo"), this);
	//act->setShortcut( QKeySequence(tr("-")));
	act->setStatusTip(tr("Turbo (Fastest)"));
	act->setIcon( style()->standardIcon( QStyle::SP_MediaSkipForward ) );
	connect(act, SIGNAL(triggered()), this, SLOT(emuFastestSpd(void)) );
	
	subMenu->addAction(act);
	
	// Emulation -> Speed -> Custom Speed
	act = new QAction(tr("&Custom"), this);
	//act->setShortcut( QKeySequence(tr("-")));
	act->setStatusTip(tr("Custom"));
	connect(act, SIGNAL(triggered()), this, SLOT(emuCustomSpd(void)) );
	
	subMenu->addAction(act);
	
	subMenu->addSeparator();
	
	// Emulation -> Speed -> Set Frame Advance Delay
	act = new QAction(tr("Set Frame &Advance Delay"), this);
	//act->setShortcut( QKeySequence(tr("-")));
	act->setStatusTip(tr("Set Frame Advance Delay"));
	connect(act, SIGNAL(triggered()), this, SLOT(emuSetFrameAdvDelay(void)) );
	
	subMenu->addAction(act);

	emuMenu->addSeparator();

	// Emulation -> AutoFire Pattern
	subMenu = emuMenu->addMenu(tr("&AutoFire Pattern"));
	
	group   = new QActionGroup(this);
	group->setExclusive(true);

	for (int i=1; i<6; i++)
	{
		char stmp[64];

		for (int j=1; j<=(6-i); j++)
		{
			snprintf( stmp, sizeof(stmp), "%i On, %i Off", i, j );
			autoFireMenuAction *afAct = new autoFireMenuAction( i, j, tr(stmp), this);
			afAct->setCheckable(true);
			group->addAction(afAct);
			subMenu->addAction(afAct);
			afActList.push_back(afAct);

			connect( afAct, SIGNAL(triggered(void)), afAct, SLOT(activateCB(void)) );
		}
	}

	g_config->getOption("SDL.AutofireCustomOnFrames"  , &customAutofireOnFrames );
	g_config->getOption("SDL.AutofireCustomOffFrames" , &customAutofireOffFrames);

	afActCustom = new autoFireMenuAction( customAutofireOnFrames, customAutofireOffFrames, tr("Custom"), this);
	afActCustom->setCheckable(true);
	group->addAction(afActCustom);
	subMenu->addAction(afActCustom);
	//afActList.push_back(afAct);

	connect( afActCustom, SIGNAL(triggered(void)), afActCustom, SLOT(activateCB(void)) );

	subMenu->addSeparator();

	syncAutoFirePatternMenu();

	// Emulation -> AutoFire Pattern -> Set Custom Pattern
	act = new QAction(tr("Set Custom Pattern"), this);
	act->setStatusTip(tr("Set Custom Pattern"));
	connect(act, SIGNAL(triggered()), this, SLOT(setCustomAutoFire(void)) );
	
	subMenu->addAction(act);

	//-----------------------------------------------------------------------
	// NetPlay

	connect( netPlayMenu, SIGNAL(aboutToShow(void)), this, SLOT(mainMenuOpen(void)) );
	connect( netPlayMenu, SIGNAL(aboutToHide(void)), this, SLOT(mainMenuClose(void)) );

	// NetPlay -> Host
	act = new QAction(tr("&Host"), this);
	//act->setShortcut( QKeySequence(tr("Shift+F7")));
	act->setStatusTip(tr("Host Game Window"));
	connect(act, SIGNAL(triggered()), this, SLOT(openNetPlayHostWindow(void)) );
	netPlayHostAct = act;

	netPlayMenu->addAction(act);

	// NetPlay -> Join
	act = new QAction(tr("&Join"), this);
	//act->setShortcut( QKeySequence(tr("Shift+F7")));
	act->setStatusTip(tr("Join Game Window"));
	connect(act, SIGNAL(triggered()), this, SLOT(openNetPlayJoinWindow(void)) );
	netPlayJoinAct = act;

	netPlayMenu->addAction(act);

	// NetPlay -> Client Status Dialog
	act = new QAction(tr("Host &Status"), this);
	//act->setShortcut( QKeySequence(tr("Shift+F7")));
	act->setStatusTip(tr("Open Netplay Host Status Dialog"));
	connect(act, SIGNAL(triggered()), this, SLOT(openNetPlayStatusWindow(void)) );
	act->setEnabled(false);
	act->setVisible(false);
	netPlayHostStatAct = act;

	netPlayMenu->addAction(act);

	// NetPlay -> Client Status Dialog
	act = new QAction(tr("Client &Status"), this);
	//act->setShortcut( QKeySequence(tr("Shift+F7")));
	act->setStatusTip(tr("Open Netplay Client Status Dialog"));
	connect(act, SIGNAL(triggered()), this, SLOT(openNetPlayStatusWindow(void)) );
	act->setEnabled(false);
	act->setVisible(false);
	netPlayClientStatAct = act;

	netPlayMenu->addAction(act);

	netPlayMenu->addSeparator();
	act = new QAction(tr(""), this);
	act->setEnabled(false);
	netPlayMenu->addAction(act);

	// NetPlay -> End Game / Disconnect
	act = new QAction(tr("&Disconnect/End Game"), this);
	//act->setShortcut( QKeySequence(tr("Shift+F7")));
	act->setStatusTip(tr("Disconnect Netplay Game"));
	connect(act, SIGNAL(triggered()), this, SLOT(closeNetPlaySession(void)) );
	act->setEnabled(false);
	netPlayDiscAct = act;

	netPlayMenu->addAction(act);


	//netPlayMenu->setEnabled(false);
	//-----------------------------------------------------------------------
	// Tools

	connect( toolsMenu, SIGNAL(aboutToShow(void)), this, SLOT(mainMenuOpen(void)) );
	connect( toolsMenu, SIGNAL(aboutToHide(void)), this, SLOT(mainMenuClose(void)) );

	// Tools -> Cheats
	cheatsAct = new QAction(tr("&Cheats..."), this);
	//cheatsAct->setShortcut( QKeySequence(tr("Shift+F7")));
	cheatsAct->setStatusTip(tr("Open Cheat Window"));
	connect(cheatsAct, SIGNAL(triggered()), this, SLOT(openCheats(void)) );
	
	Hotkeys[ HK_CHEAT_MENU ].setAction( cheatsAct );
	connect( Hotkeys[ HK_CHEAT_MENU ].getShortcut(), SIGNAL(activated()), this, SLOT(openCheats(void)) );

	toolsMenu->addAction(cheatsAct);

	// Tools -> RAM Search
	ramSearchAct = new QAction(tr("RAM &Search..."), this);
	//ramSearchAct->setShortcut( QKeySequence(tr("Shift+F7")));
	ramSearchAct->setStatusTip(tr("Open RAM Search Window"));
	connect(ramSearchAct, SIGNAL(triggered()), this, SLOT(openRamSearch(void)) );
	
	toolsMenu->addAction(ramSearchAct);

	// Tools -> RAM Watch
	ramWatchAct = new QAction(tr("RAM &Watch..."), this);
	//ramWatchAct->setShortcut( QKeySequence(tr("Shift+F7")));
	ramWatchAct->setStatusTip(tr("Open RAM Watch Window"));
	connect(ramWatchAct, SIGNAL(triggered()), this, SLOT(openRamWatch(void)) );
	
	toolsMenu->addAction(ramWatchAct);

	// Tools -> Frame Timing
	act = new QAction(tr("&Frame Timing ..."), this);
	//act->setShortcut( QKeySequence(tr("Shift+F7")));
	act->setStatusTip(tr("Open Frame Timing Window"));
	connect(act, SIGNAL(triggered()), this, SLOT(openTimingStatWin(void)) );

	toolsMenu->addAction(act);

	// Tools -> Palette Editor
	act = new QAction(tr("&Palette Editor ..."), this);
	//act->setShortcut( QKeySequence(tr("Shift+F7")));
	act->setStatusTip(tr("Open Palette Editor Window"));
	connect(act, SIGNAL(triggered()), this, SLOT(openPaletteEditorWin(void)) );

	toolsMenu->addAction(act);

	// Tools -> AVI RIFF Viewer
	act = new QAction(tr("&AVI RIFF Viewer ..."), this);
	//act->setShortcut( QKeySequence(tr("Shift+F7")));
	act->setStatusTip(tr("Open AVI RIFF Viewer Window"));
	connect(act, SIGNAL(triggered()), this, SLOT(openAviRiffViewer(void)) );

	toolsMenu->addAction(act);

	// Tools -> TAS Editor
	tasEditorAct = act = new QAction(tr("&TAS Editor ..."), this);
	//act->setShortcut( QKeySequence(tr("Shift+F7")));
	act->setStatusTip(tr("Open TAS Editor Window"));
	connect(act, SIGNAL(triggered()), this, SLOT(openTasEditor(void)) );

	toolsMenu->addAction(act);

#ifdef __FCEU_REST_API_ENABLE__
	// Tools -> REST API Server
	restApiAct = act = new QAction(tr("&REST API Server"), this);
	act->setCheckable(true);
	act->setStatusTip(tr("Enable/disable REST API server"));
	connect(act, &QAction::toggled, this, &consoleWin_t::toggleRestApiServer);
	
	// Set initial checked state based on server status
	bool restApiEnabled = false;
	g_config->getOption("SDL.RestApiEnabled", &restApiEnabled);
	act->setChecked(restApiEnabled);
	
	toolsMenu->addAction(act);
#endif

	 //-----------------------------------------------------------------------
	 // Debug

	connect( debugMenu, SIGNAL(aboutToShow(void)), this, SLOT(mainMenuOpen(void)) );
	connect( debugMenu, SIGNAL(aboutToHide(void)), this, SLOT(mainMenuClose(void)) );

	// Debug -> Debugger 
	debuggerAct = new QAction(tr("&Debugger..."), this);
	//debuggerAct->setShortcut( QKeySequence(tr("Shift+F7")));
	debuggerAct->setStatusTip(tr("Open 6502 Debugger"));
	connect(debuggerAct, SIGNAL(triggered()), this, SLOT(openDebugWindow(void)) );
	
	debugMenu->addAction(debuggerAct);

	// Debug -> Hex Editor
	hexEditAct = new QAction(tr("&Hex Editor..."), this);
	//hexEditAct->setShortcut( QKeySequence(tr("Shift+F7")));
	hexEditAct->setStatusTip(tr("Open Memory Hex Editor"));
	connect(hexEditAct, SIGNAL(triggered()), this, SLOT(openHexEditor(void)) );
	
	debugMenu->addAction(hexEditAct);

	// Debug -> PPU Viewer
	ppuViewAct = new QAction(tr("&PPU Viewer..."), this);
	//ppuViewAct->setShortcut( QKeySequence(tr("Shift+F7")));
	ppuViewAct->setStatusTip(tr("Open PPU Viewer"));
	connect(ppuViewAct, SIGNAL(triggered()), this, SLOT(openPPUViewer(void)) );
	
	debugMenu->addAction(ppuViewAct);

	// Debug -> Sprite Viewer
	oamViewAct = new QAction(tr("&Sprite Viewer..."), this);
	//oamViewAct->setShortcut( QKeySequence(tr("Shift+F7")));
	oamViewAct->setStatusTip(tr("Open Sprite Viewer"));
	connect(oamViewAct, SIGNAL(triggered()), this, SLOT(openOAMViewer(void)) );
	
	debugMenu->addAction(oamViewAct);

	// Debug -> Name Table Viewer
	ntViewAct = new QAction(tr("&Name Table Viewer..."), this);
	//ntViewAct->setShortcut( QKeySequence(tr("Shift+F7")));
	ntViewAct->setStatusTip(tr("Open Name Table Viewer"));
	connect(ntViewAct, SIGNAL(triggered()), this, SLOT(openNTViewer(void)) );
	
	debugMenu->addAction(ntViewAct);

	// Debug -> Trace Logger
	traceLogAct = new QAction(tr("&Trace Logger..."), this);
	//traceLogAct->setShortcut( QKeySequence(tr("Shift+F7")));
	traceLogAct->setStatusTip(tr("Open Trace Logger"));
	connect(traceLogAct, SIGNAL(triggered()), this, SLOT(openTraceLogger(void)) );
	
	debugMenu->addAction(traceLogAct);

	// Debug -> Code/Data Logger
	codeDataLogAct = new QAction(tr("&Code/Data Logger..."), this);
	//codeDataLogAct->setShortcut( QKeySequence(tr("Shift+F7")));
	codeDataLogAct->setStatusTip(tr("Open Code Data Logger"));
	connect(codeDataLogAct, SIGNAL(triggered()), this, SLOT(openCodeDataLogger(void)) );
	
	debugMenu->addAction(codeDataLogAct);

	// Debug -> Game Genie Encode/Decode Viewer
	ggEncodeAct = new QAction(tr("&Game Genie Encode/Decode"), this);
	//ggEncodeAct->setShortcut( QKeySequence(tr("Shift+F7")));
	ggEncodeAct->setStatusTip(tr("Open Game Genie Encode/Decode"));
	connect(ggEncodeAct, SIGNAL(triggered()), this, SLOT(openGGEncoder(void)) );
	
	debugMenu->addAction(ggEncodeAct);

	// Debug -> NES Header Editor
	iNesEditAct = new QAction(tr("NES Header Edito&r..."), this);
	//iNesEditAct->setShortcut( QKeySequence(tr("Shift+F7")));
	iNesEditAct->setStatusTip(tr("Open NES Header Editor"));
	connect(iNesEditAct, SIGNAL(triggered()), this, SLOT(openNesHeaderEditor(void)) );
	
	debugMenu->addAction(iNesEditAct);

	//-----------------------------------------------------------------------
	// Movie

	connect( movieMenu, SIGNAL(aboutToShow(void)), this, SLOT(mainMenuOpen(void)) );
	connect( movieMenu, SIGNAL(aboutToHide(void)), this, SLOT(mainMenuClose(void)) );

	// Movie -> Play
	openMovAct = new QAction(tr("Movie &Play"), this);
	//openMovAct->setShortcut( QKeySequence(tr("Shift+F7")));
	openMovAct->setStatusTip(tr("Play Movie File"));
	openMovAct->setIcon( style()->standardIcon( QStyle::SP_MediaPlay ) );
	connect(openMovAct, SIGNAL(triggered()), this, SLOT(openMovie(void)) );
	
	Hotkeys[ HK_PLAY_MOVIE_FROM ].setAction( openMovAct );
	connect( Hotkeys[ HK_PLAY_MOVIE_FROM ].getShortcut(), SIGNAL(activated()), this, SLOT(openMovie(void)) );

	movieMenu->addAction(openMovAct);

	// Movie -> Play From Beginning
	playMovBeginAct = new QAction(tr("Movie Play From &Beginning"), this);
	//playMovBeginAct->setShortcut( QKeySequence(tr("Shift+F7")));
	playMovBeginAct->setStatusTip(tr("Play Movie From Beginning"));
	//playMovBeginAct->setIcon( style()->standardIcon( QStyle::SP_MediaPlay ) );
	connect(playMovBeginAct, SIGNAL(triggered()), this, SLOT(playMovieFromBeginning(void)) );
	
	Hotkeys[ HK_MOVIE_PLAY_RESTART ].setAction( playMovBeginAct );
	connect( Hotkeys[ HK_MOVIE_PLAY_RESTART ].getShortcut(), SIGNAL(activated()), this, SLOT(playMovieFromBeginning(void)) );

	movieMenu->addAction(playMovBeginAct);

	// Movie -> Stop
	stopMovAct = new QAction(tr("Movie &Stop"), this);
	//stopMovAct->setShortcut( QKeySequence(tr("Shift+F7")));
	stopMovAct->setStatusTip(tr("Stop Movie Recording"));
	stopMovAct->setIcon( style()->standardIcon( QStyle::SP_MediaStop ) );
	connect(stopMovAct, SIGNAL(triggered()), this, SLOT(stopMovie(void)) );
	
	Hotkeys[ HK_STOP_MOVIE ].setAction( stopMovAct );
	connect( Hotkeys[ HK_STOP_MOVIE ].getShortcut(), SIGNAL(activated()), this, SLOT(stopMovie(void)) );

	movieMenu->addAction(stopMovAct);
	
	movieMenu->addSeparator();

	// Movie -> Record
	recMovAct = new QAction(tr("Movie &Record"), this);
	//recMovAct->setShortcut( QKeySequence(tr("Shift+F5")));
	recMovAct->setStatusTip(tr("Record Movie"));
	recMovAct->setIcon( QIcon(":icons/media-record.png") );
	connect(recMovAct, SIGNAL(triggered()), this, SLOT(recordMovie(void)) );
	
	Hotkeys[ HK_RECORD_MOVIE_TO ].setAction( recMovAct );
	connect( Hotkeys[ HK_RECORD_MOVIE_TO ].getShortcut(), SIGNAL(activated()), this, SLOT(recordMovie(void)) );

	movieMenu->addAction(recMovAct);

	movieMenu->addSeparator();

	// Movie -> Avi Recording

	// Movie -> Avi Recording -> Record
	recAviAct = new QAction(tr("AVI &Record"), this);
	//recAviAct->setShortcut( QKeySequence(tr("Shift+F5")));
	recAviAct->setStatusTip(tr("AVI Record Start"));
	recAviAct->setIcon( QIcon(":icons/media-record.png") );
	connect(recAviAct, SIGNAL(triggered()), this, SLOT(aviRecordStart(void)) );
	
	Hotkeys[ HK_RECORD_AVI ].setAction( recAviAct );
	connect( Hotkeys[ HK_RECORD_AVI ].getShortcut(), SIGNAL(activated()), this, SLOT(aviRecordStart(void)) );

	movieMenu->addAction(recAviAct);

	// Movie -> Avi Recording -> Record As
	recAsAviAct = new QAction(tr("AVI Record &As"), this);
	//recAsAviAct->setShortcut( QKeySequence(tr("Shift+F5")));
	recAsAviAct->setStatusTip(tr("AVI Record As Start"));
	//recAsAviAct->setIcon( QIcon(":icons/media-record.png") );
	connect(recAsAviAct, SIGNAL(triggered()), this, SLOT(aviRecordAsStart(void)) );
	
	Hotkeys[ HK_RECORD_AVI_TO ].setAction( recAsAviAct );
	connect( Hotkeys[ HK_RECORD_AVI_TO ].getShortcut(), SIGNAL(activated()), this, SLOT(aviRecordAsStart(void)) );

	movieMenu->addAction(recAsAviAct);

	// Movie -> Avi Recording -> Stop
	stopAviAct = new QAction(tr("AVI &Stop"), this);
	//stopAviAct->setShortcut( QKeySequence(tr("Shift+F5")));
	stopAviAct->setStatusTip(tr("AVI Record Stop"));
	stopAviAct->setIcon( style()->standardIcon( QStyle::SP_MediaStop ) );
	connect(stopAviAct, SIGNAL(triggered()), this, SLOT(aviRecordStop(void)) );
	
	Hotkeys[ HK_STOP_AVI ].setAction( stopAviAct );
	connect( Hotkeys[ HK_STOP_AVI ].getShortcut(), SIGNAL(activated()), this, SLOT(aviRecordStop(void)) );

	movieMenu->addAction(stopAviAct);

	movieMenu->addSeparator();

	// Movie -> WAV Recording

	// Movie -> WAV Recording -> Record
	recWavAct = new QAction(tr("WAV &Record"), this);
	//recWavAct->setShortcut( QKeySequence(tr("Shift+F5")));
	recWavAct->setStatusTip(tr("WAV Record Start"));
	recWavAct->setIcon( QIcon(":icons/media-record.png") );
	connect(recWavAct, SIGNAL(triggered()), this, SLOT(wavRecordStart(void)) );
	
	Hotkeys[ HK_RECORD_WAV ].setAction( recWavAct );
	connect( Hotkeys[ HK_RECORD_WAV ].getShortcut(), SIGNAL(activated()), this, SLOT(wavRecordStart(void)) );

	movieMenu->addAction(recWavAct);

	// Movie -> WAV Recording -> Record As
	recAsWavAct = new QAction(tr("WAV Record &As"), this);
	//recAsWavAct->setShortcut( QKeySequence(tr("Shift+F5")));
	recAsWavAct->setStatusTip(tr("WAV Record As Start"));
	//recAsWavAct->setIcon( QIcon(":icons/media-record.png") );
	connect(recAsWavAct, SIGNAL(triggered()), this, SLOT(wavRecordAsStart(void)) );
	
	Hotkeys[ HK_RECORD_WAV_TO ].setAction( recAsWavAct );
	connect( Hotkeys[ HK_RECORD_WAV_TO ].getShortcut(), SIGNAL(activated()), this, SLOT(wavRecordAsStart(void)) );

	movieMenu->addAction(recAsWavAct);

	// Movie -> WAV Recording -> Stop
	stopWavAct = new QAction(tr("WAV &Stop"), this);
	//stopWavAct->setShortcut( QKeySequence(tr("Shift+F5")));
	stopWavAct->setStatusTip(tr("WAV Record Stop"));
	stopWavAct->setIcon( style()->standardIcon( QStyle::SP_MediaStop ) );
	connect(stopWavAct, SIGNAL(triggered()), this, SLOT(wavRecordStop(void)) );
	
	Hotkeys[ HK_STOP_WAV ].setAction( stopWavAct );
	connect( Hotkeys[ HK_STOP_WAV ].getShortcut(), SIGNAL(activated()), this, SLOT(wavRecordStop(void)) );

	movieMenu->addAction(stopWavAct);

	//-----------------------------------------------------------------------
	// Help
 
	connect( helpMenu, SIGNAL(aboutToShow(void)), this, SLOT(mainMenuOpen(void)) );
	connect( helpMenu, SIGNAL(aboutToHide(void)), this, SLOT(mainMenuClose(void)) );

	// Help -> About FCEUX
	aboutAct = new QAction(tr("&About FCEUX"), this);
	aboutAct->setStatusTip(tr("About FCEUX"));
	aboutAct->setIcon( style()->standardIcon( QStyle::SP_MessageBoxInformation ) );
	connect(aboutAct, SIGNAL(triggered()), this, SLOT(aboutFCEUX(void)) );
	
	helpMenu->addAction(aboutAct);

	// Help -> About Qt
	aboutActQt = new QAction(tr("About &Qt"), this);
	aboutActQt->setStatusTip(tr("About Qt"));
	aboutActQt->setIcon( style()->standardIcon( QStyle::SP_TitleBarMenuButton ) );
	connect(aboutActQt, SIGNAL(triggered()), this, SLOT(aboutQt(void)) );
	
	helpMenu->addAction(aboutActQt);

	// Help -> Message Log
	msgLogAct = new QAction(tr("&Message Log"), this);
	msgLogAct->setStatusTip(tr("Message Log"));
	msgLogAct->setIcon( style()->standardIcon( QStyle::SP_MessageBoxWarning ) );
	connect(msgLogAct, SIGNAL(triggered()), this, SLOT(openMsgLogWin(void)) );
	
	helpMenu->addAction(msgLogAct);

	// Help -> Documentation Online
	subMenu = helpMenu->addMenu( tr("&Documentation") );
	subMenu->setIcon( style()->standardIcon( QStyle::SP_DialogHelpButton ) );

	// Help -> Documentation Online
	act = new QAction(tr("&Online"), this);
	act->setStatusTip(tr("Documentation"));
	//act->setIcon( style()->standardIcon( QStyle::SP_DialogHelpButton ) );
	connect(act, SIGNAL(triggered()), this, SLOT(openOnlineDocs(void)) );
	
	subMenu->addAction(act);

#if  defined(WIN32) || defined(_USE_QHELP)
	// Help -> Documentation Offline
	act = new QAction(tr("&Local"), this);
	act->setStatusTip(tr("Documentation"));
	//act->setIcon( style()->standardIcon( QStyle::SP_DialogHelpButton ) );
	connect(act, SIGNAL(triggered()), this, SLOT(openOfflineDocs(void)) );

	subMenu->addAction(act);
#endif
};
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
int consoleWin_t::unloadVideoDriver(void)
{
	viewport_Interface = NULL;

	if (viewport_GL != NULL)
	{
		if ( viewport_GL == centralWidget() )
		{
			takeCentralWidget();
		}
		else
		{
			printf("Error: Central Widget Failed!\n");
		}
		viewport_GL->deleteLater();

		viewport_GL = NULL;
	}

	if (viewport_SDL != NULL)
	{
		if ( viewport_SDL == centralWidget() )
		{
			takeCentralWidget();
		}
		else
		{
			printf("Error: Central Widget Failed!\n");
		}
		viewport_SDL->deleteLater();

		viewport_SDL = NULL;
	}

	if (viewport_QWidget != NULL)
	{
		if ( viewport_QWidget == centralWidget() )
		{
			takeCentralWidget();
		}
		else
		{
			printf("Error: Central Widget Failed!\n");
		}
		viewport_QWidget->deleteLater();

		viewport_QWidget = NULL;
	}
	return 0;
}
//---------------------------------------------------------------------------
void consoleWin_t::videoDriverDestroyed(QObject* obj)
{
	if (viewport_GL == obj)
	{
		//printf("GL Video Driver Destroyed\n");

		if (viewport_Interface == static_cast<ConsoleViewerBase*>(viewport_GL))
		{
			viewport_Interface = NULL;
		}
		viewport_GL = NULL;
	}

	if (viewport_SDL == obj)
	{
		//printf("SDL Video Driver Destroyedi\n");

		if (viewport_Interface == static_cast<ConsoleViewerBase*>(viewport_SDL))
		{
			viewport_Interface = NULL;
		}
		viewport_SDL = NULL;
	}

	if (viewport_QWidget == obj)
	{
		//printf("QPainter Video Driver Destroyed\n");

		if (viewport_Interface == static_cast<ConsoleViewerBase*>(viewport_QWidget))
		{
			viewport_Interface = NULL;
		}
		viewport_QWidget = NULL;
	}
	printf("Video Driver Destroyed: %p\n", obj);
	//printf("viewport_GL: %p\n", viewport_GL);
	//printf("viewport_SDL: %p\n", viewport_SDL);
	//printf("viewport_Qt: %p\n", viewport_QWidget);
	//printf("viewport_Interface: %p\n", viewport_Interface);
}
//---------------------------------------------------------------------------
int consoleWin_t::loadVideoDriver( int driverId, bool force )
{
	if (viewport_Interface)
	{
		if (viewport_Interface->driver() == driverId)
		{  // Already Loaded
			if (force)
			{
				unloadVideoDriver();
			}
			else
			{
				return 0;
			}
		}
	}

	switch ( driverId )
	{  
		case ConsoleViewerBase::VIDEO_DRIVER_SDL:
		{
			viewport_SDL = new ConsoleViewSDL_t(this);

			viewport_Interface = static_cast<ConsoleViewerBase*>(viewport_SDL);

			setCentralWidget(viewport_SDL);

			setViewportAspect();

			viewport_SDL->init();

			connect( viewport_SDL, SIGNAL(destroyed(QObject*)), this, SLOT(videoDriverDestroyed(QObject*)) );
		}
		break;
		case ConsoleViewerBase::VIDEO_DRIVER_OPENGL:
		{
			viewport_GL = new ConsoleViewGL_t(this);

			viewport_Interface = static_cast<ConsoleViewerBase*>(viewport_GL);

			setCentralWidget(viewport_GL);

			setViewportAspect();

			viewport_GL->init();

			connect( viewport_GL, SIGNAL(destroyed(QObject*)), this, SLOT(videoDriverDestroyed(QObject*)) );
		}
		break;
		default:
		case ConsoleViewerBase::VIDEO_DRIVER_QPAINTER:
		{
			viewport_QWidget = new ConsoleViewQWidget_t(this);

			viewport_Interface = static_cast<ConsoleViewerBase*>(viewport_QWidget);

			setCentralWidget(viewport_QWidget);

			setViewportAspect();

			viewport_QWidget->init();

			connect( viewport_QWidget, SIGNAL(destroyed(QObject*)), this, SLOT(videoDriverDestroyed(QObject*)) );
		}
		break;
	}

	// Reload Viewport Cursor Type and Visibility
	loadCursor();

	return 0;
}
//---------------------------------------------------------------------------
void consoleWin_t::clearRomList(void)
{
	std::list <std::string*>::iterator it;

	for (it=romList.begin(); it != romList.end(); it++)
	{
		delete *it;
	}
	romList.clear();
}
//---------------------------------------------------------------------------
void consoleWin_t::buildRecentRomMenu(void)
{
	QAction *act;
	std::string s;
	std::string *sptr;
	char buf[128];

	clearRomList();
	recentRomMenu->clear();

	for (int i=0; i<10; i++)
	{
		snprintf(buf, sizeof(buf), "SDL.RecentRom%02i", i);

		g_config->getOption( buf, &s);

		//printf("Recent Rom:%i  '%s'\n", i, s.c_str() );
		bool fileExists = !s.empty() && QFile::exists(tr(s.c_str()));

		if ( fileExists )
		{
			act = new consoleRecentRomAction( tr(s.c_str()), recentRomMenu);

			recentRomMenu->addAction( act );

			connect(act, SIGNAL(triggered()), act, SLOT(activateCB(void)) );

			sptr = new std::string();

			sptr->assign( s.c_str() );

			romList.push_front( sptr );
		}
		else
		{
			// Clear the option if file does not exist
			s.clear();

			g_config->setOption( buf, s);
		}
	}
	// Add a dummy disable QAction to create a larger dead space between the ROM list and the clear item.
	// Helps prevent accidental unintended clicking of the clear list item
	recentRomMenu->addSeparator();
	act = new QAction(recentRomMenu);
	act->setEnabled(false);
	recentRomMenu->addAction(act);

	act = new QAction(tr("Clear Recent ROM List"), recentRomMenu);
	connect(act, SIGNAL(triggered()), this, SLOT(clearRecentRomMenu(void)) );
	recentRomMenu->addAction(act);
}
//---------------------------------------------------------------------------
void consoleWin_t::saveRecentRomMenu(void)
{
	int i;
	std::string *s;
	std::list <std::string*>::iterator it;
	char buf[128];

	i = romList.size() - 1;

	for (it=romList.begin(); it != romList.end(); it++)
	{
		s = *it;
		snprintf(buf, sizeof(buf), "SDL.RecentRom%02i", i);

		g_config->setOption( buf, *s );

		//printf("Recent Rom:%u  '%s'\n", i, s->c_str() );
		i--;
	}
	for (i = romList.size(); i < 10; i++)
	{
		snprintf(buf, sizeof(buf), "SDL.RecentRom%02i", i);
		g_config->setOption( buf, "");
	}

}
//---------------------------------------------------------------------------
void consoleWin_t::clearRecentRomMenu()
{
	char buf[128];
	for (int i = 0; i < 10; i++)
	{
		snprintf(buf, sizeof(buf), "SDL.RecentRom%02i", i);
		g_config->setOption( buf, "");
	}
	clearRomList();

	recentRomMenuReset = true;
}
//---------------------------------------------------------------------------
void consoleWin_t::addRecentRom( const char *rom )
{
	std::string *s;
	std::list <std::string*>::iterator match_it;

	for (match_it=romList.begin(); match_it != romList.end(); match_it++)
	{
		s = *match_it;

		if ( s->compare( rom ) == 0 )
		{
			//printf("Found Match: %s\n", rom );
			break;
		}
	}

	if ( match_it != romList.end() )
	{
		s = *match_it;

		romList.erase(match_it);

		romList.push_back(s);
	}
	else
	{
		s = new std::string();

		s->assign( rom );
		
		romList.push_back(s);

		if ( romList.size() > 10 )
		{
			s = romList.front();

			romList.pop_front();

			delete s;
		}
	}

	saveRecentRomMenu();

	recentRomMenuReset = true;
}
//---------------------------------------------------------------------------
void consoleWin_t::toggleMenuVis(void)
{
	if ( menubar->isVisible() )
	{
		menubar->setVisible( false );
	}
	else
	{
		menubar->setVisible( true );
	}
}
//---------------------------------------------------------------------------
void consoleWin_t::toggleMenuAutoHide(bool checked)
{
	autoHideMenuFullscreen = checked;

	g_config->setOption( "SDL.AutoHideMenuFullsreen", autoHideMenuFullscreen );
	g_config->save();
}
//---------------------------------------------------------------------------
void consoleWin_t::toggleUseBgPaletteForVideo(bool checked)
{
	usePaletteForVideoBg = checked;

	g_config->setOption( "SDL.UseBgPaletteForVideo", usePaletteForVideoBg );
	g_config->save();

	if ( !usePaletteForVideoBg )
	{
		fceuLoadConfigColor( "SDL.VideoBgColor", &videoBgColor );
	}
	bgColorMenuItem->setEnabled( !usePaletteForVideoBg );
}
//---------------------------------------------------------------------------
void consoleWin_t::closeApp(void)
{
	nes_shm->runEmulator = 0;

	gameTimer->stop();

	closeGamePadConfWindow();

	emulatorThread->quit();
	emulatorThread->wait( 1000 );

	aviDiskThread->requestInterruption();
	aviDiskThread->quit();
	aviDiskThread->wait( 10000 );

	if ( tasWin != NULL )
	{
		tasWin->requestWindowClose();
	}

	FCEU_WRAPPER_LOCK();
	fceuWrapperClose();
	FCEU_WRAPPER_UNLOCK();

	// LoadGame() checks for an IP and if it finds one begins a network session
	// clear the NetworkIP field so this doesn't happen unintentionally
	g_config->setOption ("SDL.NetworkIP", "");
	g_config->save ();

	QApplication::closeAllWindows();

	// Delay Application Quit to allow event processing to complete
	QTimer::singleShot( 250, qApp, SLOT(quit(void)) );
}
//---------------------------------------------------------------------------
void consoleWin_t::videoBgColorChanged( QColor &c )
{
	//printf("Color Changed\n");

	if ( viewport_Interface )
	{
		viewport_Interface->setBgColor(c);
		viewport_Interface->queueRedraw();
	}
}
//---------------------------------------------------------------------------
int  consoleWin_t::showListSelectDialog( const char *title, std::vector <std::string> &l )
{
	if ( QThread::currentThread() == emulatorThread )
	{
		printf("Cannot display list selection dialog from within emulation thread...\n");
		return 0;
	}
	int ret, idx = 0;
	QDialog dialog(this);
	QVBoxLayout *mainLayout;
	QHBoxLayout *hbox;
	QPushButton *okButton, *cancelButton;
	QTreeWidget *tree;
	QTreeWidgetItem *item;
	QSettings  settings;

	dialog.setWindowTitle( tr(title) );

	tree = new QTreeWidget();

	tree->setColumnCount(1);

	item = new QTreeWidgetItem();
	item->setText( 0, QString::fromStdString( "File" ) );
	item->setTextAlignment( 0, Qt::AlignLeft);

	tree->setHeaderItem( item );

	tree->header()->setSectionResizeMode( QHeaderView::ResizeToContents );

	for (size_t i=0; i<l.size(); i++)
	{
		item = new QTreeWidgetItem();

		item->setText( 0, QString::fromStdString( l[i] ) );

		item->setTextAlignment( 0, Qt::AlignLeft);

		tree->addTopLevelItem( item );
	}

	mainLayout = new QVBoxLayout();

	hbox         = new QHBoxLayout();
	okButton     = new QPushButton( tr("OK") );
	cancelButton = new QPushButton( tr("Cancel") );

	mainLayout->addWidget( tree );
	mainLayout->addLayout( hbox );
	hbox->addWidget( cancelButton );
	hbox->addWidget(     okButton );

	connect(     okButton, SIGNAL(clicked(void)), &dialog, SLOT(accept(void)) );
	connect( cancelButton, SIGNAL(clicked(void)), &dialog, SLOT(reject(void)) );

	    okButton->setIcon( style()->standardIcon( QStyle::SP_DialogOkButton ) );
	cancelButton->setIcon( style()->standardIcon( QStyle::SP_DialogCancelButton ) );

	okButton->setDefault(true);

	dialog.setLayout( mainLayout );

	// Restore Window Geometry
	dialog.restoreGeometry(settings.value("ArchiveViewer/geometry").toByteArray());

	// Run Dialog Execution Loop
	ret = dialog.exec();

	// Save Window Geometry
	settings.setValue("ArchiveViewer/geometry", dialog.saveGeometry());

	if ( ret == QDialog::Accepted )
	{
		idx = 0;

		item = tree->currentItem();

		if ( item != NULL )
		{
			idx = tree->indexOfTopLevelItem(item);
		}
	}
	else
	{
		idx = -1;
	}
	return idx;
}
//---------------------------------------------------------------------------

void consoleWin_t::openROMFile(void)
{
	int ret, useNativeFileDialogVal;
	QString filename;
	std::string last;
	std::string dir;
	const char *romDir;
	QFileDialog  dialog(this, tr("Open ROM File") );
	QList<QUrl> urls;
	QDir d;

	const QStringList filters(
			{ "All Useable files (*.nes *.NES *.nsf *.NSF *.fds *.FDS *.unf *.UNF *.unif *.UNIF *.zip *.ZIP, *.7z *.7zip)",
           "NES files (*.nes *.NES)",
           "NSF files (*.nsf *.NSF)",
           "UNF files (*.unf *.UNF *.unif *.UNIF)",
           "FDS files (*.fds *.FDS)",
           "Any files (*)"
         });

	urls << QUrl::fromLocalFile( QDir::rootPath() );
	urls << QUrl::fromLocalFile(QStandardPaths::standardLocations(QStandardPaths::HomeLocation).first());
	urls << QUrl::fromLocalFile(QStandardPaths::standardLocations(QStandardPaths::DesktopLocation).first());
	urls << QUrl::fromLocalFile(QStandardPaths::standardLocations(QStandardPaths::DownloadLocation).first());
	urls << QUrl::fromLocalFile( QDir( FCEUI_GetBaseDirectory() ).absolutePath() );

	romDir = getenv("FCEUX_ROM_PATH");

	if ( romDir != NULL )
	{
		d.setPath(romDir);

		if ( d.exists() )
		{
			urls << QUrl::fromLocalFile( d.absolutePath() );
		}
	}

	dialog.setFileMode(QFileDialog::ExistingFile);

	dialog.setNameFilters( filters );

	dialog.setViewMode(QFileDialog::List);
	dialog.setFilter( QDir::AllEntries | QDir::AllDirs | QDir::Hidden );
	dialog.setLabelText( QFileDialog::Accept, tr("Open") );

	g_config->getOption ("SDL.LastOpenFile", &last );

	getDirFromFile( last.c_str(), dir);

	dialog.setDirectory( tr(dir.c_str()) );

	// Check config option to use native file dialog or not
	g_config->getOption ("SDL.UseNativeFileDialog", &useNativeFileDialogVal);

	dialog.setOption(QFileDialog::DontUseNativeDialog, !useNativeFileDialogVal);
	dialog.setSidebarUrls(urls);

	ret = dialog.exec();

	if ( ret )
	{
		QStringList fileList;
		fileList = dialog.selectedFiles();

		if ( fileList.size() > 0 )
		{
			filename = fileList[0];
		}
	}

	if ( filename.isNull() )
	{
	   return;
	}
	qDebug() << "selected file path : " << filename.toLocal8Bit();

	g_config->setOption ("SDL.LastOpenFile", filename.toLocal8Bit().constData() );

	FCEU_WRAPPER_LOCK();
	CloseGame ();
	LoadGame ( filename.toLocal8Bit().constData() );
	FCEU_WRAPPER_UNLOCK();

   return;
}

void consoleWin_t::loadRomRequestCB( QString s )
{
	printf("Load ROM Req: '%s'\n", s.toLocal8Bit().constData() );
	FCEU_WRAPPER_LOCK();
	CloseGame ();
	LoadGame ( s.toLocal8Bit().constData() );
	FCEU_WRAPPER_UNLOCK();
}

void consoleWin_t::closeROMCB(void)
{
	if (isNetPlayClient())
	{
		QString msgBoxTxt = tr("Unloading ROM will cause a disconnect from the current netplay session.\n\nDo you want to continue with unloading and disconnection?");
		int ans = QMessageBox::question( this, tr("NetPlay Client ROM Unload Warning"), msgBoxTxt, QMessageBox::Yes | QMessageBox::No );

		if (ans == QMessageBox::No)
		{
			return;
		}
		NetPlayCloseSession();
	}

	FCEU_WRAPPER_LOCK();
	CloseGame();
	FCEU_WRAPPER_UNLOCK();
}

void consoleWin_t::loadNSF(void)
{
	int ret, useNativeFileDialogVal;
	QString filename;
	std::string last;
	std::string dir;
	const char *romDir;
	QFileDialog  dialog(this, tr("Load NSF File") );
	QList<QUrl> urls;
	QDir d;

	urls << QUrl::fromLocalFile( QDir::rootPath() );
	urls << QUrl::fromLocalFile(QStandardPaths::standardLocations(QStandardPaths::HomeLocation).first());
	urls << QUrl::fromLocalFile(QStandardPaths::standardLocations(QStandardPaths::DesktopLocation).first());
	urls << QUrl::fromLocalFile(QStandardPaths::standardLocations(QStandardPaths::DownloadLocation).first());
	urls << QUrl::fromLocalFile( QDir( FCEUI_GetBaseDirectory() ).absolutePath() );

	romDir = getenv("FCEUX_ROM_PATH");

	if ( romDir != NULL )
	{
		d.setPath(romDir);

		if ( d.exists() )
		{
			urls << QUrl::fromLocalFile( d.absolutePath() );
		}
	}
	dialog.setFileMode(QFileDialog::ExistingFile);

	dialog.setNameFilter(tr("NSF Sound Files (*.nsf *.NSF) ;; Zip Files (*.zip *.ZIP) ;; All files (*)"));

	dialog.setViewMode(QFileDialog::List);
	dialog.setFilter( QDir::AllEntries | QDir::AllDirs | QDir::Hidden );
	dialog.setLabelText( QFileDialog::Accept, tr("Load") );

	g_config->getOption ("SDL.LastOpenNSF", &last );

	getDirFromFile( last.c_str(), dir );

	dialog.setDirectory( tr(dir.c_str()) );

	// Check config option to use native file dialog or not
	g_config->getOption ("SDL.UseNativeFileDialog", &useNativeFileDialogVal);

	dialog.setOption(QFileDialog::DontUseNativeDialog, !useNativeFileDialogVal);
	dialog.setSidebarUrls(urls);

	ret = dialog.exec();

	if ( ret )
	{
		QStringList fileList;
		fileList = dialog.selectedFiles();

		if ( fileList.size() > 0 )
		{
			filename = fileList[0];
		}
	}

	if ( filename.isNull() )
   {
      return;
   }
	qDebug() << "selected file path : " << filename.toLocal8Bit();

	g_config->setOption ("SDL.LastOpenNSF", filename.toLocal8Bit().constData() );

	FCEU_WRAPPER_LOCK();
	LoadGame( filename.toLocal8Bit().constData() );
	FCEU_WRAPPER_UNLOCK();
}

void consoleWin_t::loadStateFrom(void)
{
	int ret, useNativeFileDialogVal;
	QString filename;
	std::string last;
	std::string dir;
	const char *base;
	QFileDialog  dialog(this, tr("Load State From File") );
	QList<QUrl> urls;
	QDir d;

	base = FCEUI_GetBaseDirectory();

	urls << QUrl::fromLocalFile( QDir::rootPath() );
	urls << QUrl::fromLocalFile(QStandardPaths::standardLocations(QStandardPaths::HomeLocation).first());
	urls << QUrl::fromLocalFile(QStandardPaths::standardLocations(QStandardPaths::DesktopLocation).first());
	urls << QUrl::fromLocalFile(QStandardPaths::standardLocations(QStandardPaths::DownloadLocation).first());

	if ( base )
	{
		urls << QUrl::fromLocalFile( QDir( base ).absolutePath() );

		d.setPath( QString(base) + "/fcs");

		if ( d.exists() )
		{
			urls << QUrl::fromLocalFile( d.absolutePath() );
		}

		d.setPath( QString(base) + "/sav");

		if ( d.exists() )
		{
			urls << QUrl::fromLocalFile( d.absolutePath() );
		}
	}


	dialog.setFileMode(QFileDialog::ExistingFile);

	dialog.setNameFilter(tr("FCS & SAV Files (*.sav *.SAV *.fc? *.FC?) ;; All files (*)"));

	dialog.setViewMode(QFileDialog::List);
	dialog.setFilter( QDir::AllEntries | QDir::AllDirs | QDir::Hidden );
	dialog.setLabelText( QFileDialog::Accept, tr("Load") );

	g_config->getOption ("SDL.LastLoadStateFrom", &last );

	getDirFromFile( last.c_str(), dir );

	dialog.setDirectory( tr(dir.c_str()) );

	// Check config option to use native file dialog or not
	g_config->getOption ("SDL.UseNativeFileDialog", &useNativeFileDialogVal);

	dialog.setOption(QFileDialog::DontUseNativeDialog, !useNativeFileDialogVal);
	dialog.setSidebarUrls(urls);

	ret = dialog.exec();

	if ( ret )
	{
		QStringList fileList;
		fileList = dialog.selectedFiles();

		if ( fileList.size() > 0 )
		{
			filename = fileList[0];
		}
	}

	if ( filename.isNull() )
   {
      return;
   }
	qDebug() << "selected file path : " << filename.toLocal8Bit();

	g_config->setOption ("SDL.LastLoadStateFrom", filename.toLocal8Bit().constData() );

	FCEU_WRAPPER_LOCK();
	FCEUI_LoadState( filename.toLocal8Bit().constData() );
	FCEU_WRAPPER_UNLOCK();
}

void consoleWin_t::saveStateAs(void)
{
	int ret, useNativeFileDialogVal;
	QString filename;
	std::string last;
	std::string dir;
	const char *base;
	QFileDialog  dialog(this, tr("Save State To File") );
	QList<QUrl> urls;
	QDir d;

	base = FCEUI_GetBaseDirectory();

	urls << QUrl::fromLocalFile( QDir::rootPath() );
	urls << QUrl::fromLocalFile(QStandardPaths::standardLocations(QStandardPaths::HomeLocation).first());
	urls << QUrl::fromLocalFile(QStandardPaths::standardLocations(QStandardPaths::DesktopLocation).first());
	urls << QUrl::fromLocalFile(QStandardPaths::standardLocations(QStandardPaths::DownloadLocation).first());

	if ( base )
	{
		urls << QUrl::fromLocalFile( QDir( base ).absolutePath() );

		d.setPath( QString(base) + "/fcs");

		if ( d.exists() )
		{
			urls << QUrl::fromLocalFile( d.absolutePath() );
		}

		d.setPath( QString(base) + "/sav");

		if ( d.exists() )
		{
			urls << QUrl::fromLocalFile( d.absolutePath() );
		}
	}

	dialog.setFileMode(QFileDialog::AnyFile);

	dialog.setNameFilter(tr("SAV Files (*.sav *.SAV) ;; All files (*)"));

	dialog.setViewMode(QFileDialog::List);
	dialog.setFilter( QDir::AllEntries | QDir::AllDirs | QDir::Hidden );
	dialog.setLabelText( QFileDialog::Accept, tr("Save") );
	dialog.setDefaultSuffix( tr(".sav") );

	g_config->getOption ("SDL.LastSaveStateAs", &last );

	if ( last.size() == 0 )
	{
		if ( base )
		{
			last = std::string(base) + "/sav";
		}
	}
	getDirFromFile( last.c_str(), dir );

	dialog.setDirectory( tr(dir.c_str()) );

	// Check config option to use native file dialog or not
	g_config->getOption ("SDL.UseNativeFileDialog", &useNativeFileDialogVal);

	dialog.setOption(QFileDialog::DontUseNativeDialog, !useNativeFileDialogVal);
	dialog.setSidebarUrls(urls);

	ret = dialog.exec();

	if ( ret )
	{
		QStringList fileList;
		fileList = dialog.selectedFiles();

		if ( fileList.size() > 0 )
		{
			filename = fileList[0];
		}
	}

	if ( filename.isNull() )
	{
	   return;
	}
	qDebug() << "selected file path : " << filename.toLocal8Bit();

	g_config->setOption ("SDL.LastSaveStateAs", filename.toLocal8Bit().constData() );

	FCEU_WRAPPER_LOCK();
	FCEUI_SaveState( filename.toLocal8Bit().constData() );
	FCEU_WRAPPER_UNLOCK();
}

void consoleWin_t::quickLoad(void)
{
	FCEU_WRAPPER_LOCK();
	FCEUI_LoadState( NULL );
	FCEU_WRAPPER_UNLOCK();
}

void consoleWin_t::loadState(int slot)
{
	int prevState;
	FCEU_WRAPPER_LOCK();
	prevState = FCEUI_SelectState( slot, false );
	FCEUI_LoadState( NULL, true );
	FCEUI_SelectState( prevState, false );
	FCEU_WRAPPER_UNLOCK();
}
void consoleWin_t::loadState0(void){ loadState(0); }
void consoleWin_t::loadState1(void){ loadState(1); }
void consoleWin_t::loadState2(void){ loadState(2); }
void consoleWin_t::loadState3(void){ loadState(3); }
void consoleWin_t::loadState4(void){ loadState(4); }
void consoleWin_t::loadState5(void){ loadState(5); }
void consoleWin_t::loadState6(void){ loadState(6); }
void consoleWin_t::loadState7(void){ loadState(7); }
void consoleWin_t::loadState8(void){ loadState(8); }
void consoleWin_t::loadState9(void){ loadState(9); }

void consoleWin_t::loadPrevState(void)
{
	FCEU_WRAPPER_LOCK();
	FCEU_StateRecorderLoadPrevState();
	FCEU_WRAPPER_UNLOCK();
}

void consoleWin_t::loadNextState(void)
{
	FCEU_WRAPPER_LOCK();
	FCEU_StateRecorderLoadNextState();
	FCEU_WRAPPER_UNLOCK();
}

void consoleWin_t::quickSave(void)
{
	FCEU_WRAPPER_LOCK();
	FCEUI_SaveState( NULL );
	FCEU_WRAPPER_UNLOCK();
}

void consoleWin_t::saveState(int slot)
{
	int prevState;
	FCEU_WRAPPER_LOCK();
	prevState = FCEUI_SelectState( slot, false );
	FCEUI_SaveState( NULL, true );
	FCEUI_SelectState( prevState, false );
	FCEU_WRAPPER_UNLOCK();
}
void consoleWin_t::saveState0(void){ saveState(0); }
void consoleWin_t::saveState1(void){ saveState(1); }
void consoleWin_t::saveState2(void){ saveState(2); }
void consoleWin_t::saveState3(void){ saveState(3); }
void consoleWin_t::saveState4(void){ saveState(4); }
void consoleWin_t::saveState5(void){ saveState(5); }
void consoleWin_t::saveState6(void){ saveState(6); }
void consoleWin_t::saveState7(void){ saveState(7); }
void consoleWin_t::saveState8(void){ saveState(8); }
void consoleWin_t::saveState9(void){ saveState(9); }

void consoleWin_t::changeState(int slot)
{
	FCEU_WRAPPER_LOCK();
	FCEUI_SelectState( slot, true );
	FCEU_WRAPPER_UNLOCK();
	state[slot]->setChecked(true);
}
void consoleWin_t::changeState0(void){ changeState(0); }
void consoleWin_t::changeState1(void){ changeState(1); }
void consoleWin_t::changeState2(void){ changeState(2); }
void consoleWin_t::changeState3(void){ changeState(3); }
void consoleWin_t::changeState4(void){ changeState(4); }
void consoleWin_t::changeState5(void){ changeState(5); }
void consoleWin_t::changeState6(void){ changeState(6); }
void consoleWin_t::changeState7(void){ changeState(7); }
void consoleWin_t::changeState8(void){ changeState(8); }
void consoleWin_t::changeState9(void){ changeState(9); }

void consoleWin_t::incrementState(void)
{
	FCEU_WRAPPER_LOCK();
	FCEUI_SelectStateNext(1);
	FCEU_WRAPPER_UNLOCK();
}

void consoleWin_t::decrementState(void)
{
	FCEU_WRAPPER_LOCK();
	FCEUI_SelectStateNext(-1);
	FCEU_WRAPPER_UNLOCK();
}

void consoleWin_t::mainMenuOpen(void)
{
	//printf("Main Menu Open\n");

	mainMenuEmuWasPaused = FCEUI_EmulationPaused() ? true : false;

	if ( mainMenuPauseWhenActv && !mainMenuEmuPauseSet && !mainMenuEmuWasPaused )
	{
		FCEUI_ToggleEmulationPause();
		mainMenuEmuPauseSet  = true;
	}
}

void consoleWin_t::mainMenuClose(void)
{
	//printf("Main Menu Close\n");

	if ( mainMenuEmuPauseSet )
	{
		bool isPaused = FCEUI_EmulationPaused() ? true : false;

		if ( isPaused != mainMenuEmuWasPaused )
		{
			FCEUI_ToggleEmulationPause();
		}
		mainMenuEmuPauseSet = false;
	}
}

void consoleWin_t::prepareScreenShot(void)
{
	// Set a timer single shot to take the screen shot. This gives time
	// for the GUI to remove the menu from view before taking the image.
	QTimer::singleShot( 100, Qt::CoarseTimer, this, SLOT(takeScreenShot(void)) );
}

//void consoleWin_t::takeScreenShot(void)
//{
//	FCEU_WRAPPER_LOCK();
//	FCEUI_SaveSnapshot();
//	FCEU_WRAPPER_UNLOCK();
//}

void consoleWin_t::takeScreenShot(void)
{
	int u=0;
	QPixmap  image;
	QScreen *screen = QGuiApplication::primaryScreen();

	if (const QWindow *window = windowHandle())
	{
		screen = window->screen();
	}

	if (screen == NULL)
	{
		FCEU_DispMessage("Error saving screen snapshot.",0);
		return;
	}

	FCEU_WRAPPER_LOCK();

	if ( viewport_GL )
	{
		image = screen->grabWindow( viewport_GL->winId() );
	}
	else if ( viewport_SDL )
	{
		image = screen->grabWindow( viewport_SDL->winId() );
	}
	else if ( viewport_QWidget )
	{
		image = screen->grabWindow( viewport_QWidget->winId() );
	}

	for (u = 0; u < 99999; ++u)
	{
		FILE *pp = FCEUD_UTF8fopen( FCEU_MakeFName(FCEUMKF_SNAP,u,"png").c_str(), "rb");

		if (pp == NULL)
		{
			break;
		}
		fclose(pp);
	}

	image.save( tr( FCEU_MakeFName(FCEUMKF_SNAP,u,"png").c_str() ), "png" );

	FCEU_WRAPPER_UNLOCK();

	FCEU_DispMessage("Screen snapshot %d saved.",0,u);
}

void consoleWin_t::loadLua(void)
{
#ifdef _S9XLUA_H
	LuaControlDialog_t *luaCtrlWin;

	//printf("Open Lua Control Window\n");
	
   luaCtrlWin = new LuaControlDialog_t(this);
	
   luaCtrlWin->show();
#endif
}

void consoleWin_t::loadJs(void)
{
#ifdef __FCEU_QSCRIPT_ENABLE__
	QScriptDialog_t *jsCtrlWin;

	//printf("Open JS Control Window\n");
	
   jsCtrlWin = new QScriptDialog_t(this);
	
   jsCtrlWin->show();
#endif
}

void consoleWin_t::openInputConfWin(void)
{
	//printf("Open Input Config Window\n");
	
	openInputConfWindow(this);
}

void consoleWin_t::openGamePadConfWin(void)
{
	//printf("Open GamePad Config Window\n");
	
	openGamePadConfWindow(this);
}

void consoleWin_t::openGameSndConfWin(void)
{
	ConsoleSndConfDialog_t *sndConfWin;

	//printf("Open Sound Config Window\n");
	
   sndConfWin = new ConsoleSndConfDialog_t(this);
	
   sndConfWin->show();
}

void consoleWin_t::openGameVideoConfWin(void)
{
	ConsoleVideoConfDialog_t *vidConfWin;

	//printf("Open Video Config Window\n");
	
   vidConfWin = new ConsoleVideoConfDialog_t(this);
	
   vidConfWin->show();
}

void consoleWin_t::openHotkeyConfWin(void)
{
	HotKeyConfDialog_t *hkConfWin;

	//printf("Open Hot Key Config Window\n");
	
   hkConfWin = new HotKeyConfDialog_t(this);
	
   hkConfWin->show();
}

void consoleWin_t::openPaletteConfWin(void)
{
	PaletteConfDialog_t *paletteConfWin;

	//printf("Open Palette Config Window\n");
	
   paletteConfWin = new PaletteConfDialog_t(this);
	
   paletteConfWin->show();
}

void consoleWin_t::openGuiConfWin(void)
{
	GuiConfDialog_t *guiConfWin;

	//printf("Open GUI Config Window\n");
	
   guiConfWin = new GuiConfDialog_t(this);
	
   guiConfWin->show();
}

void consoleWin_t::openTimingConfWin(void)
{
	TimingConfDialog_t *tmConfWin;

	//printf("Open Timing Config Window\n");
	
   tmConfWin = new TimingConfDialog_t(this);
	
   tmConfWin->show();
}

void consoleWin_t::openTimingStatWin(void)
{
	FrameTimingDialog_t *tmStatWin;

	//printf("Open Timing Statistics Window\n");
	
   tmStatWin = new FrameTimingDialog_t(this);
	
   tmStatWin->show();
}

void consoleWin_t::openPaletteEditorWin(void)
{
	PaletteEditorDialog_t *win;

	//printf("Open Palette Editor Window\n");
	
   win = new PaletteEditorDialog_t(this);
	
   win->show();
}

void consoleWin_t::openNetPlayHostWindow(void)
{
	//printf("Open NetPlay Host Window\n");
	
	openNetPlayHostDialog(this);
}

void consoleWin_t::openNetPlayJoinWindow(void)
{
	//printf("Open NetPlay Join Window\n");
	
	openNetPlayJoinDialog(this);
}

void consoleWin_t::openNetPlayStatusWindow(void)
{
	//printf("Open NetPlay Status Window\n");

	if (isNetPlayHost())
	{
		openNetPlayHostStatusDialog(this);
	}
	else
	{
		openNetPlayClientStatusDialog(this);
	}
}

void consoleWin_t::closeNetPlaySession(void)
{
	NetPlayCloseSession();
}

void consoleWin_t::openAviRiffViewer(void)
{
	AviRiffViewerDialog *win;

	//printf("Open AVI RIFF Viewer Window\n");
	
	win = new AviRiffViewerDialog(this);
	
	win->show();
}

void consoleWin_t::openTasEditor(void)
{
	FCEU_WRAPPER_LOCK();

	if ( tasWindowIsOpen() )
	{
		tasWindowSetFocus(true);
	}
	else if (FCEU_IsValidUI(FCEUI_TASEDITOR))
	{
		TasEditorWindow *win;

		win = new TasEditorWindow(this);
		
		win->show();

		connect(emulatorThread, SIGNAL(frameFinished(void)), win, SLOT(frameUpdate(void)) );
	}
	FCEU_WRAPPER_UNLOCK();
}

#ifdef __FCEU_REST_API_ENABLE__
void consoleWin_t::toggleRestApiServer(bool checked)
{
	if (!apiServer) {
		return;
	}
	
	if (checked) {
		// Load configuration and start server
		RestApiConfig apiConfig = loadRestApiConfig();
		apiServer->setConfig(apiConfig);
		
		// Start the server
		if (!apiServer->start()) {
			// If start failed, uncheck the menu item
			restApiAct->setChecked(false);
		}
	} else {
		// Stop the server
		apiServer->stop();
	}
	
	// Save the enabled state to configuration
	g_config->setOption("SDL.RestApiEnabled", checked);
	g_config->save();
}

void consoleWin_t::onRestApiServerStarted(void)
{
	RestApiConfig config = apiServer->getConfig();
	FCEU_DispMessage("REST API server started on %s:%d", 0, 
		config.bindAddress.toStdString().c_str(), config.port);
	
	// Update status bar
	if (this->statusBar()) {
		this->statusBar()->showMessage(
			QString("REST API: Running on %1:%2").arg(config.bindAddress).arg(config.port),
			5000  // Show for 5 seconds
		);
	}
}

void consoleWin_t::onRestApiServerStopped(void)
{
	FCEU_DispMessage("REST API server stopped", 0);
	
	// Update status bar
	if (this->statusBar()) {
		this->statusBar()->showMessage("REST API: Stopped", 3000);
	}
}

void consoleWin_t::onRestApiServerError(const QString& error)
{
	FCEU_DispMessage("REST API Error: %s", 1, error.toStdString().c_str());
	
	// Show error dialog
	QMessageBox::critical(this, tr("REST API Error"), 
		tr("Failed to start REST API server:\n%1").arg(error));
	
	// Uncheck the menu item if server failed to start
	if (restApiAct && restApiAct->isChecked()) {
		restApiAct->setChecked(false);
	}
}

RestApiConfig consoleWin_t::loadRestApiConfig(void)
{
	RestApiConfig apiConfig;
	int port = 8080;
	std::string bindAddr = "127.0.0.1";
	
	// Load from configuration
	g_config->getOption("SDL.RestApiPort", &port);
	g_config->getOption("SDL.RestApiBindAddress", &bindAddr);
	
	// Validate port range (1-65535)
	if (port < 1 || port > 65535) {
		FCEU_DispMessage("Invalid REST API port %d, using default 8080", 1, port);
		port = 8080;
	}
	
	apiConfig.port = port;
	apiConfig.bindAddress = QString::fromStdString(bindAddr);
	
	return apiConfig;
}
#endif

void consoleWin_t::openMovieOptWin(void)
{
	MovieOptionsDialog_t *win;

	//printf("Open Movie Options Window\n");
	
   win = new MovieOptionsDialog_t(this);
	
   win->show();
}

void consoleWin_t::openCheats(void)
{
	//printf("Open GUI Cheat Window\n");
	
   openCheatDialog(this);
}

void consoleWin_t::openRamWatch(void)
{
	RamWatchDialog_t *ramWatchWin;

	//printf("Open GUI RAM Watch Window\n");
	
   ramWatchWin = new RamWatchDialog_t(this);
	
   ramWatchWin->show();
}

void consoleWin_t::openRamSearch(void)
{
	//printf("Open GUI RAM Search Window\n");
	openRamSearchWindow(this);
}

void consoleWin_t::openDebugWindow(void)
{
	//printf("Open GUI 6502 Debugger Window\n");
	
	if ( debuggerWindowIsOpen() )
	{
		debuggerWindowSetFocus();
	}
	else
	{
		ConsoleDebugger *debugWin;

		debugWin = new ConsoleDebugger(this);
	
		debugWin->show();
	}
}

void consoleWin_t::openHexEditor(void)
{
	HexEditorDialog_t *hexEditWin;

	//printf("Open GUI Hex Editor Window\n");
	
   hexEditWin = new HexEditorDialog_t(this);
	
   hexEditWin->show();
}

void consoleWin_t::openPPUViewer(void)
{
	//printf("Open GUI PPU Viewer Window\n");
	
	openPPUViewWindow(this);
}

void consoleWin_t::openOAMViewer(void)
{
	//printf("Open GUI OAM Viewer Window\n");
	
	openOAMViewWindow(this);
}

void consoleWin_t::openNTViewer(void)
{
	//printf("Open GUI Name Table Viewer Window\n");
	
	openNameTableViewWindow(this);
}

void consoleWin_t::openCodeDataLogger(void)
{
	openCDLWindow(this);
}

void consoleWin_t::openGGEncoder(void)
{
	GameGenieDialog_t *win;

	//printf("Open Game Genie Window\n");
	
	win = new GameGenieDialog_t(this);
	
	win->show();
}

void consoleWin_t::openNesHeaderEditor(void)
{
	iNesHeaderEditor_t *win;

	//printf("Open NES Header Editor Window\n");
	
	win = new iNesHeaderEditor_t(this);
	
	if ( win->isInitialized() )
	{
		win->show();
	}
	else
	{
		delete win;
	}
}

void consoleWin_t::openTraceLogger(void)
{
	openTraceLoggerWindow(this);
}

void consoleWin_t::toggleAutoResume(void)
{
   //printf("Auto Resume: %i\n", autoResume->isChecked() );

	g_config->setOption ("SDL.AutoResume", (int) autoResume->isChecked() );

	AutoResumePlay = autoResume->isChecked();
}

void consoleWin_t::winResizeIx(int iscale)
{
	QSize w, v;
	double xscale = 1.0, yscale = 1.0, aspectRatio = 1.0;
	int texture_width  = nes_shm->video.ncol;
	int texture_height = nes_shm->video.nrow;
	int l=0, r=texture_width;
	int t=0, b=texture_height;
	int dw=0, dh=0, rw, rh;
	bool forceAspect = false;

	xscale = (double)iscale;
	yscale = (double)iscale;

	w = size();

	if ( viewport_Interface )
	{
		v = viewport_Interface->size();
		aspectRatio = viewport_Interface->getAspectRatio();
		forceAspect = viewport_Interface->getForceAspectOpt();
	}

	dw = w.width()  - v.width();
	dh = w.height() - v.height();

	if ( forceAspect )
	{
		xscale = xscale / nes_shm->video.xscale;
		yscale = xscale * (double)nes_shm->video.xyRatio;
	}
	else
	{
		xscale = xscale / nes_shm->video.xscale;
		yscale = yscale / nes_shm->video.yscale;
	}
	rw=(int)((r-l)*xscale);
	rh=(int)((b-t)*yscale);

	if ( forceAspect )
	{
		double rr;

		rr = (double)rh / (double)rw;

		if ( rr > aspectRatio )
		{
			rw = (int)( (((double)rh) / aspectRatio) + 0.50);
		}
		else
		{
			rh = (int)( (((double)rw) * aspectRatio) + 0.50);
		}
	}

	resize( rw + dw, rh + dh );
}

void consoleWin_t::toggleFullscreen(void)
{
	if ( isFullScreen() )
	{
		showNormal();

		if ( autoHideMenuFullscreen )
		{
			menubar->setVisible(true);
		}
	}
	else
	{
		if ( autoHideMenuFullscreen )
		{
			menubar->setVisible(false);
		}
		showFullScreen();
	}
}

void consoleWin_t::toggleFamKeyBrdEnable(void)
{
	toggleFamilyKeyboardFunc();
}

extern int globalCheatDisabled;

void consoleWin_t::toggleGlobalCheatEnable(void)
{
	FCEU_WRAPPER_LOCK();
	FCEUI_GlobalToggleCheat(globalCheatDisabled);
	FCEU_WRAPPER_UNLOCK();

	g_config->setOption("SDL.CheatsDisabled", globalCheatDisabled);
	g_config->save();

	updateCheatDialog();
}

void consoleWin_t::warnAmbiguousShortcut( QShortcut *shortcut)
{
	char stmp[256];
	std::string msg;
	int c = 0;

	snprintf( stmp, sizeof(stmp), "Error: Ambiguous Shortcut Activation for Key Sequence: '%s'\n", shortcut->key().toString().toLocal8Bit().constData() );

	msg.assign( stmp );

	for (int i = 0; i < HK_MAX; i++)
	{
		QShortcut *sc = Hotkeys[i].getShortcut();

		if ( sc == NULL )
		{
			continue;
		}

		if ( (sc == shortcut) || (shortcut->key().matches( sc->key() ) == QKeySequence::ExactMatch) )
		{
			if ( c == 0 )
			{
				msg.append("Hot Key Conflict: "); c++;
			}
			else
			{
				msg.append(" and "); c++;
			}
			msg.append( Hotkeys[i].getConfigName() );
		}
	}
	QueueErrorMsgWindow( msg.c_str() );
}

void consoleWin_t::powerConsoleCB(void)
{
	FCEU_WRAPPER_LOCK();
	FCEUI_PowerNES();
	FCEU_WRAPPER_UNLOCK();
   return;
}

void consoleWin_t::consoleHardReset(void)
{
	FCEU_WRAPPER_LOCK();
	fceuWrapperHardReset();
	FCEU_WRAPPER_UNLOCK();
   return;
}

void consoleWin_t::consoleSoftReset(void)
{
	FCEU_WRAPPER_LOCK();
	fceuWrapperSoftReset();
	FCEU_WRAPPER_UNLOCK();
   return;
}

void consoleWin_t::consolePause(void)
{
	FCEU_WRAPPER_LOCK();
	fceuWrapperTogglePause();
	FCEU_WRAPPER_UNLOCK();

	bool isPaused = FCEUI_EmulationPaused() ? true : false;
	emit pauseToggled( isPaused );

	mainMenuEmuPauseSet = false;
   return;
}

void consoleWin_t::setRegion(int region)
{
	int currentRegion;

	g_config->setOption ("SDL.PAL", region);
	g_config->save ();

	currentRegion = FCEUI_GetRegion();

	if ( currentRegion != region )
	{
		FCEU_WRAPPER_LOCK();
		FCEUI_SetRegion (region, true);
		FCEU_WRAPPER_UNLOCK();
	}
	return;
}

void consoleWin_t::setRegionNTSC(void)
{
	setRegion(0);
	return;
}

void consoleWin_t::setRegionPAL(void)
{
	setRegion(1);
	return;
}

void consoleWin_t::setRegionDendy(void)
{
	setRegion(2);
	return;
}

void consoleWin_t::setRamInit0(void)
{
	RAMInitOption = 0;

	g_config->setOption ("SDL.RamInitMethod", RAMInitOption);
	return;
}

void consoleWin_t::setRamInit1(void)
{
	RAMInitOption = 1;

	g_config->setOption ("SDL.RamInitMethod", RAMInitOption);
	return;
}

void consoleWin_t::setRamInit2(void)
{
	RAMInitOption = 2;

	g_config->setOption ("SDL.RamInitMethod", RAMInitOption);
	return;
}

void consoleWin_t::setRamInit3(void)
{
	RAMInitOption = 3;

	g_config->setOption ("SDL.RamInitMethod", RAMInitOption);
	return;
}

void consoleWin_t::toggleGameGenie(bool checked)
{
	int gg_enabled;

	FCEU_WRAPPER_LOCK();
	g_config->getOption ("SDL.GameGenie", &gg_enabled);
	g_config->setOption ("SDL.GameGenie", !gg_enabled);
	g_config->save ();
	FCEUI_SetGameGenie (gg_enabled);
	FCEU_WRAPPER_UNLOCK();
   return;
}

void consoleWin_t::loadGameGenieROM(void)
{
	int ret, useNativeFileDialogVal;
	QString filename;
	std::string last;
	std::string dir;
	QFileDialog  dialog(this, tr("Open Game Genie ROM") );
	QList<QUrl> urls;

	urls << QUrl::fromLocalFile( QDir::rootPath() );
	urls << QUrl::fromLocalFile(QStandardPaths::standardLocations(QStandardPaths::HomeLocation).first());
	urls << QUrl::fromLocalFile(QStandardPaths::standardLocations(QStandardPaths::DesktopLocation).first());
	urls << QUrl::fromLocalFile(QStandardPaths::standardLocations(QStandardPaths::DownloadLocation).first());

	dialog.setFileMode(QFileDialog::ExistingFile);

	dialog.setNameFilter(tr("GG ROM File (gg.rom  *Genie*.nes) ;; All files (*)"));

	dialog.setViewMode(QFileDialog::List);
	dialog.setFilter( QDir::AllEntries | QDir::AllDirs | QDir::Hidden );
	dialog.setLabelText( QFileDialog::Accept, tr("Load") );

	g_config->getOption ("SDL.LastOpenFile", &last );

	getDirFromFile( last.c_str(), dir );

	dialog.setDirectory( tr(dir.c_str()) );

	// Check config option to use native file dialog or not
	g_config->getOption ("SDL.UseNativeFileDialog", &useNativeFileDialogVal);

	dialog.setOption(QFileDialog::DontUseNativeDialog, !useNativeFileDialogVal);
	dialog.setSidebarUrls(urls);

	ret = dialog.exec();

	if ( ret )
	{
		QStringList fileList;
		fileList = dialog.selectedFiles();

		if ( fileList.size() > 0 )
		{
			filename = fileList[0];
		}
	}

	if ( filename.isNull() )
	{
	   return;
	}
	qDebug() << "selected file path : " << filename.toLocal8Bit();

	g_config->setOption ("SDL.LastOpenFile", filename.toLocal8Bit().constData() );

	// copy file to proper place (~/.fceux/gg.rom)
	std::ifstream f1 ( filename.toLocal8Bit().constData(), std::fstream::binary);
	std::string fn_out = FCEU_MakeFName (FCEUMKF_GGROM, 0, "");
	std::ofstream f2 (fn_out.c_str (),
	std::fstream::trunc | std::fstream::binary);
	f2 << f1.rdbuf ();

   return;
}

void consoleWin_t::openFamilyKeyboard(void)
{
	openFamilyKeyboardDialog(this);
	return;
}

void consoleWin_t::insertCoin(void)
{
	FCEU_WRAPPER_LOCK();
	FCEUI_VSUniCoin();
	FCEU_WRAPPER_UNLOCK();
   return;
}

void consoleWin_t::fdsSwitchDisk(void)
{
	FCEU_WRAPPER_LOCK();
	FCEU_FDSSelect();
	FCEU_WRAPPER_UNLOCK();
   return;
}

void consoleWin_t::fdsEjectDisk(void)
{
	FCEU_WRAPPER_LOCK();
	FCEU_FDSInsert();
	FCEU_WRAPPER_UNLOCK();
   return;
}

void consoleWin_t::fdsLoadBiosFile(void)
{
	int ret, useNativeFileDialogVal;
	QString filename;
	std::string last;
	std::string dir;
	QFileDialog  dialog(this, tr("Load FDS BIOS (disksys.rom)") );
	QList<QUrl> urls;

	urls << QUrl::fromLocalFile( QDir::rootPath() );
	urls << QUrl::fromLocalFile(QStandardPaths::standardLocations(QStandardPaths::HomeLocation).first());
	urls << QUrl::fromLocalFile(QStandardPaths::standardLocations(QStandardPaths::DesktopLocation).first());
	urls << QUrl::fromLocalFile(QStandardPaths::standardLocations(QStandardPaths::DownloadLocation).first());

	dialog.setFileMode(QFileDialog::ExistingFile);

	dialog.setNameFilter(tr("ROM files (*.rom *.ROM) ;; All files (*)"));

	dialog.setViewMode(QFileDialog::List);
	dialog.setFilter( QDir::AllEntries | QDir::AllDirs | QDir::Hidden );
	dialog.setLabelText( QFileDialog::Accept, tr("Load") );

	g_config->getOption ("SDL.LastOpenFile", &last );

	getDirFromFile( last.c_str(), dir);

	dialog.setDirectory( tr(dir.c_str()) );

	// Check config option to use native file dialog or not
	g_config->getOption ("SDL.UseNativeFileDialog", &useNativeFileDialogVal);

	dialog.setOption(QFileDialog::DontUseNativeDialog, !useNativeFileDialogVal);
	dialog.setSidebarUrls(urls);

	ret = dialog.exec();

	if ( ret )
	{
		QStringList fileList;
		fileList = dialog.selectedFiles();

		if ( fileList.size() > 0 )
		{
			filename = fileList[0];
		}
	}

	if ( filename.isNull() )
	{
	   return;
	}
	qDebug() << "selected file path : " << filename.toLocal8Bit();

	// copy BIOS file to proper place (~/.fceux/disksys.rom)
	std::ifstream fdsBios (filename.toLocal8Bit().constData(), std::fstream::binary);
	std::string output_filename =
		FCEU_MakeFName (FCEUMKF_FDSROM, 0, "");
	std::ofstream outFile (output_filename.c_str (),
			       std::fstream::trunc | std::fstream::
			       binary);
	outFile << fdsBios.rdbuf ();
	if (outFile.fail ())
	{
		FCEUD_PrintError ("Error copying the FDS BIOS file.");
	}
	else
	{
		printf("Famicom Disk System BIOS loaded.  If you are you having issues, make sure your BIOS file is 8KB in size.\n");
	}

   return;
}

void consoleWin_t::emuSpeedUp(void)
{
   IncreaseEmulationSpeed();
}

void consoleWin_t::emuSlowDown(void)
{
   DecreaseEmulationSpeed();
}

void consoleWin_t::emuSlowestSpd(void)
{
   FCEUD_SetEmulationSpeed( EMUSPEED_SLOWEST );
}

void consoleWin_t::emuNormalSpd(void)
{
   FCEUD_SetEmulationSpeed( EMUSPEED_NORMAL );
}

void consoleWin_t::emuFastestSpd(void)
{
   FCEUD_SetEmulationSpeed( EMUSPEED_FASTEST );
}

void consoleWin_t::emuCustomSpd(void)
{
	int ret;
	QInputDialog dialog(this);

	dialog.setWindowTitle( tr("Emulation Speed") );
	dialog.setLabelText( tr("Enter a percentage from 1 to 1000.") );
	dialog.setOkButtonText( tr("Ok") );
	dialog.setInputMode( QInputDialog::IntInput );
	dialog.setIntRange( 1, 1000 );
	dialog.setIntValue( 100 );
	
	ret = dialog.exec();
	
	if ( QDialog::Accepted == ret )
	{
	   int spdPercent;
	
	   spdPercent = dialog.intValue();
	
	   CustomEmulationSpeed( spdPercent );
	}
}

void consoleWin_t::emuSetFrameAdvDelay(void)
{
	int ret;
	QInputDialog dialog(this);

	dialog.setWindowTitle( tr("Frame Advance Delay") );
	dialog.setLabelText( tr("How much time should elapse before holding the frame advance unpauses the simulation?") );
	dialog.setOkButtonText( tr("Ok") );
	dialog.setInputMode( QInputDialog::IntInput );
	dialog.setIntRange( 0, 1000 );
	dialog.setIntValue( frameAdvance_Delay );
	
	ret = dialog.exec();
	
	if ( QDialog::Accepted == ret )
	{
	   frameAdvance_Delay = dialog.intValue();

	   g_config->setOption("SDL.FrameAdvanceDelay", frameAdvance_Delay );
	   g_config->save();
	}
}

void consoleWin_t::syncAutoFirePatternMenu(void)
{
	int on, off;

	GetAutoFirePattern( &on, &off );

	for (size_t i=0; i<afActList.size(); i++)
	{
		if ( afActList[i]->isMatch( on, off ) )
		{
			afActList[i]->setChecked(true);
			return;
		}
	}

	// If we get here, then the custom option is selected.
	afActCustom->setChecked(true);

}
void consoleWin_t::setCustomAutoFire(void)
{
	int ret, autoFireOnFrames, autoFireOffFrames;
	QDialog dialog(this);
	QLabel *lbl;
	QGridLayout *grid;
	QVBoxLayout *vbox;
	QSpinBox *onBox, *offBox;
	QPushButton *okButton, *cancelButton;

	autoFireOnFrames  = afActCustom->getOnValue();
	autoFireOffFrames = afActCustom->getOffValue();

	dialog.setWindowTitle( tr("Custom AutoFire Pattern") );

	 onBox = new QSpinBox();
	offBox = new QSpinBox();

	 onBox->setMinimum( 1);
	offBox->setMinimum( 1);
	 onBox->setMaximum(30);
	offBox->setMaximum(30);

	 onBox->setValue( autoFireOnFrames  );
	offBox->setValue( autoFireOffFrames );

	vbox = new QVBoxLayout();
	grid = new QGridLayout();

	lbl = new QLabel( tr("# ON Frames") );

	grid->addWidget( lbl, 0, 0 );

	lbl = new QLabel( tr("# OFF Frames") );

	grid->addWidget( lbl, 1, 0 );

	grid->addWidget( onBox , 0, 1 );
	grid->addWidget( offBox, 1, 1 );

	    okButton = new QPushButton( tr("Ok") );
	cancelButton = new QPushButton( tr("Cancel") );

	    okButton->setIcon( style()->standardIcon( QStyle::SP_DialogApplyButton  ) );
	cancelButton->setIcon( style()->standardIcon( QStyle::SP_DialogCancelButton ) );

	grid->addWidget( cancelButton , 2, 0 );
	grid->addWidget(     okButton , 2, 1 );

	vbox->addLayout( grid );

	dialog.setLayout( vbox );
	
	connect( cancelButton, SIGNAL(clicked(void)), &dialog, SLOT(reject(void)) );
	connect(     okButton, SIGNAL(clicked(void)), &dialog, SLOT(accept(void)) );

	okButton->setDefault(true);

	ret = dialog.exec();
	
	if ( QDialog::Accepted == ret )
	{
		autoFireOnFrames  =  onBox->value();
		autoFireOffFrames = offBox->value();

		afActCustom->setPattern( autoFireOnFrames, autoFireOffFrames );

		if ( afActCustom->isChecked() )
		{
			afActCustom->activateCB();
		}
		g_config->setOption("SDL.AutofireCustomOnFrames"  , autoFireOnFrames );
		g_config->setOption("SDL.AutofireCustomOffFrames" , autoFireOffFrames);
		g_config->save();
	}
}

void consoleWin_t::muteSoundVolume(void)
{
	FCEU_WRAPPER_LOCK();
	FCEUD_SoundToggle();
	FCEU_WRAPPER_UNLOCK();
}

void consoleWin_t::incrSoundVolume(void)
{
	FCEU_WRAPPER_LOCK();
	FCEUD_SoundVolumeAdjust( 1);
	FCEU_WRAPPER_UNLOCK();
}

void consoleWin_t::decrSoundVolume(void)
{
	FCEU_WRAPPER_LOCK();
	FCEUD_SoundVolumeAdjust(-1);
	FCEU_WRAPPER_UNLOCK();
}

void consoleWin_t::toggleLagCounterDisplay(void)
{
	FCEU_WRAPPER_LOCK();
	lagCounterDisplay = !lagCounterDisplay;
	g_config->setOption("SDL.ShowLagCount", lagCounterDisplay);
	FCEU_WRAPPER_UNLOCK();
}

void consoleWin_t::toggleFrameAdvLagSkip(void)
{
	FCEU_WRAPPER_LOCK();
	frameAdvanceLagSkip = !frameAdvanceLagSkip;
	FCEUI_DispMessage ("Skipping lag in Frame Advance %sabled.", 0, frameAdvanceLagSkip ? "en" : "dis");
	FCEU_WRAPPER_UNLOCK();
}

void consoleWin_t::toggleMovieBindSaveState(void)
{
	FCEU_WRAPPER_LOCK();
	bindSavestate = !bindSavestate;
	g_config->setOption("SDL.MovieBindSavestate", bindSavestate);
	FCEUI_DispMessage ("Savestate binding to movie %sabled.", 0, bindSavestate ? "en" : "dis");
	FCEU_WRAPPER_UNLOCK();
}

void consoleWin_t::toggleMovieFrameDisplay(void)
{
	extern int frame_display;
	FCEU_WRAPPER_LOCK();
	FCEUI_MovieToggleFrameDisplay();
	g_config->setOption("SDL.ShowFrameCount", frame_display );
	FCEU_WRAPPER_UNLOCK();
}

void consoleWin_t::toggleMovieReadWrite(void)
{
	FCEU_WRAPPER_LOCK();
	//FCEUI_SetMovieToggleReadOnly (!FCEUI_GetMovieToggleReadOnly ());
	FCEUI_MovieToggleReadOnly();

	if ( tasWin != NULL )
	{
		tasWin->updateRecordStatus();
	}
	FCEU_WRAPPER_UNLOCK();
}

void consoleWin_t::toggleInputDisplay(void)
{
	FCEU_WRAPPER_LOCK();
	FCEUI_ToggleInputDisplay();
	g_config->setOption ("SDL.InputDisplay", input_display);
	FCEU_WRAPPER_UNLOCK();
}

void consoleWin_t::toggleBackground(void)
{
	bool fgOn, bgOn;
	FCEU_WRAPPER_LOCK();
	FCEUI_GetRenderPlanes( fgOn,  bgOn );
	FCEUI_SetRenderPlanes( fgOn, !bgOn );
	FCEU_WRAPPER_UNLOCK();
}

void consoleWin_t::toggleForeground(void)
{
	bool fgOn, bgOn;
	FCEU_WRAPPER_LOCK();
	FCEUI_GetRenderPlanes(  fgOn, bgOn );
	FCEUI_SetRenderPlanes( !fgOn, bgOn );
	FCEU_WRAPPER_UNLOCK();
}

void consoleWin_t::toggleTurboMode(void)
{
	NoWaiting ^= 1;
}

void consoleWin_t::openStateRecorderConfWin(void)
{
	StateRecorderDialog_t *win;

	win = new StateRecorderDialog_t(this);

	win->show();
}

void consoleWin_t::openMovie(void)
{
	MoviePlayDialog_t *win;

	win = new MoviePlayDialog_t(this);

	win->show();
}

void consoleWin_t::playMovieFromBeginning(void)
{
	FCEU_WRAPPER_LOCK();
	FCEUI_MoviePlayFromBeginning();
	FCEU_WRAPPER_UNLOCK();
}

void consoleWin_t::stopMovie(void)
{
	FCEU_WRAPPER_LOCK();
	FCEUI_StopMovie();
	FCEU_WRAPPER_UNLOCK();
   return;
}

void consoleWin_t::recordMovie(void)
{
	FCEU_WRAPPER_LOCK();
	if (fceuWrapperGameLoaded())
	{
		MovieRecordDialog_t dialog(this);
		dialog.exec();
	}
	FCEU_WRAPPER_UNLOCK();
	return;
}

void consoleWin_t::aviRecordStart(void)
{
	if ( !aviRecordRunning() )
	{
		FCEU_WRAPPER_LOCK();
		if ( aviRecordOpenFile(NULL) == 0 )
		{
			aviDiskThread->start();
		}
		FCEU_WRAPPER_UNLOCK();
	}
}

void consoleWin_t::aviRecordAsStart(void)
{
	if ( aviRecordRunning() )
	{
		return;
	}
	std::string last;
	int ret, useNativeFileDialogVal;
	QString filename;
	std::string lastPath;
	//char dir[512];
	const char *base, *rom;
	QFileDialog  dialog(this, tr("Save AVI Movie for Recording") );
	QList<QUrl> urls;
	QDir d;

	dialog.setFileMode(QFileDialog::AnyFile);

	dialog.setNameFilter(tr("AVI Movies (*.avi) ;; All files (*)"));

	dialog.setViewMode(QFileDialog::List);
	dialog.setFilter( QDir::AllEntries | QDir::AllDirs | QDir::Hidden );
	dialog.setLabelText( QFileDialog::Accept, tr("Save") );

	base = FCEUI_GetBaseDirectory();

	urls << QUrl::fromLocalFile( QDir::rootPath() );
	urls << QUrl::fromLocalFile(QStandardPaths::standardLocations(QStandardPaths::HomeLocation).first());
	urls << QUrl::fromLocalFile(QStandardPaths::standardLocations(QStandardPaths::DesktopLocation).first());
	urls << QUrl::fromLocalFile(QStandardPaths::standardLocations(QStandardPaths::DownloadLocation).first());

	if ( base )
	{
		urls << QUrl::fromLocalFile( QDir( base ).absolutePath() );

		d.setPath( QString(base) + "/avi");

		if ( d.exists() )
		{
			urls << QUrl::fromLocalFile( d.absolutePath() );
		}

		dialog.setDirectory( d.absolutePath() );
	}
	dialog.setDefaultSuffix( tr(".avi") );

	g_config->getOption ("SDL.AviFilePath", &lastPath);
	if ( lastPath.size() > 0 )
	{
		dialog.setDirectory( QString::fromStdString(lastPath) );
	}

	rom = getRomFile();

	if ( rom )
	{
		char baseName[512];
		getFileBaseName( rom, baseName );

		if ( baseName[0] != 0 )
		{
			dialog.selectFile(baseName);
		}
	}

	// Check config option to use native file dialog or not
	g_config->getOption ("SDL.UseNativeFileDialog", &useNativeFileDialogVal);

	dialog.setOption(QFileDialog::DontUseNativeDialog, !useNativeFileDialogVal);
	dialog.setSidebarUrls(urls);

	ret = dialog.exec();

	if ( ret )
	{
		QStringList fileList;
		fileList = dialog.selectedFiles();

		if ( fileList.size() > 0 )
		{
			filename = fileList[0];
		}
	}

	if ( filename.isNull() )
	{
	   return;
	}
	qDebug() << "selected file path : " << filename.toLocal8Bit();

	FCEUI_printf ("AVI Recording movie to %s\n", filename.toLocal8Bit().constData() );

	lastPath = QFileInfo(filename).absolutePath().toLocal8Bit().constData();

	if ( lastPath.size() > 0 )
	{
		g_config->setOption ("SDL.AviFilePath", lastPath);
	}

	FCEU_WRAPPER_LOCK();
	if ( aviRecordOpenFile( filename.toLocal8Bit().constData() ) == 0 )
	{
		aviDiskThread->start();
	}
	FCEU_WRAPPER_UNLOCK();
}

void consoleWin_t::aviRecordStop(void)
{
	if ( aviRecordRunning() )
	{
		QGuiApplication::setOverrideCursor( QCursor(Qt::BusyCursor) );
		FCEU_WRAPPER_LOCK();
		aviDiskThread->requestInterruption();
		aviDiskThread->quit();
		aviDiskThread->wait(10000);
		FCEU_WRAPPER_UNLOCK();
		QGuiApplication::restoreOverrideCursor();
	}
}

void consoleWin_t::aviAudioEnableChange(bool checked)
{
	aviSetAudioEnable( checked );

	return;
}

void consoleWin_t::setAviHudEnable(bool checked)
{
	FCEUI_SetAviEnableHUDrecording( checked );

	g_config->setOption("SDL.RecordHUD", checked );
}

void consoleWin_t::setAviMsgEnable(bool checked)
{
	FCEUI_SetAviDisableMovieMessages( !checked );

	g_config->setOption("SDL.MovieMsg", checked );
}

void consoleWin_t::aviVideoFormatChanged(int idx)
{
	aviSetSelVideoFormat(idx);
}

void consoleWin_t::wavRecordStart(void)
{
	if ( !FCEUI_WaveRecordRunning() )
	{
		const char *romFile;
		std::string fileName;

		romFile = getRomFile();

		if ( romFile )
		{
			char base[512];
			const char *baseDir = FCEUI_GetBaseDirectory();
			std::string lastPath;

			getFileBaseName( romFile, base );

			g_config->getOption ("SDL.WavFilePath", &lastPath);

			if ( lastPath.size() > 0 )
			{
				fileName.assign( lastPath );
				fileName.append( "/" );
			}
			else if ( baseDir )
			{
				fileName.assign( baseDir );
				fileName.append( "/wav/" );
			}
			else
			{
				fileName.clear();
			}
			fileName.append( base );
			fileName.append(".wav");
			//printf("WAV Filepath:'%s'\n", fileName );
		}
		else
		{
			return;
		}
		FCEU_WRAPPER_LOCK();
		FCEUI_BeginWaveRecord( fileName.c_str() );
		FCEU_WRAPPER_UNLOCK();
	}
}

void consoleWin_t::wavRecordAsStart(void)
{
	if ( FCEUI_WaveRecordRunning() )
	{
		return;
	}
	int ret, useNativeFileDialogVal;
	QString filename;
	std::string lastPath;
	const char *base, *rom;
	QFileDialog  dialog(this, tr("Save WAV Movie for Recording") );
	QList<QUrl> urls;
	QDir d;

	dialog.setFileMode(QFileDialog::AnyFile);

	dialog.setNameFilter(tr("WAV Movies (*.wav) ;; All files (*)"));

	dialog.setViewMode(QFileDialog::List);
	dialog.setFilter( QDir::AllEntries | QDir::AllDirs | QDir::Hidden );
	dialog.setLabelText( QFileDialog::Accept, tr("Save") );

	base = FCEUI_GetBaseDirectory();

	urls << QUrl::fromLocalFile( QDir::rootPath() );
	urls << QUrl::fromLocalFile(QStandardPaths::standardLocations(QStandardPaths::HomeLocation).first());
	urls << QUrl::fromLocalFile(QStandardPaths::standardLocations(QStandardPaths::DesktopLocation).first());
	urls << QUrl::fromLocalFile(QStandardPaths::standardLocations(QStandardPaths::DownloadLocation).first());

	if ( base )
	{
		urls << QUrl::fromLocalFile( QDir( base ).absolutePath() );

		d.setPath( QString(base) + "/wav");

		if ( d.exists() )
		{
			urls << QUrl::fromLocalFile( d.absolutePath() );
		}

		dialog.setDirectory( d.absolutePath() );
	}
	dialog.setDefaultSuffix( tr(".wav") );

	g_config->getOption ("SDL.WavFilePath", &lastPath);
	if ( lastPath.size() > 0 )
	{
		dialog.setDirectory( QString::fromStdString(lastPath) );
	}

	rom = getRomFile();

	if ( rom )
	{
		char baseName[512];
		getFileBaseName( rom, baseName );

		if ( baseName[0] != 0 )
		{
			dialog.selectFile(baseName);
		}
	}

	// Check config option to use native file dialog or not
	g_config->getOption ("SDL.UseNativeFileDialog", &useNativeFileDialogVal);

	dialog.setOption(QFileDialog::DontUseNativeDialog, !useNativeFileDialogVal);
	dialog.setSidebarUrls(urls);

	ret = dialog.exec();

	if ( ret )
	{
		QStringList fileList;
		fileList = dialog.selectedFiles();

		if ( fileList.size() > 0 )
		{
			filename = fileList[0];
		}
	}

	if ( filename.isNull() )
	{
	   return;
	}
	qDebug() << "selected file path : " << filename.toLocal8Bit();

	FCEUI_printf ("WAV Recording movie to %s\n", filename.toLocal8Bit().constData() );

	lastPath = QFileInfo(filename).absolutePath().toLocal8Bit().constData();

	if ( lastPath.size() > 0 )
	{
		g_config->setOption ("SDL.WavFilePath", lastPath);
	}

	FCEU_WRAPPER_LOCK();
	FCEUI_BeginWaveRecord( filename.toLocal8Bit().constData() );
	FCEU_WRAPPER_UNLOCK();
}

void consoleWin_t::wavRecordStop(void)
{
	if ( FCEUI_WaveRecordRunning() )
	{
		FCEU_WRAPPER_LOCK();
		FCEUI_EndWaveRecord();
		FCEU_WRAPPER_UNLOCK();
	}
}

void consoleWin_t::aboutFCEUX(void)
{
	AboutWindow *aboutWin;

	//printf("About FCEUX Window\n");
	
	aboutWin = new AboutWindow(this);
	
	aboutWin->show();
	return;
}

void consoleWin_t::aboutQt(void)
{
	//printf("About Qt Window\n");
	
	QMessageBox::aboutQt(this);

	//printf("About Qt Destroyed\n");
	return;
}

void consoleWin_t::openMsgLogWin(void)
{
	//printf("Open Message Log Window\n");
	MsgLogViewDialog_t *msgLogWin;
	
	msgLogWin = new MsgLogViewDialog_t(this);

	msgLogWin->show();

	return;
}

void consoleWin_t::openOnlineDocs(void)
{
	if ( QDesktopServices::openUrl( QUrl("https://fceux.com/web/help/fceux.html") ) == false )
	{
		QueueErrorMsgWindow("Error: Failed to open link to: https://fceux.com/web/help/fceux.html");
	}
	return;
}

void consoleWin_t::openOfflineDocs(void)
{
	OpenHelpWindow();
	return;
}

#if defined(__linux__) || defined(__APPLE__) || defined(__unix__)
int consoleWin_t::setNicePriority( int value )
{
	int ret = 0;
#if defined(__linux__) || defined(__unix__)

	if ( value < -20 )
	{
		value = -20;
	}
	else if ( value > 19 )
	{
		value =  19;
	}

	if ( ::setpriority( PRIO_PROCESS, getpid(), value ) )
	{
		perror("Emulator thread setpriority error: ");
		ret = -1;
	}
#elif defined(__APPLE__)

	if ( value < -20 )
	{
		value = -20;
	}
	else if ( value > 20 )
	{
		value =  20;
	}

	if ( ::setpriority( PRIO_PROCESS, getpid(), value ) )
	{
		perror("Emulator thread setpriority error: ");
		ret = -1;
	}
#endif
	return ret;
}

int consoleWin_t::getNicePriority(void)
{
	return ::getpriority( PRIO_PROCESS, getpid() );
}

int consoleWin_t::getMinSchedPriority(void)
{
	int policy, prio;

	if ( getSchedParam( policy, prio ) )
	{
		return 0;
	}
	return sched_get_priority_min( policy );
}

int consoleWin_t::getMaxSchedPriority(void)
{
	int policy, prio;

	if ( getSchedParam( policy, prio ) )
	{
		return 0;
	}
	return sched_get_priority_max( policy );
}

int consoleWin_t::getSchedParam( int &policy, int &priority )
{
	int ret = 0;

#if defined(__linux__) || defined(__unix__) && !defined(__OpenBSD__)
	struct sched_param  p;

	policy = sched_getscheduler( getpid() );

	if ( sched_getparam( getpid(), &p ) )
	{
		perror("GUI thread sched_getparam error: ");
		ret = -1;
		priority = 0;
	}
	else
	{
		priority = p.sched_priority;
	}

#elif defined(__APPLE__)
	struct sched_param  p;

	if ( pthread_getschedparam( pthread_self(), &policy, &p ) )
	{
		perror("GUI thread pthread_getschedparam error: ");
		ret = -1;
		priority = 0;
	}
	else
	{
		priority = p.sched_priority;
	}
#endif
	return ret;
}

int consoleWin_t::setSchedParam( int policy, int priority )
{
	int ret = 0;
#if defined(__linux__) || defined(__unix__) && !defined(__OpenBSD__)
	struct sched_param  p;
	int minPrio, maxPrio;

	minPrio = sched_get_priority_min( policy );
	maxPrio = sched_get_priority_max( policy );

	if ( priority < minPrio )
	{
		priority = minPrio;
	}
	else if ( priority > maxPrio )
	{
		priority = maxPrio;
	}
	p.sched_priority = priority;

	if ( sched_setscheduler( getpid(), policy, &p ) )
	{
		perror("GUI thread sched_setscheduler error");
		ret = -1;
	}
#elif defined(__APPLE__)
	struct sched_param  p;
	int minPrio, maxPrio;

	minPrio = sched_get_priority_min( policy );
	maxPrio = sched_get_priority_max( policy );

	if ( priority < minPrio )
	{
		priority = minPrio;
	}
	else if ( priority > maxPrio )
	{
		priority = maxPrio;
	}
	p.sched_priority = priority;

	if ( ::pthread_setschedparam( pthread_self(), policy, &p ) != 0 )
	{
		perror("GUI thread pthread_setschedparam error: ");
	}
#endif
	return ret;
}
#endif

void consoleWin_t::syncActionConfig( QAction *act, const char *property )
{
	if ( act->isCheckable() )
	{
		int enable;
		g_config->getOption ( property, &enable);

		act->setChecked( enable ? true : false );
	}
}

void consoleWin_t::loadMostRecentROM(void)
{
	if ( romList.size() <= 0 )
	{
		return;
	}
	FCEU_WRAPPER_LOCK();
	CloseGame ();
	LoadGame ( (romList.back())->c_str() );
	FCEU_WRAPPER_UNLOCK();
}

QString consoleWin_t::getTempDir()
{
	QString path;
	if (tempDir->isValid())
	{
		path = tempDir->path();
	}
	else
	{
		path = QDir::tempPath();
	}
	return path;
}

int consoleWin_t::getPeriodicInterval(void)
{
	return gameTimer->interval();
}

void consoleWin_t::transferVideoBuffer(bool allowRedraw)
{
	FCEU_PROFILE_FUNC(prof, "VideoXfer");

	{
		FCEU::autoScopedLock lock(videoBufferMutex);
		if ( nes_shm->blitUpdated )
		{
			nes_shm->blitUpdated = 0;

			if (viewport_Interface != nullptr)
			{
				viewport_Interface->transfer2LocalBuffer();
				redrawVideoRequest = true;
			}
		}
	}

	// Don't queue redraw in mutex lock scope
	if (allowRedraw && redrawVideoRequest && (viewport_Interface != nullptr))
	{
		viewport_Interface->queueRedraw();
		redrawVideoRequest = false;
	}
}

void consoleWin_t::emuFrameFinish(void)
{
	//static bool eventProcessingInProg = false;

	guiSignalRecvMark();

	//if ( eventProcessingInProg )
	//{   // Prevent recursion as processEvents function can double back on us
	//	return;
	//}
	// Prevent recursion as processEvents function can double back on us
	//if ( !eventProcessingInProg )
	//{
	//	eventProcessingInProg = true;
	//	// Process all events before attempting to render viewport
	//	QCoreApplication::processEvents();
	//	eventProcessingInProg = false;
	//}

	// Update Input Devices
	//FCEUD_UpdateInput();
	
	//printf("EMU Frame Finish\n");

//#ifdef __FCEU_QSCRIPT_ENABLE__
//	QtScriptManager::getInstance()->frameFinishedUpdate();
//#endif

	transferVideoBuffer(false);
}

void consoleWin_t::updatePeriodic(void)
{
	FCEU_PROFILE_FUNC(prof, "updatePeriodic");
	static bool eventProcessingInProg = false;

	//if ( eventProcessingInProg )
	//{   // Prevent recursion as processEvents function can double back on us
	//	return;
	//}
	// Prevent recursion as processEvents function can double back on us
	if ( !eventProcessingInProg )
	{
		eventProcessingInProg = true;
		// Process all events before attempting to render viewport
		QCoreApplication::processEvents();
		eventProcessingInProg = false;
	}

	// Update Input Devices
	FCEUD_UpdateInput();
	
	// RePaint Game Viewport
	transferVideoBuffer(true);

	// Low Rate Updates
	if ( (updateCounter % 30) == 0 )
	{
		// Keep region menu selection sync'd to actual state
		int actRegion = FCEUI_GetRegion();

		if ( !region[ actRegion ]->isChecked() )
		{
			region[ actRegion ]->setChecked(true);
		}

		powerAct->setEnabled( FCEU_IsValidUI( FCEUI_POWER ) );
		resetAct->setEnabled( FCEU_IsValidUI( FCEUI_RESET ) );
		sresetAct->setEnabled( FCEU_IsValidUI( FCEUI_RESET ) );
		playMovBeginAct->setEnabled( FCEU_IsValidUI( FCEUI_PLAYFROMBEGINNING ) );
		insCoinAct->setEnabled( FCEU_IsValidUI( FCEUI_INSERT_COIN ) );
		fdsSwitchAct->setEnabled( FCEU_IsValidUI( FCEUI_SWITCH_DISK ) );
		fdsEjectAct->setEnabled( FCEU_IsValidUI( FCEUI_EJECT_DISK ) );
		stopMovAct->setEnabled( FCEU_IsValidUI( FCEUI_STOPMOVIE ) );
		recentRomMenu->setEnabled( !recentRomMenu->isEmpty() );
		quickLoadAct->setEnabled( FCEU_IsValidUI( FCEUI_QUICKLOAD ) );
		quickSaveAct->setEnabled( FCEU_IsValidUI( FCEUI_QUICKSAVE ) );
		loadStateAct->setEnabled( FCEU_IsValidUI( FCEUI_LOADSTATE ) );
		saveStateAct->setEnabled( FCEU_IsValidUI( FCEUI_SAVESTATE ) );
		openMovAct->setEnabled( FCEU_IsValidUI( FCEUI_PLAYMOVIE ) );
		recMovAct->setEnabled( FCEU_IsValidUI( FCEUI_RECORDMOVIE ) );
		recAviAct->setEnabled( FCEU_IsValidUI( FCEUI_RECORDMOVIE ) && !FCEU_IsValidUI( FCEUI_STOPAVI ) );
		recAsAviAct->setEnabled( FCEU_IsValidUI( FCEUI_RECORDMOVIE ) && !FCEU_IsValidUI( FCEUI_STOPAVI ) );
		stopAviAct->setEnabled( FCEU_IsValidUI( FCEUI_STOPAVI ) );
		recWavAct->setEnabled( FCEU_IsValidUI( FCEUI_RECORDMOVIE ) && !FCEUI_WaveRecordRunning() );
		recAsWavAct->setEnabled( FCEU_IsValidUI( FCEUI_RECORDMOVIE ) && !FCEUI_WaveRecordRunning() );
		stopWavAct->setEnabled( FCEUI_WaveRecordRunning() );
		tasEditorAct->setEnabled( FCEU_IsValidUI(FCEUI_TASEDITOR) );
	}

	if ( errorMsgValid )
	{
		showErrorMsgWindow();
		errorMsgValid = false;
	}

	if ( recentRomMenuReset )
	{
		FCEU_WRAPPER_LOCK();
		buildRecentRomMenu();
		recentRomMenuReset = false;
		FCEU_WRAPPER_UNLOCK();
	}

	if ( closeRequested )
	{
		closeApp();
		closeRequested = false;
	}

	NetPlayPeriodicUpdate();

	updateCounter++;

#ifdef __FCEU_PROFILER_ENABLE__
		FCEU_profiler_log_thread_activity();
#endif
   return;
}

void consoleWin_t::onNetPlayChange(void)
{
	const bool netPlayactv = NetPlayActive();

	netPlayHostAct->setEnabled( !netPlayactv );
	netPlayJoinAct->setEnabled( !netPlayactv );
	netPlayDiscAct->setEnabled(  netPlayactv );

	if (netPlayactv)
	{
		const bool isHost = isNetPlayHost();
		netPlayHostStatAct->setEnabled(isHost);
		netPlayHostStatAct->setVisible(isHost);
		netPlayClientStatAct->setEnabled(!isHost);
		netPlayClientStatAct->setVisible(!isHost);
	}
	else
	{
		netPlayHostStatAct->setEnabled(false);
		netPlayHostStatAct->setVisible(false);
		netPlayClientStatAct->setEnabled(false);
		netPlayClientStatAct->setVisible(false);
	}
}

emulatorThread_t::emulatorThread_t( QObject *parent )
	: QThread(parent)
{
	#if defined(__linux__) || defined(__APPLE__) || defined(__unix__)
	pself = 0;
	#endif

	setObjectName( QString("EmulationThread") );
}

#if defined(__linux__) 
#ifndef SYS_gettid
#error "SYS_gettid unavailable on this system"
#endif

#define gettid() ((pid_t)syscall(SYS_gettid))
#endif


void emulatorThread_t::init(void)
{
	int opt;

	#if defined(__linux__) || defined(__APPLE__) || defined(__unix__)
	if ( pthread_self() == (pthread_t)QThread::currentThreadId() )
	{
		pself = pthread_self();
		//printf("EMU is using PThread: %p\n", (void*)pself);
	}
	#endif

	#if defined(__linux__)
	pid = gettid();
	#elif defined(__APPLE__) || defined(__unix__)
	pid = getpid();
	#endif

	g_config->getOption( "SDL.SetSchedParam", &opt );

	if ( opt )
	{
		#ifndef WIN32
		int policy, prio, nice;

		g_config->getOption( "SDL.EmuSchedPolicy", &policy );
		g_config->getOption( "SDL.EmuSchedPrioRt", &prio   );
		g_config->getOption( "SDL.EmuSchedNice"  , &nice   );

		setNicePriority( nice );

		setSchedParam( policy, prio );
		#endif
	}
}

void emulatorThread_t::setPriority( QThread::Priority priority_req )
{
	//printf("New Priority: %i \n", priority_req );
	//printf("Old Priority: %i \n", priority() );

	QThread::setPriority( priority_req );

	//printf("Set Priority: %i \n", priority() );
}

#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
int emulatorThread_t::setNicePriority( int value )
{
	int ret = 0;
#if defined(__linux__) || defined(__unix__)

	if ( value < -20 )
	{
		value = -20;
	}
	else if ( value > 19 )
	{
		value =  19;
	}

	if ( ::setpriority( PRIO_PROCESS, pid, value ) )
	{
		perror("Emulator thread setpriority error: ");
		ret = -1;
	}
#elif defined(__APPLE__)

	if ( value < -20 )
	{
		value = -20;
	}
	else if ( value > 20 )
	{
		value =  20;
	}

	if ( ::setpriority( PRIO_PROCESS, pid, value ) )
	{
		perror("Emulator thread setpriority error: ");
		ret = -1;
	}
#endif
	return ret;
}

int emulatorThread_t::getNicePriority(void)
{
	return ::getpriority( PRIO_PROCESS, pid );
}

int emulatorThread_t::getMinSchedPriority(void)
{
	int policy, prio;

	if ( getSchedParam( policy, prio ) )
	{
		return 0;
	}
	return sched_get_priority_min( policy );
}

int emulatorThread_t::getMaxSchedPriority(void)
{
	int policy, prio;

	if ( getSchedParam( policy, prio ) )
	{
		return 0;
	}
	return sched_get_priority_max( policy );
}

int emulatorThread_t::getSchedParam( int &policy, int &priority )
{
	struct sched_param  p;

	if ( pthread_getschedparam( pself, &policy, &p ) )
	{
		perror("Emulator thread pthread_getschedparam error: ");
		return -1;
	}
	priority = p.sched_priority;

	return 0;
}

int emulatorThread_t::setSchedParam( int policy, int priority )
{
	int ret = 0;
#if defined(__linux__) || defined(__unix__)
	struct sched_param  p;
	int minPrio, maxPrio;

	minPrio = sched_get_priority_min( policy );
	maxPrio = sched_get_priority_max( policy );

	if ( priority < minPrio )
	{
		priority = minPrio;
	}
	else if ( priority > maxPrio )
	{
		priority = maxPrio;
	}
	p.sched_priority = priority;

	if ( ::pthread_setschedparam( pself, policy, &p ) != 0 )
	{
		perror("Emulator thread pthread_setschedparam error: ");
		ret = -1;
	}

#elif defined(__APPLE__)
	struct sched_param  p;
	int minPrio, maxPrio;

	minPrio = sched_get_priority_min( policy );
	maxPrio = sched_get_priority_max( policy );

	if ( priority < minPrio )
	{
		priority = minPrio;
	}
	else if ( priority > maxPrio )
	{
		priority = maxPrio;
	}
	p.sched_priority = priority;

	if ( ::pthread_setschedparam( pself, policy, &p ) != 0 )
	{
		perror("Emulator thread pthread_setschedparam error: ");
	}
#endif
	return ret;
}
#endif

void emulatorThread_t::run(void)
{
	printf("Emulator Start\n");
	nes_shm->runEmulator = 1;

	init();

	while ( nes_shm->runEmulator )
	{
		fceuWrapperUpdate();
	}
	printf("Emulator Exit\n");
	emit finished();
}

void emulatorThread_t::signalFrameFinished(void)
{
	emuSignalSendMark();
	emit frameFinished();
}

void emulatorThread_t::signalRomLoad( const char *path )
{
	emit loadRomRequest( QString(path) );
}

//-----------------------------------------------------------------------------
// Custom QMenuBar for Console
//-----------------------------------------------------------------------------
consoleMenuBar::consoleMenuBar(QWidget *parent)
	: QMenuBar(parent)
{

}
consoleMenuBar::~consoleMenuBar(void)
{

}

void consoleMenuBar::keyPressEvent(QKeyEvent *event)
{
	QMenuBar::keyPressEvent(event);

	pushKeyEvent( event, 1 );

	// Force de-focus of menu bar when escape key is pressed.
	// This prevents the menubar from hi-jacking keyboard input focus
	// when using menu accelerators
	if ( event->key() == Qt::Key_Escape )
	{
		((QWidget*)parent())->setFocus();
	}
	event->accept();
}

void consoleMenuBar::keyReleaseEvent(QKeyEvent *event)
{
	QMenuBar::keyReleaseEvent(event);

	pushKeyEvent( event, 0 );

	event->accept();
}
//-----------------------------------------------------------------------------
//----------------------------------------------------------------------------
consoleRecentRomAction::consoleRecentRomAction(QString desc, QWidget *parent)
	: QAction( desc, parent )
{
	QString txt;
	QFileInfo fi(desc);

	path = desc.toLocal8Bit().constData();

	txt  = fi.fileName();
	txt += QString("\t");
	txt += desc;

	setText( txt );
}
//----------------------------------------------------------------------------
consoleRecentRomAction::~consoleRecentRomAction(void)
{
	//printf("Recent ROM Menu Action Deleted\n");
}
//----------------------------------------------------------------------------
void consoleRecentRomAction::activateCB(void)
{
	printf("Activate Recent ROM: %s \n", path.c_str() );

	FCEU_WRAPPER_LOCK();
	CloseGame ();
	LoadGame ( path.c_str() );
	FCEU_WRAPPER_UNLOCK();
}
//-----------------------------------------------------------------------------
autoFireMenuAction::autoFireMenuAction(int on, int off, QString name, QWidget *parent)
	: QAction( name, parent)
{
	onFrames = on;  offFrames = off;
}
//-----------------------------------------------------------------------------
autoFireMenuAction::~autoFireMenuAction(void)
{

}
//-----------------------------------------------------------------------------
void autoFireMenuAction::activateCB(void)
{
	g_config->setOption("SDL.AutofireOnFrames"  , onFrames );
	g_config->setOption("SDL.AutofireOffFrames" , offFrames);
	g_config->save();

	SetAutoFirePattern( onFrames, offFrames );
}
//-----------------------------------------------------------------------------
bool autoFireMenuAction::isMatch( int on, int off )
{
	return ( (on == onFrames) && (off == offFrames) );
}
//-----------------------------------------------------------------------------
void autoFireMenuAction::setPattern(int on, int off)
{
	onFrames = on;  offFrames = off;
}
//-----------------------------------------------------------------------------
