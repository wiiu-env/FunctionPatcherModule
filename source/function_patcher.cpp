
#include <coreinit/dynload.h>
#include <coreinit/cache.h>
#include <coreinit/debug.h>
#include <coreinit/memorymap.h>
#include <kernel/kernel.h>
#include "function_patcher.h"
#include "logger.h"
#include "CThread.h"

#define DEBUG_LOG_DYN 0

void writeDataAndFlushIC(CThread *thread, void *arg) {
    uint32_t *data = (uint32_t *) arg;
    uint16_t core = OSGetThreadAffinity(OSGetCurrentThread());

    DCFlushRange(data, sizeof(uint32_t) * 3);

    uint32_t replace_instruction = data[0];
    uint32_t physical_address = data[1];
    uint32_t effective_address = data[2];
    DCFlushRange(&replace_instruction, 4);
    DCFlushRange(&physical_address, 4);

    DEBUG_FUNCTION_LINE("Write instruction %08X to %08X [%08X] on core %d", replace_instruction, effective_address, physical_address, core / 2);

    uint32_t replace_instruction_physical = (uint32_t) &replace_instruction;

    if (replace_instruction_physical < 0x00800000 || replace_instruction_physical >= 0x01000000) {
        replace_instruction_physical = OSEffectiveToPhysical(replace_instruction_physical);
    } else {
        replace_instruction_physical = replace_instruction_physical + 0x30800000 - 0x00800000;
    }

    KernelCopyData(physical_address, replace_instruction_physical, 4);
    ICInvalidateRange((void *) (effective_address), 4);
}


