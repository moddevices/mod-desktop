/*
*/

#include "JackAudioDriver.h"
#include "JackDriverLoader.h"
#include "JackEngineControl.h"
#include "JackLockedEngine.h"
#include "JackMidiPort.h"
#include "JackTools.h"

#ifdef _WIN32
#else
# include <fcntl.h>
# include <sys/mman.h>
# ifdef __APPLE__
#  include <mach/mach.h>
#  include <mach/semaphore.h>
#  include <servers/bootstrap.h>
# else
#  include <cerrno>
#  include <syscall.h>
#  include <sys/time.h>
#  include <linux/futex.h>
# endif
#endif

namespace Jack
{

// -----------------------------------------------------------------------------------------------------------

class DesktopAudioDriver : public JackAudioDriver
{
    struct Data {
        uint32_t magic;
        int32_t padding1;
       #if defined(__APPLE__)
        char bootname1[32];
        char bootname2[32];
       #elif defined(_WIN32)
        HANDLE sem1;
        HANDLE sem2;
       #else
        int32_t sem1;
        int32_t sem2;
       #endif
        uint16_t midiEventCount;
        uint16_t midiFrames[511];
        uint8_t midiData[511 * 4];
        uint8_t padding2[4];
        float audio[];
    };

    static constexpr const size_t kDataSize = sizeof(Data) + sizeof(float) * 128 * 2;

    Data* fShmData;

   #ifdef _WIN32
    HANDLE fShm;
   #else
    int fShmFd;
   #endif

   #if defined(__APPLE__)
    mach_port_t task = MACH_PORT_NULL;
    semaphore_t sem1 = MACH_PORT_NULL;
    semaphore_t sem2 = MACH_PORT_NULL;

    void post()
    {
        semaphore_signal(sem2);
    }

    bool wait()
    {
        const mach_timespec timeout = { 1, 0 };
        return semaphore_timedwait(sem1, timeout) == KERN_SUCCESS;
    }
   #elif defined(_WIN32)
    void post()
    {
        ReleaseSemaphore(fShmData->sem2, 1, nullptr);
    }

    bool wait()
    {
        return WaitForSingleObject(fShmData->sem1, 1000) == WAIT_OBJECT_0;
    }
   #else
    void post()
    {
        const bool unlocked = __sync_bool_compare_and_swap(&fShmData->sem2, 0, 1);
        if (! unlocked)
            return;
        ::syscall(__NR_futex, &fShmData->sem2, FUTEX_WAKE, 1, nullptr, nullptr, 0);
    }

    bool wait()
    {
        const timespec timeout = { 1, 0 };

        for (;;)
        {
            if (__sync_bool_compare_and_swap(&fShmData->sem1, 1, 0))
                return true;

            if (::syscall(__NR_futex, &fShmData->sem1, FUTEX_WAIT, 0, &timeout, nullptr, 0) != 0)
                if (errno != EAGAIN && errno != EINTR)
                    return false;
        }
    }
   #endif

    jack_native_thread_t fProcessThread;
    int fCaptureMidiPort;
    int fPlaybackMidiPort;
    bool fIsProcessing, fIsRunning;

    static void* on_process(void* const arg)
    {
        static_cast<DesktopAudioDriver*>(arg)->_process();
        return nullptr;
    }

    void _process()
    {
        if (set_threaded_log_function())
        {
        }

        while (fIsProcessing && fIsRunning)
        {
            if (! wait())
                continue;

            if (fShmData->magic == 7331)
            {
                fIsProcessing = false;
                post();
                return;
            }

            CycleTakeBeginTime();

            if (Process() != 0)
                fIsProcessing = false;

            post();
        }
    }

public:
    DesktopAudioDriver(const char* name, const char* alias, JackLockedEngine* engine, JackSynchro* table)
        : JackAudioDriver(name, alias, engine, table),
          fShmData(nullptr),
         #ifdef _WIN32
          fShm(nullptr),
         #else
          fShmFd(-1),
         #endif
          fCaptureMidiPort(0),
          fPlaybackMidiPort(0),
          fIsProcessing(false),
          fIsRunning(false)
    {
        printf("%03d:%s\n", __LINE__, __FUNCTION__);
        fflush(stdout);
    }

