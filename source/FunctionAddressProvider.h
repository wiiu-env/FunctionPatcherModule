#pragma once

#include <coreinit/dynload.h>
#include <cstdint>
#include <function_patcher/fpatching_defines.h>
#include <list>

typedef struct rpl_handling {
    function_replacement_library_type_t library;
    const char rplname[15];
    OSDynLoad_Module handle;
} rpl_handling;

class FunctionAddressProvider {
public:
    uint32_t getEffectiveAddressOfFunction(function_replacement_library_type_t library, const char *functionName);
    void resetHandles();

    std::list<rpl_handling> rpl_handles = {
            {LIBRARY_AVM, "avm.rpl", nullptr},
            {LIBRARY_CAMERA, "camera.rpl", nullptr},
            {LIBRARY_COREINIT, "coreinit.rpl", nullptr},
            {LIBRARY_DC, "dc.rpl", nullptr},
            {LIBRARY_DMAE, "dmae.rpl", nullptr},
            {LIBRARY_DRMAPP, "drmapp.rpl", nullptr},
            {LIBRARY_ERREULA, "erreula.rpl", nullptr},
            {LIBRARY_GX2, "gx2.rpl", nullptr},
            {LIBRARY_H264, "h264.rpl", nullptr},
            {LIBRARY_LZMA920, "lzma920.rpl", nullptr},
            {LIBRARY_MIC, "mic.rpl", nullptr},
            {LIBRARY_NFC, "nfc.rpl", nullptr},
            {LIBRARY_NIO_PROF, "nio_prof.rpl", nullptr},
            {LIBRARY_NLIBCURL, "nlibcurl.rpl", nullptr},
            {LIBRARY_NLIBNSS, "nlibnss.rpl", nullptr},
            {LIBRARY_NLIBNSS2, "nlibnss2.rpl", nullptr},
            {LIBRARY_NN_AC, "nn_ac.rpl", nullptr},
            {LIBRARY_NN_ACP, "nn_acp.rpl", nullptr},
            {LIBRARY_NN_ACT, "nn_act.rpl", nullptr},
            {LIBRARY_NN_AOC, "nn_aoc.rpl", nullptr},
            {LIBRARY_NN_BOSS, "nn_boss.rpl", nullptr},
            {LIBRARY_NN_CCR, "nn_ccr.rpl", nullptr},
            {LIBRARY_NN_CMPT, "nn_cmpt.rpl", nullptr},
            {LIBRARY_NN_DLP, "nn_dlp.rpl", nullptr},
            {LIBRARY_NN_EC, "nn_ec.rpl", nullptr},
            {LIBRARY_NN_FP, "nn_fp.rpl", nullptr},
            {LIBRARY_NN_HAI, "nn_hai.rpl", nullptr},
            {LIBRARY_NN_HPAD, "nn_hpad.rpl", nullptr},
            {LIBRARY_NN_IDBE, "nn_idbe.rpl", nullptr},
            {LIBRARY_NN_NDM, "nn_ndm.rpl", nullptr},
            {LIBRARY_NN_NETS2, "nn_nets2.rpl", nullptr},
            {LIBRARY_NN_NFP, "nn_nfp.rpl", nullptr},
            {LIBRARY_NN_NIM, "nn_nim.rpl", nullptr},
            {LIBRARY_NN_OLV, "nn_olv.rpl", nullptr},
            {LIBRARY_NN_PDM, "nn_pdm.rpl", nullptr},
            {LIBRARY_NN_SAVE, "nn_save.rpl", nullptr},
            {LIBRARY_NN_SL, "nn_sl.rpl", nullptr},
            {LIBRARY_NN_SPM, "nn_spm.rpl", nullptr},
            {LIBRARY_NN_TEMP, "nn_temp.rpl", nullptr},
            {LIBRARY_NN_UDS, "nn_uds.rpl", nullptr},
            {LIBRARY_NN_VCTL, "nn_vctl.rpl", nullptr},
            {LIBRARY_NSYSCCR, "nsysccr.rpl", nullptr},
            {LIBRARY_NSYSHID, "nsyshid.rpl", nullptr},
            {LIBRARY_NSYSKBD, "nsyskbd.rpl", nullptr},
            {LIBRARY_NSYSNET, "nsysnet.rpl", nullptr},
            {LIBRARY_NSYSUHS, "nsysuhs.rpl", nullptr},
            {LIBRARY_NSYSUVD, "nsysuvd.rpl", nullptr},
            {LIBRARY_NTAG, "ntag.rpl", nullptr},
            {LIBRARY_PADSCORE, "padscore.rpl", nullptr},
            {LIBRARY_PROC_UI, "proc_ui.rpl", nullptr},
            {LIBRARY_SNDCORE2, "sndcore2.rpl", nullptr},
            {LIBRARY_SNDUSER2, "snduser2.rpl", nullptr},
            {LIBRARY_SND_CORE, "snd_core.rpl", nullptr},
            {LIBRARY_SND_USER, "snd_user.rpl", nullptr},
            {LIBRARY_SWKBD, "swkbd.rpl", nullptr},
            {LIBRARY_SYSAPP, "sysapp.rpl", nullptr},
            {LIBRARY_TCL, "tcl.rpl", nullptr},
            {LIBRARY_TVE, "tve.rpl", nullptr},
            {LIBRARY_UAC, "uac.rpl", nullptr},
            {LIBRARY_UAC_RPL, "uac_rpl.rpl", nullptr},
            {LIBRARY_USB_MIC, "usb_mic.rpl", nullptr},
            {LIBRARY_UVC, "uvc.rpl", nullptr},
            {LIBRARY_UVD, "uvd.rpl", nullptr},
            {LIBRARY_VPAD, "vpad.rpl", nullptr},
            {LIBRARY_VPADBASE, "vpadbase.rpl", nullptr},
            {LIBRARY_ZLIB125, "zlib125.rpl", nullptr}};
};
