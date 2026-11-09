/**
 * @file SoundKey_Plugin.h
 * @brief Adobe After Effects Native C++ Plugin - Master Header
 * Compatible with Adobe After Effects SDK (CS6 - CC 2025)
 * Multi-Frame Rendering (MFR) Thread-Safe Architecture
 * 
 * Copyright (c) 2025. All Rights Reserved.
 */

#pragma once

#ifndef SOUNDKEY_PLUGIN_H
#define SOUNDKEY_PLUGIN_H

#include "AEConfig.h"
#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "AE_EffectSuites.h"
#include "String_Utils.h"
#include "AEGP_SuiteHandler.h"

#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <cstdint>

#define MAJOR_VERSION     1
#define MINOR_VERSION     0
#define BUG_VERSION       0
#define STAGE_VERSION     PF_Stage_DEVELOP
#define BUILD_VERSION     1

#define PLUGIN_NAME       "SoundKey Audio Analyzer"
#define PLUGIN_MATCH_NAME "ADBE_SoundKey_Native"
#define PLUGIN_CATEGORY   "Audio / Motion"

// Kompletny zestaw identyfikatorów parametrów Effect Controls
enum {
    SOUNDKEY_INPUT = 0,
    
    // --- GRUPA 1: AUDIO SOURCE ---
    SOUNDKEY_GROUP_AUDIO_START,
    SOUNDKEY_AUDIO_LAYER_POPUP,
    SOUNDKEY_AUDIO_CHANNEL_POPUP,
    SOUNDKEY_PRESCAN_BUTTON,
    SOUNDKEY_GROUP_AUDIO_END,
    
    // --- GRUPA 2: CHANNEL 1 (BASS: 20 - 150 Hz) ---
    SOUNDKEY_GROUP_CH1_START,
    SOUNDKEY_CH1_ENABLE,
    SOUNDKEY_CH1_FREQ_LOW,
    SOUNDKEY_CH1_FREQ_HIGH,
    SOUNDKEY_CH1_SENSITIVITY,
    SOUNDKEY_CH1_THRESHOLD,
    SOUNDKEY_CH1_ATTACK,
    SOUNDKEY_CH1_RELEASE,
    SOUNDKEY_CH1_CURVE,
    SOUNDKEY_CH1_MIN_VAL,
    SOUNDKEY_CH1_MAX_VAL,
    SOUNDKEY_CH1_OUT_VALUE,
    SOUNDKEY_GROUP_CH1_END,
    
    // --- GRUPA 3: CHANNEL 2 (MID: 150 - 2000 Hz) ---
    SOUNDKEY_GROUP_CH2_START,
    SOUNDKEY_CH2_ENABLE,
    SOUNDKEY_CH2_FREQ_LOW,
    SOUNDKEY_CH2_FREQ_HIGH,
    SOUNDKEY_CH2_SENSITIVITY,
    SOUNDKEY_CH2_THRESHOLD,
    SOUNDKEY_CH2_ATTACK,
    SOUNDKEY_CH2_RELEASE,
    SOUNDKEY_CH2_CURVE,
    SOUNDKEY_CH2_MIN_VAL,
    SOUNDKEY_CH2_MAX_VAL,
    SOUNDKEY_CH2_OUT_VALUE,
    SOUNDKEY_GROUP_CH2_END,
    
    // --- GRUPA 4: CHANNEL 3 (HIGH: 2000 - 12000 Hz) ---
    SOUNDKEY_GROUP_CH3_START,
    SOUNDKEY_CH3_ENABLE,
    SOUNDKEY_CH3_FREQ_LOW,
    SOUNDKEY_CH3_FREQ_HIGH,
    SOUNDKEY_CH3_SENSITIVITY,
    SOUNDKEY_CH3_THRESHOLD,
    SOUNDKEY_CH3_ATTACK,
    SOUNDKEY_CH3_RELEASE,
    SOUNDKEY_CH3_CURVE,
    SOUNDKEY_CH3_MIN_VAL,
    SOUNDKEY_CH3_MAX_VAL,
    SOUNDKEY_CH3_OUT_VALUE,
    SOUNDKEY_GROUP_CH3_END,

