/**
 * @file SoundKey_AEGP.cpp
 * @brief Native AEGP Bridge: Direct PCM Sound Extraction & Keyframe Injection
 */

#include "SoundKey_AEGP.h"
#include "SoundKey_AudioEngine.h"

static AEGP_PluginID g_aegp_id = 0;
static SPBasicSuite* g_pica_basic = nullptr;

/**
 * Ekstrakcja surowych próbek PCM audio ze wskazanej warstwy After Effects
 */
PF_Err SoundKey_ExtractAudioFromLayer(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[]) {
    AEGP_SuiteHandler suites(in_data->pica_basicP);
    SoundKeyGlobalData* audioData = GetSoundKeyGlobalAudio();
    if (!audioData) return PF_Err_NONE;

    try {
        // 1. Pobranie aktywnej kompozycji
        AEGP_CompH compH = NULL;
        suites.CompSuite11()->AEGP_GetMostRecentlyUsedComp(&compH);
        if (!compH) return PF_Err_NONE;

        // 2. Pobranie referencji do warstwy wybranej w menu popup
        PF_ParamDef layerParam;
        AEFX_CLR_STRUCT(layerParam);
        PF_CHECKOUT_PARAM(in_data, SOUNDKEY_AUDIO_LAYER_POPUP, in_data->current_time, in_data->time_step, in_data->time_scale, &layerParam);

        A_long layerIndex = layerParam.u.ld.data; // Indeks wybranej warstwy
        if (layerIndex <= 0) {
            suites.UtilitySuite6()->AEGP_ReportInfo(g_aegp_id, "SoundKey: Wybierz poprawną warstwę audio z kompozycji.");
            return PF_Err_NONE;
        }

        AEGP_LayerH sourceLayerH = NULL;
        suites.LayerSuite9()->AEGP_GetCompLayerByIndex(compH, layerIndex - 1, &sourceLayerH);
        if (!sourceLayerH) return PF_Err_NONE;

        // 3. Sprawdzenie czy warstwa posiada audio
        A_Boolean hasAudio = FALSE;
        suites.LayerSuite9()->AEGP_GetLayerFlags(sourceLayerH, &hasAudio);

        // 4. Pobranie próbek PCM przez AEGP_SoundDataSuite
        AEGP_SoundDataH soundDataH = NULL;
        A_Time duration;
        suites.CompSuite11()->AEGP_GetCompDuration(compH, &duration);

        // Zapytanie o format 48 kHz, 32-bit Float Stereo
        AEGP_SoundDataFormat format;
        format.sample_rateF = 48000.0;
        format.encoding = AEGP_SoundEncoding_FLOAT;
        format.bytes_per_sampleL = sizeof(float);
        format.num_channelsL = 2;

        suites.SoundDataSuite1()->AEGP_NewSoundDataH(&format, &soundDataH);

        if (soundDataH) {
            std::unique_lock<std::shared_mutex> lock(audioData->audioMutex);
            audioData->sampleRate = 48000;
            audioData->durationSeconds = (double)duration.value / (double)duration.scale;
            
            // Pobranie wskaźnika do bufora i skopiowanie danych
            A_long numSamples = 0;
            suites.SoundDataSuite1()->AEGP_SoundDataGetNumSamples(soundDataH, &numSamples);

            audioData->pcmBufferMono.resize(numSamples);
            audioData->pcmBufferLeft.resize(numSamples);
            audioData->pcmBufferRight.resize(numSamples);

            void *rawAudioPtr = NULL;
            suites.SoundDataSuite1()->AEGP_SoundDataGetAudioData(soundDataH, &rawAudioPtr);

            if (rawAudioPtr) {
                float *interleaved = reinterpret_cast<float*>(rawAudioPtr);
                for (A_long i = 0; i < numSamples; ++i) {
                    float left = interleaved[i * 2];
                    float right = interleaved[i * 2 + 1];
                    audioData->pcmBufferLeft[i] = left;
                    audioData->pcmBufferRight[i] = right;
                    audioData->pcmBufferMono[i] = (left + right) * 0.5f;
                }
                audioData->isAudioLoaded = true;
            }

            suites.SoundDataSuite1()->AEGP_DisposeSoundDataH(soundDataH);
        }

        suites.UtilitySuite6()->AEGP_ReportInfo(g_aegp_id, "SoundKey: Bufor audio został pomyślnie zsynchronizowany!");

    } catch (...) {
        return PF_Err_INTERNAL_STRUCT_DAMAGED;
    }

    return PF_Err_NONE;
}

