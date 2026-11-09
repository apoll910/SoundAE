/**
 * @file SoundKey_Plugin.cpp
 * @brief Adobe After Effects Native C++ Plugin - Core Engine Implementation
 */

#include "SoundKey_Plugin.h"
#include "SoundKey_AudioEngine.h"
#include "SoundKey_AEGP.h"

// Globalny singleton pamięci audio dla MFR
static SoundKeyGlobalData g_SoundKeyAudioData;

// Pobranie wskaźnika do globalnych danych audio (dla modułu AEGP)
SoundKeyGlobalData* GetSoundKeyGlobalAudio() {
    return &g_SoundKeyAudioData;
}

static PF_Err About(PF_InData *in_data, PF_OutData *out_data) {
    PF_SPRINTF(out_data->return_msg, 
        "%s v%d.%d\nProfesjonalna Analiza Audio & Keyframe Generator\nAfter Effects SDK C++20 MFR", 
        PLUGIN_NAME, MAJOR_VERSION, MINOR_VERSION);
    return PF_Err_NONE;
}

static PF_Err GlobalSetup(PF_InData *in_data, PF_OutData *out_data) {
    out_data->my_version = PF_VERSION(MAJOR_VERSION, MINOR_VERSION, BUG_VERSION, STAGE_VERSION, BUILD_VERSION);
    
    // Pełna deklaracja możliwości: 32-bit float, Multi-Frame Rendering, Custom UI
    out_data->out_flags = PF_OutFlag_DEEP_COLOR_AWARE       |
                          PF_OutFlag_CUSTOM_UI              |
                          PF_OutFlag_PIX_INDEPENDENT        |
                          PF_OutFlag_SEND_DO_DIALOG         |
                          PF_OutFlag_USE_OUTPUT_EXTENTS;
                          
    out_data->out_flags2 = PF_OutFlag2_SUPPORTS_SMART_RENDER          |
                           PF_OutFlag2_SUPPORTS_THREADED_RENDERING    |
                           PF_OutFlag2_MUTABLE_RENDER_SEQUENCE_DATA_SLOWER;

    return PF_Err_NONE;
}

