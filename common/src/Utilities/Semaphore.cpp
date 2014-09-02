/*  PCSX2 - PS2 Emulator for PCs
*  Copyright (C) 2002-2010  PCSX2 Dev Team
*
*  PCSX2 is free software: you can redistribute it and/or modify it under the terms
*  of the GNU Lesser General Public License as published by the Free Software Found-
*  ation, either version 3 of the License, or (at your option) any later version.
*
*  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
*  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
*  PURPOSE.  See the GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License along with PCSX2.
*  If not, see <http://www.gnu.org/licenses/>.
*/


#include "PrecompiledHeader.h"

#include "Threading.h"
#include "wxBaseTools.h"
#include "wxGuiTools.h"
#include "ThreadingInternal.h"

// --------------------------------------------------------------------------------------
//  Semaphore Implementations
// --------------------------------------------------------------------------------------

Threading::Semaphore::Semaphore()
{
	m_sema = nullptr;
	Reset();
}

Threading::Semaphore::~Semaphore() throw()
{
	Release();
}

void Threading::Semaphore::Release() {
	if (m_sema) {
		//TODO OSX this doesn't work as excepted
	#ifdef __APPLE__
		//dispatch_release(m_sema);
	#else
		sem_destroy( &m_sema );
	#endif
		m_sema = nullptr;
	}
}

void Threading::Semaphore::Reset()
{
	Release();
#ifdef __APPLE__
	m_sema = dispatch_semaphore_create(Semaphore::INITIAL_VALUE);
#else
	sem_init( &m_sema, false, Semaphore::INITIAL_VALUE );
#endif
}

void Threading::Semaphore::Post()
{
#ifdef __APPLE__
	if (m_sema)dispatch_semaphore_signal(m_sema);
#else
	sem_post( &m_sema );
#endif
}

void Threading::Semaphore::Post( int multiple )
{
#if defined(_MSC_VER)
	sem_post_multiple( &m_sema, multiple );
#else
	// Only w32pthreads has the post_multiple, but it's easy enough to fake:
	while( multiple > 0 )
	{
		multiple--;
		Post();
	}
#endif
}

void Threading::Semaphore::WaitWithoutYield()
{
	pxAssertMsg( !wxThread::IsMain(), "Unyielding semaphore wait issued from the main/gui thread.  Please use Wait() instead." );
	SEMWAIT( m_sema );
}

#ifdef __APPLE__
bool Threading::Semaphore::WaitWithoutYield( const wxTimeSpan& timeout2 )
{
	wxTimeSpan timeout = timeout2;
	const wxTimeSpan substr = wxTimeSpan::Milliseconds(1);
	int ret=3;

	while ( timeout.IsPositive() ) {
		ret = dispatch_semaphore_wait(m_sema, DISPATCH_TIME_NOW);

		if (ret==0)
			break;
		timeout-=substr;
		Sleep(substr.GetMilliseconds().GetLo());
	}
	return ret==0;
}
#else
bool Threading::Semaphore::WaitWithoutYield( const wxTimeSpan& timeout )
{
	wxDateTime megafail( wxDateTime::UNow() + timeout );
	const timespec fail = { megafail.GetTicks(), megafail.GetMillisecond() * 1000000 };
	return sem_timedwait( &m_sema, &fail ) == 0;
}
#endif


// This is a wxApp-safe implementation of Wait, which makes sure and executes the App's
// pending messages *if* the Wait is performed on the Main/GUI thread.  This ensures that
// user input continues to be handled and that windoes continue to repaint.  If the Wait is
// called from another thread, no message pumping is performed.
//
void Threading::Semaphore::Wait()
{
#if wxUSE_GUI
	if( !wxThread::IsMain() || (wxTheApp == NULL) )
	{
		SEMWAIT( m_sema );
	}
	else if( _WaitGui_RecursionGuard( L"Semaphore::Wait" ) )
	{
		ScopedBusyCursor hourglass( Cursor_ReallyBusy );
		SEMWAIT( m_sema );
	}
	else
	{
		//ScopedBusyCursor hourglass( Cursor_KindaBusy );
		while( !WaitWithoutYield( def_yieldgui_interval ) )
			YieldToMain();
	}
#else
	SEMWAIT( m_sema );
#endif
}

// This is a wxApp-safe implementation of WaitWithoutYield, which makes sure and executes the App's
// pending messages *if* the Wait is performed on the Main/GUI thread.  This ensures that
// user input continues to be handled and that windows continue to repaint.  If the Wait is
// called from another thread, no message pumping is performed.
//
// Returns:
//   false if the wait timed out before the semaphore was signaled, or true if the signal was
//   reached prior to timeout.
//
bool Threading::Semaphore::Wait( const wxTimeSpan& timeout )
{
#if wxUSE_GUI
	if( !wxThread::IsMain() || (wxTheApp == NULL) )
	{
		return WaitWithoutYield( timeout );
	}
	else if( _WaitGui_RecursionGuard( L"Semaphore::TimedWait" ) )
	{
		ScopedBusyCursor hourglass( Cursor_ReallyBusy );
		return WaitWithoutYield( timeout );
	}
	else
	{
		//ScopedBusyCursor hourglass( Cursor_KindaBusy );
		wxTimeSpan countdown( (timeout) );

		do {
			if( WaitWithoutYield( def_yieldgui_interval ) ) break;
			YieldToMain();
			countdown -= def_yieldgui_interval;
		} while( countdown.GetMilliseconds() > 0 );

		return countdown.GetMilliseconds() > 0;
	}
#else
	return WaitWithoutYield( timeout );
#endif
}

// Performs an uncancellable wait on a semaphore; restoring the thread's previous cancel state
// after the wait has completed.  Useful for situations where the semaphore itself is stored on
// the stack and passed to another thread via GUI message or such, avoiding complications where
// the thread might be canceled and the stack value becomes invalid.
//
// Performance note: this function has quite a bit more overhead compared to Semaphore::WaitWithoutYield(), so
// consider manually specifying the thread as uncancellable and using WaitWithoutYield() instead if you need
// to do a lot of no-cancel waits in a tight loop worker thread, for example.
void Threading::Semaphore::WaitNoCancel()
{
	int oldstate;
	pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &oldstate );
	//WaitWithoutYield();
	Wait();
	pthread_setcancelstate( oldstate, NULL );
}

void Threading::Semaphore::WaitNoCancel( const wxTimeSpan& timeout )
{
	int oldstate;
	pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &oldstate );
	//WaitWithoutYield( timeout );
	Wait( timeout );
	pthread_setcancelstate( oldstate, NULL );
}

int Threading::Semaphore::Count()
{
	int retval=Semaphore::INITIAL_VALUE;
#ifndef __APPLE__ //// TODO OSX dunno if dispatch needs to implement this
	sem_getvalue( &m_sema, &retval );
#endif
	return retval;
}
