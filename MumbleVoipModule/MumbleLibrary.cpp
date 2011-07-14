// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "MumbleLibrary.h"
#include "MumbleMainLoopThread.h"
#define BUILDING_DLL // for dll import/export declarations
#define CreateEvent  CreateEventW // for \boost\asio\detail\win_event.hpp and \boost\asio\detail\win_iocp_handle_service.hpp
#include <mumbleclient/client_lib.h>
#include <mumbleclient/logging.h>
#undef BUILDING_DLL // for dll import/export declarations

#include "MemoryLeakCheck.h"

namespace MumbleLib
{
    QString MumbleLibrary::reason_ = "";
    MumbleMainLoopThread* MumbleLibrary::mumble_main_loop_ = 0;

    void MumbleLibrary::Start()
    {
        StartMumbleThread();
        //emit Started();
    }

    void MumbleLibrary::Stop()
    {
        StopMumbleThread();
        //emit Stoped();
    }

    bool MumbleLibrary::IsRunning()
    {
        if (!mumble_main_loop_)
            return false;
        return mumble_main_loop_->isRunning();
    }

    QString MumbleLibrary::Reason()
    {
        return reason_;
    }

    void MumbleLibrary::StartMumbleThread()
    {

        if (!mumble_main_loop_)
        {
            mumble_main_loop_ = new MumbleMainLoopThread();
        }

        if (mumble_main_loop_->isRunning())
            return;

        //! @todo
        //connect(lib_mumble_thread_, SIGNAL(finished()), SLOT(MumbleThreadFinished()) );
        //connect(lib_mumble_thread_, SIGNAL(terminated()), SLOT(MumbleThreadFinished()) );
                
        mumble_main_loop_->start();
        mumble_main_loop_->setPriority(QThread::LowPriority);
    }

    void MumbleLibrary::StopMumbleThread()
    {
        if (!mumble_main_loop_)
            return;

        MumbleClient::MumbleClientLib* mumble_lib = MumbleClient::MumbleClientLib::instance();
        if (!mumble_lib)
            return;

        mumble_lib->Shutdown();
        mumble_main_loop_->wait();
        SAFE_DELETE(mumble_main_loop_);
    }

    QThread* MumbleLibrary::MainLoopThread()
    {
        return mumble_main_loop_;
    }

} // MumbleLib