static PF_Err ParamsSetup(PF_InData *in_data, PF_OutData *out_data) {
    PF_Err err = PF_Err_NONE;
    PF_ParamDef def;

    // =========================================================================
    // SEKCJA 1: AUDIO SOURCE (Wybór warstwy i kanałów)
    // =========================================================================
    AEFX_CLR_STRUCT(def);
    PF_ADD_TOPIC("1. Źródło Dźwięku (Audio Source)", SOUNDKEY_GROUP_AUDIO_START);

    AEFX_CLR_STRUCT(def);
    PF_ADD_LAYER("Warstwa Audio", PF_LayerDefault_NONE, SOUNDKEY_AUDIO_LAYER_POPUP);

    AEFX_CLR_STRUCT(def);
    PF_ADD_POPUP("Kanał Dźwiękowy", 4, 3, "Lewy|Prawy|Stereo (Suma L+R)|Różnica (L-R)", SOUNDKEY_AUDIO_CHANNEL_POPUP);

    AEFX_CLR_STRUCT(def);
    PF_ADD_BUTTON("Skanuj Audio / Odśwież Cache", "SKANUJ DŹWIĘK", PF_ParamFlag_SUPERVISE, PF_ParamFlag_CANNOT_TIME_VARY, SOUNDKEY_PRESCAN_BUTTON);

    AEFX_CLR_STRUCT(def);
    PF_END_TOPIC(SOUNDKEY_GROUP_AUDIO_END);

    // =========================================================================
    // SEKCJA 2: KANAŁ 1 - BASS (20 - 150 Hz)
    // =========================================================================
    AEFX_CLR_STRUCT(def);
    PF_ADD_TOPIC("2. Kanał 1: BASS (Sub-Kick)", SOUNDKEY_GROUP_CH1_START);

    AEFX_CLR_STRUCT(def);
    PF_ADD_CHECKBOOX("CH1: Aktywny", TRUE, 0, SOUNDKEY_CH1_ENABLE);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH1 Dolne Pasmo (Hz)", 20.0, 500.0, 20.0, 150.0, 20.0, PF_Precision_INTEGER, 0, 0, SOUNDKEY_CH1_FREQ_LOW);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH1 Górne Pasmo (Hz)", 40.0, 1000.0, 40.0, 300.0, 150.0, PF_Precision_INTEGER, 0, 0, SOUNDKEY_CH1_FREQ_HIGH);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH1 Czułość (Sens)", 0.1, 5.0, 0.1, 3.0, 1.4, PF_Precision_HUNDREDTHS, 0, 0, SOUNDKEY_CH1_SENSITIVITY);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH1 Bramka (Threshold %)", 0.0, 100.0, 0.0, 100.0, 8.0, PF_Precision_TENTHS, 0, 0, SOUNDKEY_CH1_THRESHOLD);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH1 Attack (ms)", 0.0, 500.0, 0.0, 200.0, 8.0, PF_Precision_TENTHS, 0, 0, SOUNDKEY_CH1_ATTACK);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH1 Release (ms)", 10.0, 2000.0, 10.0, 1000.0, 140.0, PF_Precision_TENTHS, 0, 0, SOUNDKEY_CH1_RELEASE);

    AEFX_CLR_STRUCT(def);
    PF_ADD_POPUP("CH1 Krzywa Reakcji", 4, 2, "Liniowa|Wykładnicza (Gamma)|Logarytmiczna (dB)|S-Krzywa (Smoothstep)", SOUNDKEY_CH1_CURVE);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH1 Wartość Min", -10000.0, 10000.0, 0.0, 100.0, 0.0, PF_Precision_TENTHS, 0, 0, SOUNDKEY_CH1_MIN_VAL);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH1 Wartość Max", -10000.0, 10000.0, 0.0, 100.0, 100.0, PF_Precision_TENTHS, 0, 0, SOUNDKEY_CH1_MAX_VAL);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH1 Wartość Wyjściowa", -10000.0, 10000.0, 0.0, 100.0, 0.0, PF_Precision_HUNDREDTHS, PF_ParamFlag_CANNOT_TIME_VARY, 0, SOUNDKEY_CH1_OUT_VALUE);

    AEFX_CLR_STRUCT(def);
    PF_END_TOPIC(SOUNDKEY_GROUP_CH1_END);

    // =========================================================================
    // SEKCJA 3: KANAŁ 2 - MID (150 - 2000 Hz)
    // =========================================================================
    AEFX_CLR_STRUCT(def);
    PF_ADD_TOPIC("3. Kanał 2: MID (Snare & Wokal)", SOUNDKEY_GROUP_CH2_START);

    AEFX_CLR_STRUCT(def);
    PF_ADD_CHECKBOOX("CH2: Aktywny", TRUE, 0, SOUNDKEY_CH2_ENABLE);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH2 Dolne Pasmo (Hz)", 100.0, 3000.0, 100.0, 1000.0, 150.0, PF_Precision_INTEGER, 0, 0, SOUNDKEY_CH2_FREQ_LOW);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH2 Górne Pasmo (Hz)", 300.0, 8000.0, 300.0, 3000.0, 2000.0, PF_Precision_INTEGER, 0, 0, SOUNDKEY_CH2_FREQ_HIGH);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH2 Czułość (Sens)", 0.1, 5.0, 0.1, 3.0, 1.2, PF_Precision_HUNDREDTHS, 0, 0, SOUNDKEY_CH2_SENSITIVITY);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH2 Bramka (Threshold %)", 0.0, 100.0, 0.0, 100.0, 5.0, PF_Precision_TENTHS, 0, 0, SOUNDKEY_CH2_THRESHOLD);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH2 Attack (ms)", 0.0, 500.0, 0.0, 200.0, 12.0, PF_Precision_TENTHS, 0, 0, SOUNDKEY_CH2_ATTACK);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH2 Release (ms)", 10.0, 2000.0, 10.0, 1000.0, 220.0, PF_Precision_TENTHS, 0, 0, SOUNDKEY_CH2_RELEASE);

    AEFX_CLR_STRUCT(def);
    PF_ADD_POPUP("CH2 Krzywa Reakcji", 4, 1, "Liniowa|Wykładnicza (Gamma)|Logarytmiczna (dB)|S-Krzywa (Smoothstep)", SOUNDKEY_CH2_CURVE);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH2 Wartość Min", -10000.0, 10000.0, 0.0, 100.0, 0.0, PF_Precision_TENTHS, 0, 0, SOUNDKEY_CH2_MIN_VAL);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH2 Wartość Max", -10000.0, 10000.0, 0.0, 100.0, 100.0, PF_Precision_TENTHS, 0, 0, SOUNDKEY_CH2_MAX_VAL);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH2 Wartość Wyjściowa", -10000.0, 10000.0, 0.0, 100.0, 0.0, PF_Precision_HUNDREDTHS, PF_ParamFlag_CANNOT_TIME_VARY, 0, SOUNDKEY_CH2_OUT_VALUE);

    AEFX_CLR_STRUCT(def);
    PF_END_TOPIC(SOUNDKEY_GROUP_CH2_END);

    // =========================================================================
    // SEKCJA 4: KANAŁ 3 - HIGH (2000 - 12000 Hz)
    // =========================================================================
    AEFX_CLR_STRUCT(def);
    PF_ADD_TOPIC("4. Kanał 3: HIGH (Hi-Hats & Air)", SOUNDKEY_GROUP_CH3_START);

    AEFX_CLR_STRUCT(def);
    PF_ADD_CHECKBOOX("CH3: Aktywny", TRUE, 0, SOUNDKEY_CH3_ENABLE);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH3 Dolne Pasmo (Hz)", 1000.0, 10000.0, 1000.0, 5000.0, 2000.0, PF_Precision_INTEGER, 0, 0, SOUNDKEY_CH3_FREQ_LOW);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH3 Górne Pasmo (Hz)", 3000.0, 20000.0, 3000.0, 16000.0, 12000.0, PF_Precision_INTEGER, 0, 0, SOUNDKEY_CH3_FREQ_HIGH);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH3 Czułość (Sens)", 0.1, 5.0, 0.1, 3.0, 1.1, PF_Precision_HUNDREDTHS, 0, 0, SOUNDKEY_CH3_SENSITIVITY);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH3 Bramka (Threshold %)", 0.0, 100.0, 0.0, 100.0, 4.0, PF_Precision_TENTHS, 0, 0, SOUNDKEY_CH3_THRESHOLD);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH3 Attack (ms)", 0.0, 500.0, 0.0, 200.0, 5.0, PF_Precision_TENTHS, 0, 0, SOUNDKEY_CH3_ATTACK);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH3 Release (ms)", 10.0, 2000.0, 10.0, 1000.0, 110.0, PF_Precision_TENTHS, 0, 0, SOUNDKEY_CH3_RELEASE);

    AEFX_CLR_STRUCT(def);
    PF_ADD_POPUP("CH3 Krzywa Reakcji", 4, 3, "Liniowa|Wykładnicza (Gamma)|Logarytmiczna (dB)|S-Krzywa (Smoothstep)", SOUNDKEY_CH3_CURVE);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH3 Wartość Min", -10000.0, 10000.0, 0.0, 100.0, 0.0, PF_Precision_TENTHS, 0, 0, SOUNDKEY_CH3_MIN_VAL);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH3 Wartość Max", -10000.0, 10000.0, 0.0, 100.0, 100.0, PF_Precision_TENTHS, 0, 0, SOUNDKEY_CH3_MAX_VAL);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH3 Wartość Wyjściowa", -10000.0, 10000.0, 0.0, 100.0, 0.0, PF_Precision_HUNDREDTHS, PF_ParamFlag_CANNOT_TIME_VARY, 0, SOUNDKEY_CH3_OUT_VALUE);

    AEFX_CLR_STRUCT(def);
    PF_END_TOPIC(SOUNDKEY_GROUP_CH3_END);

    // =========================================================================
    // SEKCJA 5: KANAŁ 4 - CUSTOM / TRANSIENTS (20 - 20000 Hz)
    // =========================================================================
    AEFX_CLR_STRUCT(def);
    PF_ADD_TOPIC("5. Kanał 4: CUSTOM (Dowolne Pasmo)", SOUNDKEY_GROUP_CH4_START);

    AEFX_CLR_STRUCT(def);
    PF_ADD_CHECKBOOX("CH4: Aktywny", TRUE, 0, SOUNDKEY_CH4_ENABLE);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH4 Dolne Pasmo (Hz)", 20.0, 15000.0, 20.0, 1000.0, 60.0, PF_Precision_INTEGER, 0, 0, SOUNDKEY_CH4_FREQ_LOW);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH4 Górne Pasmo (Hz)", 40.0, 20000.0, 100.0, 10000.0, 4500.0, PF_Precision_INTEGER, 0, 0, SOUNDKEY_CH4_FREQ_HIGH);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH4 Czułość (Sens)", 0.1, 5.0, 0.1, 3.0, 1.8, PF_Precision_HUNDREDTHS, 0, 0, SOUNDKEY_CH4_SENSITIVITY);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH4 Bramka (Threshold %)", 0.0, 100.0, 0.0, 100.0, 12.0, PF_Precision_TENTHS, 0, 0, SOUNDKEY_CH4_THRESHOLD);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH4 Attack (ms)", 0.0, 500.0, 0.0, 200.0, 4.0, PF_Precision_TENTHS, 0, 0, SOUNDKEY_CH4_ATTACK);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH4 Release (ms)", 10.0, 2000.0, 10.0, 1000.0, 80.0, PF_Precision_TENTHS, 0, 0, SOUNDKEY_CH4_RELEASE);

    AEFX_CLR_STRUCT(def);
    PF_ADD_POPUP("CH4 Krzywa Reakcji", 4, 4, "Liniowa|Wykładnicza (Gamma)|Logarytmiczna (dB)|S-Krzywa (Smoothstep)", SOUNDKEY_CH4_CURVE);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH4 Wartość Min", -10000.0, 10000.0, 0.0, 100.0, 0.0, PF_Precision_TENTHS, 0, 0, SOUNDKEY_CH4_MIN_VAL);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH4 Wartość Max", -10000.0, 10000.0, 0.0, 100.0, 100.0, PF_Precision_TENTHS, 0, 0, SOUNDKEY_CH4_MAX_VAL);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("CH4 Wartość Wyjściowa", -10000.0, 10000.0, 0.0, 100.0, 0.0, PF_Precision_HUNDREDTHS, PF_ParamFlag_CANNOT_TIME_VARY, 0, SOUNDKEY_CH4_OUT_VALUE);

    AEFX_CLR_STRUCT(def);
    PF_END_TOPIC(SOUNDKEY_GROUP_CH4_END);

    // =========================================================================
    // SEKCJA 6: GENERATOR KLUCZY & AKCJE
    // =========================================================================
    AEFX_CLR_STRUCT(def);
    PF_ADD_TOPIC("6. Generator Klatek Kluczowych (Bake)", SOUNDKEY_GROUP_BAKE_START);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Redukcja Kluczy (Tolerancja)", 0.01, 5.0, 0.05, 2.0, 0.4, PF_Precision_HUNDREDTHS, 0, 0, SOUNDKEY_TOLERANCE_SLIDER);

    AEFX_CLR_STRUCT(def);
    PF_ADD_POPUP("Cel Kluczy Animacji", 5, 1, "Nowe Suwaki (Slider Controls)|Krycie Warstwy (Opacity)|Skala (Scale)|Obrót (Rotation)|Pozycja Y (Position Y)", SOUNDKEY_TARGET_PROP_POPUP);

    AEFX_CLR_STRUCT(def);
    PF_ADD_BUTTON("Wypiecz Keyframes do Kompozycji", "WYPIECZ KLUCZE", PF_ParamFlag_SUPERVISE, PF_ParamFlag_CANNOT_TIME_VARY, SOUNDKEY_BAKE_KEYFRAMES_BUTTON);

    AEFX_CLR_STRUCT(def);
    PF_END_TOPIC(SOUNDKEY_GROUP_BAKE_END);

    out_data->num_params = SOUNDKEY_NUM_PARAMS;
    return err;
}