void FunctionPatcherPatchFunction(function_replacement_data_t *replacements, uint32_t size) {
    uint32_t skip_instr = 1;
    uint32_t my_instr_len = 4;
    uint32_t instr_len = my_instr_len + skip_instr + 15;
    uint32_t flush_len = 4 * instr_len;

    for (uint32_t i = 0; i < size; i++) {
        function_replacement_data_t *function_data = &replacements[i];
        /* Patch branches to it.  */
        volatile uint32_t *space = function_data->replace_data;

        DEBUG_FUNCTION_LINE_WRITE("Patching %s ...", function_data->function_name);

        if (function_data->library == LIBRARY_OTHER) {
            WHBLogWritef("Oh, using straight PA/VA");
            if (function_data->alreadyPatched == 1) {
                DEBUG_FUNCTION_LINE("Skipping %s, its already patched", function_data->function_name);
                continue;
            }
        } else {
            if (function_data->functionType == STATIC_FUNCTION && function_data->alreadyPatched == 1) {
                if (isDynamicFunction((uint32_t) OSEffectiveToPhysical(function_data->realAddr))) {
                    DEBUG_FUNCTION_LINE("INFO: The function %s is a dynamic function.", function_data->function_name);
                    function_data->functionType = DYNAMIC_FUNCTION;
                } else {
                    WHBLogWritef("Skipping %s, its already patched", function_data->function_name);
                    continue;
                }
            }
        }

        uint32_t physical = function_data->physicalAddr;
        uint32_t repl_addr = (uint32_t) function_data->replaceAddr;
        uint32_t call_addr = (uint32_t) function_data->replaceCall;

        uint32_t real_addr = function_data->virtualAddr;
        if (function_data->library != LIBRARY_OTHER) {
            real_addr = getAddressOfFunction(function_data->function_name, function_data->library);
        }

        if (!real_addr) {
            WHBLogWritef("");
            DEBUG_FUNCTION_LINE("OSDynLoad_FindExport failed for %s", function_data->function_name);
            continue;
        }

        if (DEBUG_LOG_DYN) {
            DEBUG_FUNCTION_LINE("%s is located at %08X!", function_data->function_name, real_addr);
        }

        if (function_data->library != LIBRARY_OTHER) {
            physical = (uint32_t) OSEffectiveToPhysical(real_addr);
        }

        if (!physical) {
            WHBLogWritef("Error. Something is wrong with the physical address");
            continue;
        }

        if (DEBUG_LOG_DYN) {
            DEBUG_FUNCTION_LINE("%s physical is located at %08X!", function_data->function_name, physical);
        }

        *(volatile uint32_t *) (call_addr) = (uint32_t) (space);

        uint32_t targetAddr = (uint32_t) space;
        if (targetAddr < 0x00800000 || targetAddr >= 0x01000000) {
            targetAddr = (uint32_t) OSEffectiveToPhysical(targetAddr);
        } else {
            targetAddr = targetAddr + 0x30800000 - 0x00800000;
        }

        KernelCopyData(targetAddr, physical, 4);

        ICInvalidateRange((void *) (space), 4);
        DCFlushRange((void *) (space), 4);

        space++;

        //Only works if skip_instr == 1
        if (skip_instr == 1) {
            // fill the restore instruction section
            function_data->realAddr = real_addr;
            function_data->restoreInstruction = space[-1];
            if (DEBUG_LOG_DYN) {
                DEBUG_FUNCTION_LINE("function_data->realAddr = %08X!", function_data->realAddr);
            }
            if (DEBUG_LOG_DYN) {
                DEBUG_FUNCTION_LINE("function_data->restoreInstruction = %08X!", function_data->restoreInstruction);
            }
        } else {
            WHBLogWritef("Error. Can't save %s for restoring!", function_data->function_name);
        }

        /*
        00808cfc 3d601234      lis        r11 ,0x1234
        00808d00 616b5678      ori        r11 ,r11 ,0x5678
        00808d04 7d6903a6      mtspr      CTR ,r11
        00808d08 4e800420      bctr
         */

        *space = 0x3d600000 | (((real_addr + (skip_instr * 4)) >> 16) & 0x0000FFFF);
        space++;   // lis        r11 ,0x1234
        *space = 0x616b0000 | ((real_addr + (skip_instr * 4)) & 0x0000ffff);
        space++;           // ori        r11 ,r11 ,0x5678
        *space = 0x7d6903a6;
        space++;                                                           // mtspr      CTR ,r11
        *space = 0x4e800420;
        space++;


        uint32_t repl_addr_test = (uint32_t) space;
        /*
        // Only use patched function if OSGetUPID is 2 (wii u menu) or 15 (game)
        *space = 0x3d600000 | (((uint32_t*) OSGetUPID)[0] & 0x0000FFFF); space++;               // lis        r11 ,0x0
        *space = 0x816b0000 | (((uint32_t*) OSGetUPID)[1] & 0x0000FFFF); space++;               // lwz        r11 ,0x0(r11)
        *space = 0x2c0b0000 | 0x00000002; space++;                                              // cmpwi      r11 ,0x2
        *space = 0x41820000 | 0x00000020; space++;                                              // beq        myfunc
        *space = 0x2c0b0000 | 0x0000000F; space++;                                              // cmpwi      r11 ,0xF
        *space = 0x41820000 | 0x00000018; space++;                                              // beq        myfunc
        *space = 0x3d600000 | (((real_addr + (skip_instr * 4)) >> 16) & 0x0000FFFF); space++;   // lis        r11 ,0x1234
        *space = 0x616b0000 | ((real_addr + (skip_instr * 4)) & 0x0000ffff); space++;           // ori        r11 ,r11 ,0x5678
        *space = 0x7d6903a6; space++;                                                           // mtspr      CTR ,r11
        *space = function_data->restoreInstruction; space++;                                    //
        *space = 0x4e800420; space++;                                                           // bctr*/
// myfunc:
        *space = 0x3d600000 | (((repl_addr) >> 16) & 0x0000FFFF);
        space++;                      // lis        r11 ,0x1234
        *space = 0x616b0000 | ((repl_addr) & 0x0000ffff);
        space++;                              // ori        r11 ,r11 ,0x5678
        *space = 0x7d6903a6;
        space++;                                                           // mtspr      CTR ,r11
        *space = 0x4e800420;
        space++;                                                           // bctr

        DCFlushRange((void *) (((uint32_t) space) - flush_len), flush_len);
        ICInvalidateRange((void *) (((uint32_t) space) - flush_len), flush_len);

        if ((repl_addr_test & 0x03fffffc) != repl_addr_test) {
            OSFatal("Jump is impossible");
        }

        //setting jump back
        uint32_t replace_instr = 0x48000002 | (repl_addr_test & 0x03fffffc);

        uint32_t data[] = {
                replace_instr,
                physical,
                real_addr
        };
        CThread::runOnAllCores(writeDataAndFlushIC, data);

        function_data->alreadyPatched = 1;
        DEBUG_FUNCTION_LINE("done with patching %s!", function_data->function_name);

    }
    DEBUG_FUNCTION_LINE("Done with patching given functions!");

}

