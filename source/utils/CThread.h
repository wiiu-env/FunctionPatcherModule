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

#include "globals.h"
#include "logger.h"
#include <coreinit/thread.h>
#include <cstdint>
#include <malloc.h>
#include <unistd.h>

class CThread {
public:
    typedef void (*Callback)(CThread *thread, void *arg);

    //! constructor
    explicit CThread(int32_t iAttr, int32_t iPriority = 16, int32_t stacksize = 0x8000, CThread::Callback callback = nullptr, void *callbackArg = nullptr)
        : pThread(nullptr), pThreadStack(nullptr), pCallback(callback), pCallbackArg(callbackArg) {
        //! save attribute assignment
        iAttributes = iAttr;
        iStackSize  = stacksize;
        //! allocate the thread on the default Cafe OS heap
        pThread = (OSThread *) gMEMAllocFromDefaultHeapExForThreads(sizeof(OSThread), 0x10);
        //! allocate the stack on the default Cafe OS heap
        pThreadStack = (uint8_t *) gMEMAllocFromDefaultHeapExForThreads(iStackSize, 0x20);
        //! create the thread
        if (pThread && pThreadStack) {
            // clang-format off
            OSCreateThread(pThread, (int(*)(int, const char **)) & CThread::threadCallback, 1, (char *) this, (void *) (pThreadStack + iStackSize), iStackSize, iPriority, iAttributes);
            // clang-format on
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
        for (int i : aff) {
            CThread thread(iAttr | i, iPriority, iStackSize, callback, callbackArg);
            thread.resumeThread();
        }
    }

    //! Get thread ID
    [[nodiscard]] virtual void *getThread() const {
        return pThread;
    }

    //! Thread entry function
    virtual void executeThread() {
        if (pCallback)
            pCallback(this, pCallbackArg);
    }

    //! Suspend thread
    virtual void suspendThread() {
        if (isThreadSuspended()) return;
        if (pThread) OSSuspendThread(pThread);
    }

    //! Resume thread
    virtual void resumeThread() {
        if (!isThreadSuspended()) return;
        if (pThread) {
            OSResumeThread(pThread);
        }
    }

    //! Set thread priority
    virtual void setThreadPriority(int32_t prio) {
        if (pThread) OSSetThreadPriority(pThread, prio);
    }

    //! Check if thread is suspended
    [[nodiscard]] virtual bool isThreadSuspended() const {
        if (pThread) return OSIsThreadSuspended(pThread);
        return false;
    }

    //! Check if thread is terminated
    [[nodiscard]] virtual bool isThreadTerminated() const {
        if (pThread) return OSIsThreadTerminated(pThread);
        return false;
    }

    //! Check if thread is running
    [[nodiscard]] virtual bool isThreadRunning() const {
        return !isThreadSuspended() && !isThreadRunning();
    }

    //! Gets the thread affinity.
    [[nodiscard]] virtual uint16_t getThreadAffinity() const {
        if (pThread) return OSGetThreadAffinity(pThread);
        return 0;
    }

    //! Shutdown thread
    void shutdownThread() {
        //! wait for thread to finish
        if (pThread && !(iAttributes & eAttributeDetach)) {
            while (isThreadSuspended()) {
                resumeThread();
            }
            OSJoinThread(pThread, nullptr);
        }
        // Some games (e.g. Minecraft) expect the default heap to be empty.
        // Make sure to clean up the memory after using it
        //! free the thread stack buffer
        if (pThreadStack) {
            memset(pThreadStack, 0, iStackSize);
            gMEMFreeToDefaultHeapForThreads(pThreadStack);
        }
        if (pThread) {
            memset(pThread, 0, sizeof(OSThread));
            gMEMFreeToDefaultHeapForThreads(pThread);
        }
        pThread      = nullptr;
        pThreadStack = nullptr;
    }

    //! Thread attributes
    enum eCThreadAttributes {
        eAttributeNone      = 0x07,
        eAttributeAffCore0  = 0x01,
        eAttributeAffCore1  = 0x02,
        eAttributeAffCore2  = 0x04,
        eAttributeDetach    = 0x08,
        eAttributePinnedAff = 0x10
    };

private:
    static int32_t threadCallback(int32_t argc, void *arg) {
        //! After call to start() continue with the internal function
        ((CThread *) arg)->executeThread();
        return 0;
    }

    uint32_t iStackSize;
    int32_t iAttributes;
    OSThread *pThread;
    uint8_t *pThreadStack;
    Callback pCallback;
    void *pCallbackArg;
};