// Inicjalizacja Sequence Data
static PF_Err SequenceSetup(PF_InData *in_data, PF_OutData *out_data) {
    PF_Handle seqDataH = in_data->utils->new_handle(sizeof(SoundKeySequenceData));
    if (!seqDataH) return PF_Err_OUT_OF_MEMORY;

    SoundKeySequenceData *seqData = reinterpret_cast<SoundKeySequenceData*>(*seqDataH);
    seqData->version = 1;
    seqData->decimationTolerance = 0.4f;
    seqData->channelMode = 3; // Stereo

    out_data->sequence_data = seqDataH;
    return PF_Err_NONE;
}

// Spłaszczenie Sequence Data do zapisu w pliku projektu
static PF_Err SequenceFlatten(PF_InData *in_data, PF_OutData *out_data) {
    if (!in_data->sequence_data) return PF_Err_NONE;
    out_data->sequence_data = in_data->sequence_data;
    return PF_Err_NONE;
}

// Obsługa kliknięć przycisków w panelu Effect Controls
static PF_Err UserChangedParam(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[], PF_UserChangedParamExtra *extra) {
    PF_Err err = PF_Err_NONE;

    if (extra->param_index == SOUNDKEY_BAKE_KEYFRAMES_BUTTON) {
        // Wywołanie procedury wstrzykiwania klatek kluczowych w AEGP
        err = SoundKey_BakeKeyframesToComposition(in_data, out_data, params);
    } else if (extra->param_index == SOUNDKEY_PRESCAN_BUTTON || extra->param_index == SOUNDKEY_AUDIO_LAYER_POPUP) {
        // Ponowne załadowanie próbek audio ze wskazanej warstwy
        err = SoundKey_ExtractAudioFromLayer(in_data, out_data, params);
    }

    return err;
}