    ~DesktopAudioDriver() override
    {
        printf("%03d:%s\n", __LINE__, __FUNCTION__);
        fflush(stdout);
    }

    int Open(jack_nframes_t buffersize,
             jack_nframes_t samplerate,
             bool capturing,
             bool playing,
             int chan_in,
             int chan_out,
             bool monitor,
             const char* capture_driver_name,
             const char* playback_driver_name,
             jack_nframes_t capture_latency,
             jack_nframes_t playback_latency) override
    {
        printf("%03d:%s\n", __LINE__, __FUNCTION__);
        fflush(stdout);
        if (JackAudioDriver::Open(buffersize, samplerate, capturing, playing, chan_in, chan_out, monitor,
            capture_driver_name, playback_driver_name, capture_latency, playback_latency) != 0) {
            return -1;
        }

        void* ptr;

      #ifdef _WIN32
        fShm = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, "/mod-desktop-test1");
        if (fShm == nullptr)
        {
            Close();
            jack_error("Can't open default MOD Desktop driver 1");
            return -1;
        }

        ptr = MapViewOfFile(fShm, FILE_MAP_ALL_ACCESS, 0, 0, kDataSize);
        if (ptr == nullptr)
        {
            Close();
            jack_error("Can't open default MOD Desktop driver 2");
            return -1;
        }

        VirtualLock(ptr, kDataSize);
      #else
        fShmFd = shm_open("/mod-desktop-test1", O_RDWR, 0);
        if (fShmFd < 0)
        {
            Close();
            jack_error("Can't open default MOD Desktop driver 1");
            return -1;
        }

       #ifdef MAP_LOCKED
        ptr = mmap(nullptr, kDataSize, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_LOCKED, fShmFd, 0);
        if (ptr == nullptr || ptr == MAP_FAILED)
       #endif
        {
            ptr = mmap(nullptr, kDataSize, PROT_READ|PROT_WRITE, MAP_SHARED, fShmFd, 0);
        }

        if (ptr == nullptr || ptr == MAP_FAILED)
        {
            Close();
            jack_error("Can't open default MOD Desktop driver 2");
            return -1;
        }

       #ifndef MAP_LOCKED
        mlock(ptr, kDataSize);
       #endif
      #endif

        fShmData = static_cast<Data*>(ptr);

        if (fShmData->magic != 1337)
        {
            Close();
            jack_error("Can't open default MOD Desktop driver 3");
            return -1;
        }

       #ifdef __APPLE__
        task = mach_task_self();

        mach_port_t bootport1;
        if (task_get_bootstrap_port(task, &bootport1) != KERN_SUCCESS ||
            bootstrap_look_up(bootport1, fShmData->bootname1, &sem1) != KERN_SUCCESS)
        {
            Close();
            jack_error("Can't open default MOD Desktop driver 4");
            return -1;
        }

        mach_port_t bootport2;
        if (task_get_bootstrap_port(task, &bootport2) != KERN_SUCCESS ||
            bootstrap_look_up(bootport2, fShmData->bootname2, &sem2) != KERN_SUCCESS)
        {
            Close();
            jack_error("Can't open default MOD Desktop driver 5");
            return -1;
        }
       #endif

        return 0;
    }

