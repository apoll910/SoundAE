/**
 * @file SoundKey_AEGP.h
 * @brief AEGP Bridge Header: Audio Extraction & Keyframe Suite Interaction
 */

#pragma once

#ifndef SOUNDKEY_AEGP_H
#define SOUNDKEY_AEGP_H

#include "SoundKey_Plugin.h"

// Forward declaration
struct SoundKeyGlobalData;
SoundKeyGlobalData* GetSoundKeyGlobalAudio();

// Wywołania z poziomu procedury EffectMain
PF_Err SoundKey_ExtractAudioFromLayer(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[]);
PF_Err SoundKey_BakeKeyframesToComposition(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[]);

#ifdef __cplusplus
extern "C" {
#endif

// Główny punkt wejścia AEGP General Plugin
A_Err GP_Main(
    struct SPBasicSuite *pica_basicP,
    A_long              major_versionL,
    A_long              minor_versionL,
    AEGP_PluginID       aegp_plugin_id,
    AEGP_GlobalRefcon   *global_refconP
);

#ifdef __cplusplus
}
#endif

#endif // SOUNDKEY_AEGP_H