// Multi-Frame Rendering Pre-Render: deklaracja zależności klatki
static PF_Err SmartPreRender(PF_InData *in_data, PF_OutData *out_data, PF_PreRenderExtra *extra) {
    PF_Err err = PF_Err_NONE;
    PF_RenderRequest req = extra->input->default_req;
    PF_CheckoutResult in_result;

    // Pobranie wejściowej klatki wideo
    err = extra->cb->checkout_layer(in_data->effect_ref, SOUNDKEY_INPUT, SOUNDKEY_INPUT, &req, in_data->current_time, in_data->time_step, in_data->time_scale, &in_result);
    return err;
}

// Smart Render: obliczenie bieżącej wartości i przekazanie klatki obrazu
static PF_Err SmartRender(PF_InData *in_data, PF_OutData *out_data, PF_SmartRenderExtra *extra) {
    PF_Err err = PF_Err_NONE;
    PF_EffectWorld *input_worldP = nullptr;
    PF_EffectWorld *output_worldP = nullptr;

    err = extra->cb->checkout_layer_pixels(in_data->effect_ref, SOUNDKEY_INPUT, &input_worldP);
    err = extra->cb->checkout_output(in_data->effect_ref, &output_worldP);

    if (input_worldP && output_worldP) {
        // Przepisanie bufora wejściowego do wyjściowego (Pass-Through dla obrazu)
        PF_COPY(input_worldP, output_worldP, nullptr, nullptr);
    }

    return err;
}