    int Close() override
    {
        printf("%03d:%s\n", __LINE__, __FUNCTION__);
        fflush(stdout);

        JackAudioDriver::Close();

       #ifdef __APPLE__
        if (sem1 != MACH_PORT_NULL)
        {
            semaphore_destroy(task, sem1);
            sem1 = MACH_PORT_NULL;
        }

        if (sem2 != MACH_PORT_NULL)
        {
            semaphore_destroy(task, sem2);
            sem2 = MACH_PORT_NULL;
        }
       #endif

       #ifdef _WIN32
        if (fShmData != nullptr)
        {
            UnmapViewOfFile(fShmData);
            fShmData = nullptr;
        }

        if (fShm != nullptr)
        {
            CloseHandle(fShm);
            fShm = nullptr;
        }
       #else
        if (fShmData != nullptr)
        {
            munmap(fShmData, kDataSize);
            fShmData = nullptr;
        }

        if (fShmFd >= 0)
        {
            close(fShmFd);
            fShmFd = -1;
        }
       #endif

        return 0;
    }

    int Attach() override
    {
        printf("%03d:%s | %u\n", __LINE__, __FUNCTION__, fShmData->magic);
        fflush(stdout);

        if (JackAudioDriver::Attach() != 0)
            return -1;

        jack_port_id_t port_index;
        JackPort* port;
        if (fEngine->PortRegister(fClientControl.fRefNum, "system:midi_capture_1", JACK_DEFAULT_MIDI_TYPE,
                                  CaptureDriverFlags, fEngineControl->fBufferSize, &port_index) < 0)
        {
            Close();
            jack_error("Can't open default MOD Desktop driver 6");
            return -1;
        }
        fCaptureMidiPort = port_index;
        port = fGraphManager->GetPort(port_index);
        port->SetAlias("MOD Desktop MIDI Capture");

        if (fEngine->PortRegister(fClientControl.fRefNum, "system:midi_playback_1", JACK_DEFAULT_MIDI_TYPE,
                                  PlaybackDriverFlags, fEngineControl->fBufferSize, &port_index) < 0)
        {
        {
            Close();
            jack_error("Can't open default MOD Desktop driver 7");
            return -1;
        }
        }
        fPlaybackMidiPort = port_index;
        port = fGraphManager->GetPort(port_index);
        port->SetAlias("MOD Desktop MIDI Playback");

        return 0;
    }

    int Detach() override
    {
        printf("%03d:%s | %u\n", __LINE__, __FUNCTION__, fShmData->magic);
        fflush(stdout);

        if (JackAudioDriver::Detach() != 0)
            return -1;

        fEngine->PortUnRegister(fClientControl.fRefNum, fCaptureMidiPort);
        fEngine->PortUnRegister(fClientControl.fRefNum, fPlaybackMidiPort);
        return 0;
    }

    int Start() override
    {
        printf("%03d:%s\n", __LINE__, __FUNCTION__);
        fflush(stdout);
        if (JackAudioDriver::Start() != 0)
            return -1;

       #if 0 // def __APPLE__
        fEngineControl->fPeriod = fEngineControl->fPeriodUsecs * 1000;
        fEngineControl->fComputation = JackTools::ComputationMicroSec(fEngineControl->fBufferSize) * 1000;
        fEngineControl->fConstraint = fEngineControl->fPeriodUsecs * 1000;
       #endif

        fIsProcessing = fIsRunning = true;
        return JackThread::StartImp(&fProcessThread, 80, 1, on_process, this);
    }

    int Stop() override
    {
        printf("%03d:%s | %u\n", __LINE__, __FUNCTION__, fShmData->magic);
        fflush(stdout);

        fIsProcessing = false;

        if (fIsRunning)
        {
            fIsRunning = false;
            JackThread::StopImp(fProcessThread);
        }

        return JackAudioDriver::Stop();
    }

