#pragma once

typedef enum function_replacement_library_type_t {
    LIBRARY_AVM,
    LIBRARY_CAMERA,
    LIBRARY_COREINIT,
    LIBRARY_DC,
    LIBRARY_DMAE,
    LIBRARY_DRMAPP,
    LIBRARY_ERREULA,
    LIBRARY_GX2,
    LIBRARY_H264,
    LIBRARY_LZMA920,
    LIBRARY_MIC,
    LIBRARY_NFC,
    LIBRARY_NIO_PROF,
    LIBRARY_NLIBCURL,
    LIBRARY_NLIBNSS,
    LIBRARY_NLIBNSS2,
    LIBRARY_NN_AC,
    LIBRARY_NN_ACP,
    LIBRARY_NN_ACT,
    LIBRARY_NN_AOC,
    LIBRARY_NN_BOSS,
    LIBRARY_NN_CCR,
    LIBRARY_NN_CMPT,
    LIBRARY_NN_DLP,
    LIBRARY_NN_EC,
    LIBRARY_NN_FP,
    LIBRARY_NN_HAI,
    LIBRARY_NN_HPAD,
    LIBRARY_NN_IDBE,
    LIBRARY_NN_NDM,
    LIBRARY_NN_NETS2,
    LIBRARY_NN_NFP,
    LIBRARY_NN_NIM,
    LIBRARY_NN_OLV,
    LIBRARY_NN_PDM,
    LIBRARY_NN_SAVE,
    LIBRARY_NN_SL,
    LIBRARY_NN_SPM,
    LIBRARY_NN_TEMP,
    LIBRARY_NN_UDS,
    LIBRARY_NN_VCTL,
    LIBRARY_NSYSCCR,
    LIBRARY_NSYSHID,
    LIBRARY_NSYSKBD,
    LIBRARY_NSYSNET,
    LIBRARY_NSYSUHS,
    LIBRARY_NSYSUVD,
    LIBRARY_NTAG,
    LIBRARY_PADSCORE,
    LIBRARY_PROC_UI,
    LIBRARY_SND_CORE,
    LIBRARY_SND_USER,
    LIBRARY_SNDCORE2,
    LIBRARY_SNDUSER2,
    LIBRARY_SWKBD,
    LIBRARY_SYSAPP,
    LIBRARY_TCL,
    LIBRARY_TVE,
    LIBRARY_UAC,
    LIBRARY_UAC_RPL,
    LIBRARY_USB_MIC,
    LIBRARY_UVC,
    LIBRARY_UVD,
    LIBRARY_VPAD,
    LIBRARY_VPADBASE,
    LIBRARY_ZLIB125,
    LIBRARY_OTHER,
} function_replacement_library_type_t;

#define MAXIMUM_FUNCTION_NAME_LENGTH                        100
#define FUNCTION_PATCHER_METHOD_STORE_SIZE                  20

#define STATIC_FUNCTION         0
#define DYNAMIC_FUNCTION        1

typedef struct function_replacement_data_t {
    uint32_t                            physicalAddr;                                       /* [needs to be filled]  */
    uint32_t                            virtualAddr;                                        /* [needs to be filled]  */
    uint32_t                            replaceAddr;                                        /* [needs to be filled] Address of our replacement function */
    uint32_t                            replaceCall;                                        /* [needs to be filled] Address to access the real_function */
    function_replacement_library_type_t library;                                            /* [needs to be filled] rpl where the function we want to replace is. */
    char                                function_name[MAXIMUM_FUNCTION_NAME_LENGTH];        /* [needs to be filled] name of the function we want to replace */
    uint32_t                            realAddr;                                           /* [will be filled] Address of the real function we want to replace. */
    volatile uint32_t                   replace_data [FUNCTION_PATCHER_METHOD_STORE_SIZE];  /* [will be filled] Space for us to store some jump instructions */
    uint32_t                            restoreInstruction;                                 /* [will be filled] Copy of the instruction we replaced to jump to our code. */
    uint8_t                             functionType;                                       /* [will be filled] */
    uint8_t                             alreadyPatched;                                     /* [will be filled] */
} function_replacement_data_t;

#define REPLACE_FUNCTION(x, lib, function_name) \
    { \
        0, \
        0, \
        (uint32_t) my_ ## x, \
        (uint32_t) &real_ ## x \
        lib, \
        # function_name, \
        0, \
        {}, \
        0, \
        functionType, \
        0 \
    }

#define REPLACE_FUNCTION_VIA_ADDRESS(x, physicalAddress, effectiveAddress) \
    { \
        physicalAddress, \
        effectiveAddress, \
        (uint32_t) my_ ## x, \
        (uint32_t) &real_ ## x \
        LIBRARY_OTHER, \
        # x, \
        0, \
        {}, \
        0, \
        functionType, \
        0 \
    }

#define DECL_FUNCTION(res, name, ...) \
        res (* real_ ## name)(__VA_ARGS__) __attribute__((section(".data"))); \
        res my_ ## name(__VA_ARGS__)