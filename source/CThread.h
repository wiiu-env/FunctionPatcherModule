/****************************************************************************
 * Copyright (C) 2015 Dimok
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/
#pragma once

#include <malloc.h>
#include <unistd.h>
#include <cstdint>
#include <coreinit/thread.h>

class CThread {
public:
    typedef void (*Callback)(CThread *thread, void *arg);

    //! constructor
    CThread(int32_t iAttr, int32_t iPriority = 16, int32_t iStackSize = 0x8000, CThread::Callback callback = NULL, void *callbackArg = NULL)
            : pThread(NULL), pThreadStack(NULL), pCallback(callback), pCallbackArg(callbackArg) {
        //! save attribute assignment
        iAttributes = iAttr;
        //! allocate the thread
        pThread = (OSThread *) memalign(0x10, sizeof(OSThread));
        //! allocate the stack
        pThreadStack = (uint8_t *) memalign(0x20, iStackSize);
        //! create the thread
        if (pThread && pThreadStack) {
            OSCreateThread(pThread, (int (*)(int, const char **)) &CThread::threadCallback, 1, (char *) this, (void *) (pThreadStack + iStackSize), iStackSize, iPriority, iAttributes);
        }
    }

    //! destructor
    virtual ~CThread() {
        shutdownThread();
    }

    static CThread *create(CThread::Callback callback, void *callbackArg, int32_t iAttr = eAttributeNone, int32_t iPriority = 16, int32_t iStackSize = 0x8000) {
        return (new CThread(iAttr, iPriority, iStackSize, callback, callbackArg));
    }

    static void runOnAllCores(CThread::Callback callback, void *callbackArg, int32_t iAttr = 0, int32_t iPriority = 16, int32_t iStackSize = 0x8000) {
        int32_t aff[] = {CThread::eAttributeAffCore2, CThread::eAttributeAffCore1, CThread::eAttributeAffCore0};

        for (uint32_t i = 0; i < (sizeof(aff) / sizeof(aff[0])); i++) {
            CThread *thread = CThread::create(callback, callbackArg, iAttr | aff[i], iPriority, iStackSize);
            thread->resumeThread();
            delete thread;
        }
    }

    //! Get thread ID
    virtual void *getThread() const {
        return pThread;
    }

    //! Thread entry function
    virtual void executeThread(void) {
        if (pCallback)
            pCallback(this, pCallbackArg);
    }

    //! Suspend thread
    virtual void suspendThread(void) {
        if (isThreadSuspended()) return;
        if (pThread) OSSuspendThread(pThread);
    }

    //! Resume thread
    virtual void resumeThread(void) {
        if (!isThreadSuspended()) return;
        if (pThread) OSResumeThread(pThread);
    }

    //! Set thread priority
    virtual void setThreadPriority(int32_t prio) {
        if (pThread) OSSetThreadPriority(pThread, prio);
    }

    //! Check if thread is suspended
    virtual bool isThreadSuspended(void) const {
        if (pThread) return OSIsThreadSuspended(pThread);
        return false;
    }

    //! Check if thread is terminated
    virtual bool isThreadTerminated(void) const {
        if (pThread) return OSIsThreadTerminated(pThread);
        return false;
    }

    //! Check if thread is running
    virtual bool isThreadRunning(void) const {
        return !isThreadSuspended() && !isThreadRunning();
    }

    //! Gets the thread affinity.
    virtual uint16_t getThreadAffinity(void) const {
        if (pThread) return OSGetThreadAffinity(pThread);
        return 0;
    }

    //! Shutdown thread
    virtual void shutdownThread(void) {
        //! wait for thread to finish
        if (pThread && !(iAttributes & eAttributeDetach)) {
            while (isThreadSuspended()) {
                resumeThread();
            }
            OSJoinThread(pThread, NULL);
        }
        //! free the thread stack buffer
        if (pThreadStack) {
            free(pThreadStack);
        }
        if (pThread) {
            free(pThread);
        }
        pThread = NULL;
        pThreadStack = NULL;
    }

    //! Thread attributes
    enum eCThreadAttributes {
        eAttributeNone = 0x07,
        eAttributeAffCore0 = 0x01,
        eAttributeAffCore1 = 0x02,
        eAttributeAffCore2 = 0x04,
        eAttributeDetach = 0x08,
        eAttributePinnedAff = 0x10
    };
private:
    static int32_t threadCallback(int32_t argc, void *arg) {
        //! After call to start() continue with the internal function
        ((CThread *) arg)->executeThread();
        return 0;
    }

    int32_t iAttributes;
    OSThread *pThread;
    uint8_t *pThreadStack;
    Callback pCallback;
    void *pCallbackArg;
};