bool isDynamicFunction(uint32_t physicalAddress) {
    if ((physicalAddress & 0x80000000) == 0x80000000) {
        return 1;
    }
    return 0;
}

rpl_handling rpl_handles[] __attribute__((section(".data"))) = {
        {LIBRARY_AVM,      "avm.rpl",      0},
        {LIBRARY_CAMERA,   "camera.rpl",   0},
        {LIBRARY_COREINIT, "coreinit.rpl", 0},
        {LIBRARY_DC,       "dc.rpl",       0},
        {LIBRARY_DMAE,     "dmae.rpl",     0},
        {LIBRARY_DRMAPP,   "drmapp.rpl",   0},
        {LIBRARY_ERREULA,  "erreula.rpl",  0},
        {LIBRARY_GX2,      "gx2.rpl",      0},
        {LIBRARY_H264,     "h264.rpl",     0},
        {LIBRARY_LZMA920,  "lzma920.rpl",  0},
        {LIBRARY_MIC,      "mic.rpl",      0},
        {LIBRARY_NFC,      "nfc.rpl",      0},
        {LIBRARY_NIO_PROF, "nio_prof.rpl", 0},
        {LIBRARY_NLIBCURL, "nlibcurl.rpl", 0},
        {LIBRARY_NLIBNSS,  "nlibnss.rpl",  0},
        {LIBRARY_NLIBNSS2, "nlibnss2.rpl", 0},
        {LIBRARY_NN_AC,    "nn_ac.rpl",    0},
        {LIBRARY_NN_ACP,   "nn_acp.rpl",   0},
        {LIBRARY_NN_ACT,   "nn_act.rpl",   0},
        {LIBRARY_NN_AOC,   "nn_aoc.rpl",   0},
        {LIBRARY_NN_BOSS,  "nn_boss.rpl",  0},
        {LIBRARY_NN_CCR,   "nn_ccr.rpl",   0},
        {LIBRARY_NN_CMPT,  "nn_cmpt.rpl",  0},
        {LIBRARY_NN_DLP,   "nn_dlp.rpl",   0},
        {LIBRARY_NN_EC,    "nn_ec.rpl",    0},
        {LIBRARY_NN_FP,    "nn_fp.rpl",    0},
        {LIBRARY_NN_HAI,   "nn_hai.rpl",   0},
        {LIBRARY_NN_HPAD,  "nn_hpad.rpl",  0},
        {LIBRARY_NN_IDBE,  "nn_idbe.rpl",  0},
        {LIBRARY_NN_NDM,   "nn_ndm.rpl",   0},
        {LIBRARY_NN_NETS2, "nn_nets2.rpl", 0},
        {LIBRARY_NN_NFP,   "nn_nfp.rpl",   0},
        {LIBRARY_NN_NIM,   "nn_nim.rpl",   0},
        {LIBRARY_NN_OLV,   "nn_olv.rpl",   0},
        {LIBRARY_NN_PDM,   "nn_pdm.rpl",   0},
        {LIBRARY_NN_SAVE,  "nn_save.rpl",  0},
        {LIBRARY_NN_SL,    "nn_sl.rpl",    0},
        {LIBRARY_NN_SPM,   "nn_spm.rpl",   0},
        {LIBRARY_NN_TEMP,  "nn_temp.rpl",  0},
        {LIBRARY_NN_UDS,   "nn_uds.rpl",   0},
        {LIBRARY_NN_VCTL,  "nn_vctl.rpl",  0},
        {LIBRARY_NSYSCCR,  "nsysccr.rpl",  0},
        {LIBRARY_NSYSHID,  "nsyshid.rpl",  0},
        {LIBRARY_NSYSKBD,  "nsyskbd.rpl",  0},
        {LIBRARY_NSYSNET,  "nsysnet.rpl",  0},
        {LIBRARY_NSYSUHS,  "nsysuhs.rpl",  0},
        {LIBRARY_NSYSUVD,  "nsysuvd.rpl",  0},
        {LIBRARY_NTAG,     "ntag.rpl",     0},
        {LIBRARY_PADSCORE, "padscore.rpl", 0},
        {LIBRARY_PROC_UI,  "proc_ui.rpl",  0},
        {LIBRARY_SNDCORE2, "sndcore2.rpl", 0},
        {LIBRARY_SNDUSER2, "snduser2.rpl", 0},
        {LIBRARY_SND_CORE, "snd_core.rpl", 0},
        {LIBRARY_SND_USER, "snd_user.rpl", 0},
        {LIBRARY_SWKBD,    "swkbd.rpl",    0},
        {LIBRARY_SYSAPP,   "sysapp.rpl",   0},
        {LIBRARY_TCL,      "tcl.rpl",      0},
        {LIBRARY_TVE,      "tve.rpl",      0},
        {LIBRARY_UAC,      "uac.rpl",      0},
        {LIBRARY_UAC_RPL,  "uac_rpl.rpl",  0},
        {LIBRARY_USB_MIC,  "usb_mic.rpl",  0},
        {LIBRARY_UVC,      "uvc.rpl",      0},
        {LIBRARY_UVD,      "uvd.rpl",      0},
        {LIBRARY_VPAD,     "vpad.rpl",     0},
        {LIBRARY_VPADBASE, "vpadbase.rpl", 0},
        {LIBRARY_ZLIB125,  "zlib125.rpl",  0}
};