DllExport PF_Err EffectMain(
    PF_Cmd          cmd,
    PF_InData       *in_data,
    PF_OutData      *out_data,
    PF_ParamDef     *params[],
    PF_LayerDef     *output,
    void            *extra
) {
    PF_Err err = PF_Err_NONE;

    try {
        switch (cmd) {
            case PF_Cmd_ABOUT:
                err = About(in_data, out_data);
                break;
            case PF_Cmd_GLOBAL_SETUP:
                err = GlobalSetup(in_data, out_data);
                break;
            case PF_Cmd_PARAMS_SETUP:
                err = ParamsSetup(in_data, out_data);
                break;
            case PF_Cmd_SEQUENCE_SETUP:
                err = SequenceSetup(in_data, out_data);
                break;
            case PF_Cmd_SEQUENCE_FLATTEN:
                err = SequenceFlatten(in_data, out_data);
                break;
            case PF_Cmd_USER_CHANGED_PARAM:
                err = UserChangedParam(in_data, out_data, params, (PF_UserChangedParamExtra*)extra);
                break;
            case PF_Cmd_SMART_PRE_RENDER:
                err = SmartPreRender(in_data, out_data, (PF_PreRenderExtra*)extra);
                break;
            case PF_Cmd_SMART_RENDER:
                err = SmartRender(in_data, out_data, (PF_SmartRenderExtra*)extra);
                break;
            default:
                break;
        }
    } catch (const std::exception& e) {
        err = PF_Err_INTERNAL_STRUCT_DAMAGED;
    }

    return err;
}