    int Read() override
    {
        memcpy(GetInputBuffer(0), fShmData->audio, sizeof(float) * fEngineControl->fBufferSize);
        memcpy(GetInputBuffer(1), fShmData->audio + fEngineControl->fBufferSize, sizeof(float) * fEngineControl->fBufferSize);

        JackMidiBuffer* cbuf = (JackMidiBuffer*)fGraphManager->GetBuffer(fCaptureMidiPort, fEngineControl->fBufferSize);
        JackMidiBuffer* pbuf = (JackMidiBuffer*)fGraphManager->GetBuffer(fPlaybackMidiPort, fEngineControl->fBufferSize);
        pbuf->Reset(fEngineControl->fBufferSize);

        for (uint16_t i = 0; i < fShmData->midiEventCount; ++i)
        {
            if (jack_midi_data_t* const data = cbuf->ReserveEvent(fShmData->midiFrames[i], 4))
            {
                memcpy(data, fShmData->midiData + (i * 4), 4);
                continue;
            }
            break;
        }

        return 0;
    }

    int Write() override
    {
        memcpy(fShmData->audio, GetOutputBuffer(0), sizeof(float) * fEngineControl->fBufferSize);
        memcpy(fShmData->audio + fEngineControl->fBufferSize, GetOutputBuffer(1), sizeof(float) * fEngineControl->fBufferSize);

        JackMidiBuffer* cbuf = (JackMidiBuffer*)fGraphManager->GetBuffer(fCaptureMidiPort, fEngineControl->fBufferSize);
        JackMidiBuffer* pbuf = (JackMidiBuffer*)fGraphManager->GetBuffer(fPlaybackMidiPort, fEngineControl->fBufferSize);
        cbuf->Reset(fEngineControl->fBufferSize);

        uint16_t mec = 0;
        for (uint32_t i = 0; i < pbuf->event_count; ++i)
        {
            JackMidiEvent& ev(pbuf->events[i]);

            if (ev.size > 4)
                continue;

            fShmData->midiFrames[mec] = ev.time;
            memcpy(fShmData->midiData + (mec * 4), ev.GetData(pbuf), ev.size);

            for (uint8_t j = ev.size; j < 4; ++j)
                fShmData->midiData[mec * 4 + j] = 0;

            if (++mec == 511)
                break;
        }
        fShmData->midiEventCount = mec;

        return 0;
    }

    bool IsFixedBufferSize() override
    {
        printf("%03d:%s\n", __LINE__, __FUNCTION__);
        return true;
    }
};

} // end of namespace

extern "C" {

#include "JackCompilerDeps.h"

SERVER_EXPORT jack_driver_desc_t* driver_get_descriptor()
{
    printf("%03d:%s\n", __LINE__, __FUNCTION__);
    jack_driver_desc_filler_t filler;
    jack_driver_param_value_t value;

    jack_driver_desc_t* const desc = jack_driver_descriptor_construct("desktop", JackDriverMaster, "MOD Desktop plugin audio backend", &filler);

    value.ui = 48000;
    jack_driver_descriptor_add_parameter(desc, &filler, "rate", 'r', JackDriverParamUInt, &value, nullptr, "Sample rate", nullptr);

    value.ui = 128;
    jack_driver_descriptor_add_parameter(desc, &filler, "period", 'p', JackDriverParamUInt, &value, nullptr, "Frames per period", nullptr);

    return desc;
}

SERVER_EXPORT Jack::JackDriverClientInterface* driver_initialize(Jack::JackLockedEngine* engine, Jack::JackSynchro* table, const JSList* params)
{
    jack_nframes_t rate = 48000;
    jack_nframes_t period = 128;

    for (const JSList* node = params; node; node = jack_slist_next(node))
    {
        const jack_driver_param_t* const param = (const jack_driver_param_t *) node->data;

        switch (param->character)
        {
        case 'r':
            rate = param->value.ui;
            break;
        case 'p':
            period = param->value.ui;
            break;
        }
    }

    Jack::JackDriverClientInterface* driver = new Jack::DesktopAudioDriver("system", "mod-desktop", engine, table);

    if (driver->Open(period, rate, true, true, 2, 2, false, "", "", 0, 0) == 0)
    {
        printf("%03d:%s OK\n", __LINE__, __FUNCTION__);
        return driver;
    }

    printf("%03d:%s FAIL\n", __LINE__, __FUNCTION__);
    delete driver;
    return nullptr;
}

} // extern "C"
