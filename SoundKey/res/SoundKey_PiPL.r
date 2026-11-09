/**
 * @file SoundKey_PiPL.r
 * @brief Adobe After Effects Plug-in Property List (PiPL) Resource Definition
 * Universal 64-bit Entry Points (Windows x64 + macOS Apple Silicon & Intel)
 */

#ifndef AE_OS_WIN
    #include "AEConfig.h"
#endif

#include "AE_General.r"

resource 'PiPL' (16000) {
    {
        Kind {
            AEEffect
        },
        Name {
            "SoundKey Audio Analyzer"
        },
        Category {
            "Audio / Motion"
        },
#ifdef AE_OS_WIN
        CodeWin64X86 {
            "EffectMain"
        },
#else
        CodeMachOPowerPC {
            0, 0, ""
        },
        CodeMacIntel64 {
            "EffectMain"
        },
        CodeMacARM64 {
            "EffectMain"
        },
#endif
        AE_Effect_Spec_Version {
            PF_PLUG_IN_VERSION,
            PF_PLUG_IN_SUBVERS
        },
        AE_Effect_Version {
            524289  /* 1.0 */
        },
        AE_Effect_Info_Flags {
            0
        },
        AE_Effect_Global_OutFlags {
            0x00000004 | /* PF_OutFlag_DEEP_COLOR_AWARE */
            0x00000008 | /* PF_OutFlag_CUSTOM_UI */
            0x00000200   /* PF_OutFlag_PIX_INDEPENDENT */
        },
        AE_Effect_Global_OutFlags_2 {
            0x00000001 | /* PF_OutFlag2_SUPPORTS_SMART_RENDER */
            0x00000002   /* PF_OutFlag2_SUPPORTS_THREADED_RENDERING (MFR Multi-Frame Rendering) */
        },
        AE_Effect_Match_Name {
            "ADBE_SoundKey_Native"
        },
        AE_Reserved_Info {
            0
        }
    }
};