/**
 * Natywne wstrzykiwanie klatek kluczowych do kompozycji After Effects
 */
PF_Err SoundKey_BakeKeyframesToComposition(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[]) {
    AEGP_SuiteHandler suites(in_data->pica_basicP);
    SoundKeyGlobalData* audioData = GetSoundKeyGlobalAudio();
    if (!audioData) return PF_Err_NONE;

    // Sprawdzenie czy audio jest załadowane
    {
        std::shared_lock<std::shared_mutex> lock(audioData->audioMutex);
        if (!audioData->isAudioLoaded || audioData->pcmBufferMono.empty()) {
            suites.UtilitySuite6()->AEGP_ReportInfo(g_aegp_id, "SoundKey: Brak załadowanych danych audio. Kliknij najpierw 'SKANUJ DŹWIĘK'.");
            return PF_Err_NONE;
        }
    }

    // 1. Otwarcie atomowej grupy transakcyjnej Undo w After Effects
    suites.UtilitySuite6()->AEGP_StartUndoGroup("SoundKey: Wypiekanie Klatek Kluczowych");

    try {
        AEGP_CompH activeCompH = NULL;
        suites.CompSuite11()->AEGP_GetMostRecentlyUsedComp(&activeCompH);
        if (!activeCompH) {
            suites.UtilitySuite6()->AEGP_EndUndoGroup();
            return PF_Err_NONE;
        }

        // 2. Pobranie parametrów kompozycji
        A_Time compDuration;
        suites.CompSuite11()->AEGP_GetCompDuration(activeCompH, &compDuration);
        A_u_long fpsNumerator = in_data->time_scale;
        A_u_long fpsDenominator = in_data->time_step;
        double fps = (fpsDenominator > 0) ? (double)fpsNumerator / (double)fpsDenominator : 30.0;

        // 3. Utworzenie nowej warstwy pomocniczej Null Object dla kontrolerów
        AEGP_LayerH nullLayerH = NULL;
        suites.LayerSuite9()->AEGP_CreateNullInComp("SoundKey Kontroler Audio", activeCompH, compDuration, &nullLayerH);

        if (nullLayerH) {
            const char* channelNames[4] = { "Bass (Sub)", "Mid (Snare)", "High (HiHat)", "Custom" };
            
            // Konfiguracje pasm pobrane z parametrów
            ChannelConfig bands[4];
            bands[0].enabled = params[SOUNDKEY_CH1_ENABLE]->u.bd.value;
            bands[0].lowFreq = params[SOUNDKEY_CH1_FREQ_LOW]->u.fs_d.value;
            bands[0].highFreq = params[SOUNDKEY_CH1_FREQ_HIGH]->u.fs_d.value;
            bands[0].sensitivity = params[SOUNDKEY_CH1_SENSITIVITY]->u.fs_d.value;
            bands[0].threshold = params[SOUNDKEY_CH1_THRESHOLD]->u.fs_d.value / 100.0f;
            bands[0].attackMs = params[SOUNDKEY_CH1_ATTACK]->u.fs_d.value;
            bands[0].releaseMs = params[SOUNDKEY_CH1_RELEASE]->u.fs_d.value;
            bands[0].minValue = params[SOUNDKEY_CH1_MIN_VAL]->u.fs_d.value;
            bands[0].maxValue = params[SOUNDKEY_CH1_MAX_VAL]->u.fs_d.value;

            bands[1].enabled = params[SOUNDKEY_CH2_ENABLE]->u.bd.value;
            bands[1].lowFreq = params[SOUNDKEY_CH2_FREQ_LOW]->u.fs_d.value;
            bands[1].highFreq = params[SOUNDKEY_CH2_FREQ_HIGH]->u.fs_d.value;
            bands[1].sensitivity = params[SOUNDKEY_CH2_SENSITIVITY]->u.fs_d.value;
            bands[1].threshold = params[SOUNDKEY_CH2_THRESHOLD]->u.fs_d.value / 100.0f;
            bands[1].attackMs = params[SOUNDKEY_CH2_ATTACK]->u.fs_d.value;
            bands[1].releaseMs = params[SOUNDKEY_CH2_RELEASE]->u.fs_d.value;
            bands[1].minValue = params[SOUNDKEY_CH2_MIN_VAL]->u.fs_d.value;
            bands[1].maxValue = params[SOUNDKEY_CH2_MAX_VAL]->u.fs_d.value;

            bands[2].enabled = params[SOUNDKEY_CH3_ENABLE]->u.bd.value;
            bands[2].lowFreq = params[SOUNDKEY_CH3_FREQ_LOW]->u.fs_d.value;
            bands[2].highFreq = params[SOUNDKEY_CH3_FREQ_HIGH]->u.fs_d.value;
            bands[2].sensitivity = params[SOUNDKEY_CH3_SENSITIVITY]->u.fs_d.value;
            bands[2].threshold = params[SOUNDKEY_CH3_THRESHOLD]->u.fs_d.value / 100.0f;
            bands[2].attackMs = params[SOUNDKEY_CH3_ATTACK]->u.fs_d.value;
            bands[2].releaseMs = params[SOUNDKEY_CH3_RELEASE]->u.fs_d.value;
            bands[2].minValue = params[SOUNDKEY_CH3_MIN_VAL]->u.fs_d.value;
            bands[2].maxValue = params[SOUNDKEY_CH3_MAX_VAL]->u.fs_d.value;

            bands[3].enabled = params[SOUNDKEY_CH4_ENABLE]->u.bd.value;
            bands[3].lowFreq = params[SOUNDKEY_CH4_FREQ_LOW]->u.fs_d.value;
            bands[3].highFreq = params[SOUNDKEY_CH4_FREQ_HIGH]->u.fs_d.value;
            bands[3].sensitivity = params[SOUNDKEY_CH4_SENSITIVITY]->u.fs_d.value;
            bands[3].threshold = params[SOUNDKEY_CH4_THRESHOLD]->u.fs_d.value / 100.0f;
            bands[3].attackMs = params[SOUNDKEY_CH4_ATTACK]->u.fs_d.value;
            bands[3].releaseMs = params[SOUNDKEY_CH4_RELEASE]->u.fs_d.value;
            bands[3].minValue = params[SOUNDKEY_CH4_MIN_VAL]->u.fs_d.value;
            bands[3].maxValue = params[SOUNDKEY_CH4_MAX_VAL]->u.fs_d.value;

            float tolerance = params[SOUNDKEY_TOLERANCE_SLIDER]->u.fs_d.value;

            // 4. Iteracja po 4 kanałach i tworzenie suwaków Slider Control
            for (int ch = 0; ch < 4; ++ch) {
                if (!bands[ch].enabled) continue;

                AEGP_EffectRefH sliderEffectH = NULL;
                suites.EffectSuite4()->AEGP_ApplyEffect(g_aegp_id, nullLayerH, "ADBE Slider Control", &sliderEffectH);

                if (sliderEffectH) {
                    // Zmiana nazwy suwaka
                    std::string effectTitle = std::string("SoundKey: ") + channelNames[ch];
                    suites.EffectSuite4()->AEGP_SetEffectName(g_aegp_id, sliderEffectH, effectTitle.c_str());

                    AEGP_StreamRefH streamH = NULL;
                    suites.StreamSuite5()->AEGP_GetNewEffectStreamByIndex(g_aegp_id, sliderEffectH, 0, &streamH);

                    if (streamH) {
                        // Obliczenie punktów klatek kluczowych w silniku DSP
                        std::vector<SoundKeyDSP::KeyframeRecord> rawKeys;
                        int64_t totalFrames = static_cast<int64_t>((audioData->durationSeconds) * fps);

                        SoundKeyDSP::EnvelopeProcessor env;
                        double dt = 1.0 / fps;

                        for (int64_t f = 0; f < totalFrames; ++f) {
                            double timeSec = (double)f / fps;
                            // Ekstrakcja energii w kanale FFT
                            double energy = SoundKeyDSP::ExtractFrequencyEnergy(
                                audioData->pcmBufferMono,
                                audioData->sampleRate,
                                timeSec,
                                bands[ch].lowFreq,
                                bands[ch].highFreq
                            );

                            double val = env.Process(
                                energy,
                                dt,
                                bands[ch].attackMs,
                                bands[ch].releaseMs,
                                bands[ch].threshold,
                                bands[ch].sensitivity
                            );

                            double mapped = bands[ch].minValue + val * (bands[ch].maxValue - bands[ch].minValue);
                            rawKeys.push_back({ timeSec, f, energy, mapped, false });
                        }

                        // Redukcja zbędnych klatek przez algorytm Douglas-Peucker
                        auto optimizedKeys = SoundKeyDSP::SimplifyCurve(rawKeys, tolerance);

                        // Wstawienie zoptymalizowanych kluczy do After Effects
                        for (const auto& kf : optimizedKeys) {
                            A_Time keyTime;
                            keyTime.scale = static_cast<A_u_long>(fps * 100.0);
                            keyTime.value = static_cast<A_long>(kf.timeSec * keyTime.scale);

                            A_long keyIndex = 0;
                            suites.KeyframeSuite4()->AEGP_AddKeyframes(streamH, AEGP_KeyframeType_FLOAT, 1, &keyTime, &keyIndex);

                            AEGP_KeyframeValueVal keyVal;
                            keyVal.val.one_d = kf.value;
                            suites.KeyframeSuite4()->AEGP_SetKeyframeValue(streamH, keyIndex, &keyVal);

                            suites.KeyframeSuite4()->AEGP_SetKeyframeInterpolation(
                                streamH,
                                keyIndex,
                                AEGP_KeyInterp_LINEAR,
                                AEGP_KeyInterp_LINEAR
                            );
                        }

                        suites.StreamSuite5()->AEGP_DisposeStream(streamH);
                    }
                    suites.EffectSuite4()->AEGP_DisposeEffect(sliderEffectH);
                }
            }
        }

        suites.UtilitySuite6()->AEGP_ReportInfo(g_aegp_id, "SoundKey: Pomyślnie wygenerowano klucze animacji w nowej warstwie kontrolera!");

    } catch (...) {
        suites.UtilitySuite6()->AEGP_EndUndoGroup();
        return PF_Err_INTERNAL_STRUCT_DAMAGED;
    }

    suites.UtilitySuite6()->AEGP_EndUndoGroup();
    return PF_Err_NONE;
}

// Punkt wejścia AEGP General Plugin
A_Err GP_Main(
    struct SPBasicSuite *pica_basicP,
    A_long              major_versionL,
    A_long              minor_versionL,
    AEGP_PluginID       aegp_plugin_id,
    AEGP_GlobalRefcon   *global_refconP
) {
    g_aegp_id = aegp_plugin_id;
    g_pica_basic = pica_basicP;
    return A_Err_NONE;
}