uint32_t getAddressOfFunction(char *functionName, function_replacement_library_type_t library) {
    uint32_t real_addr = 0;

    OSDynLoad_Module rpl_handle = 0;

    int32_t rpl_handles_size = sizeof rpl_handles / sizeof rpl_handles[0];

    for (int32_t i = 0; i < rpl_handles_size; i++) {
        if (rpl_handles[i].library == library) {
            if (rpl_handles[i].handle == 0) {
                DEBUG_FUNCTION_LINE("Lets acquire handle for rpl: %s", rpl_handles[i].rplname);
                OSDynLoad_Acquire((char *) rpl_handles[i].rplname, &rpl_handles[i].handle);
            }
            if (rpl_handles[i].handle == 0) {
                WHBLogWritef("%s failed to acquire", rpl_handles[i].rplname);
                return 0;
            }
            rpl_handle = rpl_handles[i].handle;
            break;
        }
    }

    if (!rpl_handle) {
        DEBUG_FUNCTION_LINE("Failed to find the RPL handle for %s", functionName);
        return 0;
    }

    OSDynLoad_FindExport(rpl_handle, 0, functionName, reinterpret_cast<void **>(&real_addr));

    if (!real_addr) {
        DEBUG_FUNCTION_LINE("OSDynLoad_FindExport failed for %s", functionName);
        return 0;
    }

    if ((library == LIBRARY_NN_ACP) && (uint32_t) (*(volatile uint32_t *) (real_addr) & 0x48000002) == 0x48000000) {
        uint32_t address_diff = (uint32_t) (*(volatile uint32_t *) (real_addr) & 0x03FFFFFC);
        if ((address_diff & 0x03000000) == 0x03000000) {
            address_diff |= 0xFC000000;
        }
        real_addr += (int32_t) address_diff;
        if ((uint32_t) (*(volatile uint32_t *) (real_addr) & 0x48000002) == 0x48000000) {
            return 0;
        }
    }

    return real_addr;
}


void FunctionPatcherResetLibHandles() {
    int32_t rpl_handles_size = sizeof rpl_handles / sizeof rpl_handles[0];

    for (int32_t i = 0; i < rpl_handles_size; i++) {
        if (rpl_handles[i].handle != 0) {
            DEBUG_FUNCTION_LINE("Resetting handle for rpl: %s", rpl_handles[i].rplname);
        }
        rpl_handles[i].handle = 0;
        // Release handle?
    }
}