    // --- GRUPA 5: CHANNEL 4 (CUSTOM / TRANSIENTS) ---
    SOUNDKEY_GROUP_CH4_START,
    SOUNDKEY_CH4_ENABLE,
    SOUNDKEY_CH4_FREQ_LOW,
    SOUNDKEY_CH4_FREQ_HIGH,
    SOUNDKEY_CH4_SENSITIVITY,
    SOUNDKEY_CH4_THRESHOLD,
    SOUNDKEY_CH4_ATTACK,
    SOUNDKEY_CH4_RELEASE,
    SOUNDKEY_CH4_CURVE,
    SOUNDKEY_CH4_MIN_VAL,
    SOUNDKEY_CH4_MAX_VAL,
    SOUNDKEY_CH4_OUT_VALUE,
    SOUNDKEY_GROUP_CH4_END,
    
    // --- GRUPA 6: KEYFRAME GENERATION & ACTIONS ---
    SOUNDKEY_GROUP_BAKE_START,
    SOUNDKEY_TOLERANCE_SLIDER,
    SOUNDKEY_TARGET_PROP_POPUP,
    SOUNDKEY_BAKE_KEYFRAMES_BUTTON,
    SOUNDKEY_GROUP_BAKE_END,
    
    SOUNDKEY_NUM_PARAMS
};

// Response Curve Options
enum ResponseCurveType {
    CURVE_LINEAR = 1,
    CURVE_EXPONENTIAL = 2,
    CURVE_LOGARITHMIC = 3,
    CURVE_SCURVE = 4
};

// Output Property Target Options
enum OutputTargetType {
    TARGET_NEW_SLIDERS = 1,
    TARGET_LAYER_OPACITY = 2,
    TARGET_LAYER_SCALE = 3,
    TARGET_LAYER_ROTATION = 4,
    TARGET_LAYER_POSITION_Y = 5
};

// Struktura parametrów pojedynczego kanału audio
struct ChannelConfig {
    bool enabled{true};
    float lowFreq{20.0f};
    float highFreq{150.0f};
    float sensitivity{1.0f};
    float threshold{0.0f};
    float attackMs{10.0f};
    float releaseMs{150.0f};
    ResponseCurveType curve{CURVE_LINEAR};
    float minValue{0.0f};
    float maxValue{100.0f};
    float currentOutput{0.0f};
    bool isPeak{false};
};

// Sequence Data – trwałe dane zapamiętywane w projekcie .aep
struct SoundKeySequenceData {
    uint32_t version{1};
    uint32_t selectedAudioLayerId{0};
    uint32_t channelMode{1}; // 1: Left, 2: Right, 3: Stereo, 4: Diff
    float decimationTolerance{0.4f};
    uint32_t targetProperty{1};
    ChannelConfig channels[4];
};

// Współdzielony, bezpieczny wielowątkowo bufor audio (Multi-Frame Rendering)
struct SoundKeyGlobalData {
    std::shared_mutex audioMutex;
    std::vector<float> pcmBufferMono;
    std::vector<float> pcmBufferLeft;
    std::vector<float> pcmBufferRight;
    uint32_t sampleRate{48000};
    double durationSeconds{0.0};
    bool isAudioLoaded{false};
    uint64_t audioHash{0};
    
    void Clear() {
        std::unique_lock<std::shared_mutex> lock(audioMutex);
        pcmBufferMono.clear();
        pcmBufferLeft.clear();
        pcmBufferRight.clear();
        isAudioLoaded = false;
        durationSeconds = 0.0;
        audioHash = 0;
    }
};

#ifdef __cplusplus
extern "C" {
#endif

// Główny punkt wejścia wtyczki Effect
DllExport PF_Err EffectMain(
    PF_Cmd          cmd,
    PF_InData       *in_data,
    PF_OutData      *out_data,
    PF_ParamDef     *params[],
    PF_LayerDef     *output,
    void            *extra
);

#ifdef __cplusplus
}
#endif

#endif // SOUNDKEY_PLUGIN_H
