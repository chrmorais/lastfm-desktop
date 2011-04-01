/*
   Copyright 2005-2009 Last.fm Ltd. 
      - Primarily authored by Max Howell, Jono Cole and Doug Mansell

   This file is part of the Last.fm Desktop Application Suite.

   lastfm-desktop is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   lastfm-desktop is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with lastfm-desktop.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "UnicornMainWindow.h"
#include "UnicornApplication.h"
#include "UnicornCoreApplication.h"
#include "dialogs/AboutDialog.h"
#include "dialogs/UpdateDialog.h"
#include "UnicornSettings.h"
#include <lastfm/User>
#include <QDesktopServices>
#include <QDesktopWidget>
#include <QMenuBar>
#include <QShortcut>
#include <QMouseEvent>

#ifdef Q_OS_WIN32
#include <Objbase.h>
#endif

#define SETTINGS_POSITION_KEY "MainWindowPosition"


unicorn::MainWindow::MainWindow()
:QMainWindow()
{
    new QShortcut( QKeySequence(Qt::CTRL+Qt::Key_W), this, SLOT(close()) );
    new QShortcut( QKeySequence(Qt::ALT+Qt::SHIFT+Qt::Key_L), this, SLOT(openLog()) );
    connect( qApp, SIGNAL(gotUserInfo( const lastfm::UserDetails& )), SLOT(onGotUserInfo( const lastfm::UserDetails& )) );
    connect( qApp, SIGNAL(sessionChanged( unicorn::Session* ) ),
             SLOT( onSessionChanged( unicorn::Session* ) ) );
    connect( qApp->desktop(), SIGNAL( resized(int)), SLOT( cleverlyPosition()));
#ifdef Q_OS_WIN32
    taskBarCreatedMessage = RegisterWindowMessage(L"TaskbarButtonCreated");
#endif
}


unicorn::MainWindow::~MainWindow()
{
}


void
unicorn::MainWindow::finishUi()
{
    base_ui.account = menuBar()->addMenu( User().name() );
    base_ui.profile = base_ui.account->addAction( tr("Visit &Profile"), this, SLOT(visitProfile()) );
    base_ui.account->addSeparator();
    QAction* quit = base_ui.account->addAction( tr("&Quit"), qApp, SLOT(quit()) );
    quit->setMenuRole( QAction::QuitRole );
#ifdef Q_OS_WIN
    quit->setShortcut( Qt::ALT + Qt::Key_F4 );
#else
    quit->setShortcut( Qt::CTRL + Qt::Key_Q );
#endif

    menuBar()->insertMenu( menuBar()->actions().first(), base_ui.account );
    QMenu* help = menuBar()->addMenu( tr("Help") );
    QAction* about = help->addAction( tr("About"), this, SLOT(about()) );
    QAction* c4u = help->addAction( tr("Check for Updates"), this, SLOT(checkForUpdates()) );

#ifndef NDEBUG
    QMenu* debug = menuBar()->addMenu( tr("Debug") );
    QAction* rss = debug->addAction( tr("Refresh Stylesheet"), qApp, SLOT(refreshStyleSheet()) );
    rss->setShortcut( Qt::CTRL + Qt::Key_R );
#endif

#ifdef Q_OS_MAC
    about->setMenuRole( QAction::AboutRole );
    c4u->setMenuRole( QAction::ApplicationSpecificRole );
#endif

    base_ui.update = new UpdateDialog( this );


    cleverlyPosition();
}


void
unicorn::MainWindow::refreshStyleSheet()
{
    static_cast<unicorn::Application*>(qApp)->refreshStyleSheet();
}

#ifdef Q_OS_WIN32
bool      
unicorn::MainWindow::winEvent(MSG* message, long* result)
{
    if ( message->message == taskBarCreatedMessage)
    {


        HRESULT hr = CoCreateInstance(CLSID_TaskbarList,
                                      0,
                                      CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS( &taskbar));
        if (hr == S_OK)
        {
            m_thumbButtonActions.clear();
            addWinThumbBarButtons( m_thumbButtonActions );

            Q_ASSERT_X( m_thumbButtonActions.count() <= 7, "winEvent", "More than 7 thumb buttons" );

            // make sure we update the thumb buttons everytime they change state
            foreach ( QAction* button, m_thumbButtonActions )
            {
                connect( button, SIGNAL(changed()), SLOT(updateThumbButtons()));
                connect( button, SIGNAL(toggled(bool)), SLOT(updateThumbButtons()));
            }

            if ( m_thumbButtonActions.count() > 0 )
                updateThumbButtons();

            *result = hr;
            return true;
        }
    }
    else if ( message->message == WM_COMMAND )
        if ( HIWORD(message->wParam) == THBN_CLICKED )
            m_thumbButtonActions[LOWORD(message->wParam)]->trigger();

    return false;
}


void
unicorn::MainWindow::updateThumbButtons()
{
    THUMBBUTTON buttons[7];

    for ( int i(0) ; i < m_thumbButtonActions.count() ; ++i )
    {
        buttons[i].dwMask = THB_ICON|THB_TOOLTIP|THB_FLAGS;
        buttons[i].iId = i;
        buttons[i].hIcon = m_thumbButtonActions[i]->isChecked() ?
            m_thumbButtonActions[i]->icon().pixmap( QSize(16, 16), QIcon::Normal, QIcon::On ).toWinHICON():
            m_thumbButtonActions[i]->icon().pixmap( QSize(16, 16), QIcon::Normal, QIcon::Off ).toWinHICON();
        wcscpy(buttons[i].szTip, m_thumbButtonActions[i]->text().utf16());

        buttons[i].dwFlags = m_thumbButtonActions[i]->isEnabled() ? THBF_ENABLED : THBF_DISABLED;
    }

    if (sender())
        taskbar->ThumbBarUpdateButtons(winId(), m_thumbButtonActions.count(), buttons);
    else
        taskbar->ThumbBarAddButtons(winId(), m_thumbButtonActions.count(), buttons);
}
#endif // Q_OS_WIN32


void
unicorn::MainWindow::onGotUserInfo( const lastfm::UserDetails& details )
{
    base_ui.account->setTitle( details );
    QString const text = details.getInfoString();
    if (text.size() && base_ui.account) {
        QAction* a = base_ui.account->addAction( text );
        a->setEnabled( false );
        a->setObjectName( "UserBlurb" );
        base_ui.account->insertAction( base_ui.profile, a );
    }
}


void
unicorn::MainWindow::visitProfile()
{
    QDesktopServices::openUrl( User().www() );
}


void
unicorn::MainWindow::about()
{
    if (!base_ui.about) base_ui.about = new AboutDialog( this );
    base_ui.about.show();
}


void
unicorn::MainWindow::checkForUpdates()
{
    if (!base_ui.update) base_ui.update = new UpdateDialog( this );
    base_ui.update.show();
}


void
unicorn::MainWindow::openLog()
{
    QDesktopServices::openUrl( QUrl::fromLocalFile( unicorn::CoreApplication::log().absoluteFilePath() ) );    
}



bool 
unicorn::MainWindow::eventFilter( QObject* o, QEvent* event )
{
#ifdef SUPER_MEGA_DEBUG
	qDebug() << o << event;
#endif
	
    QWidget* obj = qobject_cast<QWidget*>( o );
    if (!obj)
        return false;
    
    QMouseEvent* e = static_cast<QMouseEvent*>( event );
    
    switch ((int)e->type())
    {
        case QEvent::MouseButtonPress:
            m_dragHandleMouseDownPos[ obj ] = e->globalPos() - pos();
            return false;

        case QEvent::MouseButtonRelease:
            m_dragHandleMouseDownPos[ obj ] = QPoint();
            return false;
            
        case QEvent::MouseMove:
            if (m_dragHandleMouseDownPos.contains( obj ) && !m_dragHandleMouseDownPos[ obj ].isNull()) {
                move( e->globalPos() - m_dragHandleMouseDownPos[ obj ] );
                return true;
            }
    }

    return QMainWindow::eventFilter(o, event);
}


void 
unicorn::MainWindow::addDragHandleWidget( QWidget* w )
{
    w->installEventFilter( this );
}


void 
unicorn::MainWindow::hideEvent( QHideEvent* )
{
    emit hidden( false );
}


void 
unicorn::MainWindow::showEvent( QShowEvent* )
{
    emit shown( true );
}


void
unicorn::MainWindow::onSessionChanged( Session* session )
{
    base_ui.account->findChild<QAction*>("UserBlurb")->deleteLater();
    base_ui.account->setTitle( session->userInfo().name() );
}

void 
unicorn::MainWindow::storeGeometry() const
{
    AppSettings s;
    s.beginGroup( metaObject()->className());
        s.setValue( "geometry", frameGeometry());
    s.endGroup();
}


void 
unicorn::MainWindow::moveEvent( QMoveEvent* )
{
    storeGeometry();
}


void
unicorn::MainWindow::resizeEvent( QResizeEvent* )
{
    storeGeometry();
}


void
unicorn::MainWindow::cleverlyPosition()
{
    AppSettings s;
    s.beginGroup( metaObject()->className());
        QRect geo = s.value( "geometry", QRect()).toRect();
    s.endGroup();

    if( !geo.isValid())
        return;

    move( geo.topLeft());
    resize( geo.size());
    
    int screenNum = qApp->desktop()->screenNumber( this );
    QRect screenRect = qApp->desktop()->availableGeometry( screenNum );
    if( !screenRect.contains( frameGeometry(), true)) {
        QRect diff;

        diff = screenRect.intersected( frameGeometry() );

        if (diff.left() == screenRect.left() )
            move( diff.left(), pos().y());
        if( diff.right() == screenRect.right())
            move( diff.right() - width(), pos().y());
        if( diff.top() == screenRect.top())
            move( pos().x(), diff.top());
        if( diff.bottom() == screenRect.bottom())
            move( pos().x(), diff.bottom() - height());
    }
}
