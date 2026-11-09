/**
 * @file SoundKey_AudioEngine.h
 * @brief Header-Only High-Performance C++20 Audio DSP & FFT Keyframe Math Engine
 */

#pragma once

#ifndef SOUNDKEY_AUDIO_ENGINE_H
#define SOUNDKEY_AUDIO_ENGINE_H

#include <vector>
#include <cmath>
#include <algorithm>
#include <complex>
#include <cstdint>

namespace SoundKeyDSP {

constexpr double PI = 3.14159265358979323846;

struct KeyframeRecord {
    double timeSec{0.0};
    int64_t frame{0};
    double rawEnergy{0.0};
    double value{0.0};
    bool isPeak{false};
};

/**
 * Szybka Transformata Fouriera (In-Place Radix-2 Cooley-Tukey FFT)
 */
inline void FastFourierTransform(std::vector<std::complex<double>>& a) {
    const size_t n = a.size();
    if (n <= 1) return;

    // Bit-reversal permutation
    for (size_t i = 1, j = 0; i < n; i++) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }

    // Obliczenia motylkowe (Butterfly Computations)
    for (size_t len = 2; len <= n; len <<= 1) {
        double ang = 2.0 * PI / static_cast<double>(len);
        std::complex<double> wlen(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (size_t j = 0; j < len / 2; j++) {
                std::complex<double> u = a[i + j];
                std::complex<double> v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

/**
 * Obliczenie energii wybranego pasma częstotliwości [lowFreq, highFreq] w zadanym czasie
 */
inline double ExtractFrequencyEnergy(
    const std::vector<float>& pcmData,
    uint32_t sampleRate,
    double timeSec,
    float lowFreq,
    float highFreq
) {
    constexpr size_t FFT_SIZE = 2048;
    const size_t halfFFT = FFT_SIZE / 2;

    int64_t centerSample = static_cast<int64_t>(timeSec * sampleRate);
    int64_t startSample = centerSample - static_cast<int64_t>(halfFFT);

    std::vector<std::complex<double>> buffer(FFT_SIZE);

    // Okno czasowe Hann'a
    for (size_t i = 0; i < FFT_SIZE; ++i) {
        int64_t idx = startSample + static_cast<int64_t>(i);
        double sample = (idx >= 0 && idx < static_cast<int64_t>(pcmData.size())) ? pcmData[idx] : 0.0;
        double window = 0.5 * (1.0 - std::cos(2.0 * PI * i / (FFT_SIZE - 1)));
        buffer[i] = std::complex<double>(sample * window, 0.0);
    }

    // Wykonanie FFT
    FastFourierTransform(buffer);

    // Przeliczenie częstotliwości na indeksy binów FFT
    double binWidth = static_cast<double>(sampleRate) / static_cast<double>(FFT_SIZE);
    size_t startBin = std::max<size_t>(1, static_cast<size_t>(lowFreq / binWidth));
    size_t endBin = std::min<size_t>(halfFFT - 1, static_cast<size_t>(highFreq / binWidth));

    if (startBin >= endBin) return 0.0;

    double energySum = 0.0;
    size_t count = 0;

    for (size_t b = startBin; b <= endBin; ++b) {
        double mag = std::abs(buffer[b]);
        energySum += mag * mag;
        count++;
    }

    return count > 0 ? std::sqrt(energySum / count) : 0.0;
}

/**
 * Filtr obwiedni z wykładniczym czasem narastania (Attack) i opadania (Release)
 */
class EnvelopeProcessor {
public:
    double currentEnvelope{0.0};
    double prevRawEnergy{0.0};

    void Reset() {
        currentEnvelope = 0.0;
        prevRawEnergy = 0.0;
    }

    double Process(
        double rawEnergy,
        double dt,
        double attackMs,
        double releaseMs,
        double threshold,
        double sensitivity
    ) {
        // 1. Skalowanie czułości i bramka szumów
        double energy = rawEnergy * sensitivity;
        if (energy < threshold) {
            energy = 0.0;
        } else {
            energy = (energy - threshold) / std::max(0.0001, 1.0 - threshold);
        }
        energy = std::clamp(energy, 0.0, 1.0);

        // 2. Ballistyka wykładnicza
        double attackCoeff = std::exp(-dt / std::max(0.001, attackMs / 1000.0));
        double releaseCoeff = std::exp(-dt / std::max(0.001, releaseMs / 1000.0));

        if (energy > currentEnvelope) {
            currentEnvelope = energy + attackCoeff * (currentEnvelope - energy);
        } else {
            currentEnvelope = energy + releaseCoeff * (currentEnvelope - energy);
        }

        prevRawEnergy = rawEnergy;
        return currentEnvelope;
    }
};

/**
 * Algorytm Ramera-Douglasa-Peuckera do redukcji nadmiarowych klatek kluczowych
 */
inline std::vector<KeyframeRecord> SimplifyCurve(const std::vector<KeyframeRecord>& points, double tolerance) {
    if (points.size() <= 2) return points;

    double maxDist = 0.0;
    size_t maxIndex = 0;
    const auto& start = points.front();
    const auto& end = points.back();

    for (size_t i = 1; i < points.size() - 1; ++i) {
        if (points[i].isPeak) {
            std::vector<KeyframeRecord> left(points.begin(), points.begin() + i + 1);
            std::vector<KeyframeRecord> right(points.begin() + i, points.end());
            auto simLeft = SimplifyCurve(left, tolerance);
            auto simRight = SimplifyCurve(right, tolerance);
            simLeft.pop_back();
            simLeft.insert(simLeft.end(), simRight.begin(), simRight.end());
            return simLeft;
        }

        double num = std::abs((end.value - start.value) * points[i].frame -
                              (end.frame - start.frame) * points[i].value +
                              end.frame * start.value -
                              end.value * start.frame);
        double den = std::sqrt(std::pow(end.value - start.value, 2) + std::pow(end.frame - start.frame, 2));
        double dist = (den == 0.0) ? 0.0 : num / den;

        if (dist > maxDist) {
            maxDist = dist;
            maxIndex = i;
        }
    }

    if (maxDist > tolerance) {
        std::vector<KeyframeRecord> left(points.begin(), points.begin() + maxIndex + 1);
        std::vector<KeyframeRecord> right(points.begin() + maxIndex, points.end());
        auto simLeft = SimplifyCurve(left, tolerance);
        auto simRight = SimplifyCurve(right, tolerance);
        simLeft.pop_back();
        simLeft.insert(simLeft.end(), simRight.begin(), simRight.end());
        return simLeft;
    } else {
        return { start, end };
    }
}

} // namespace SoundKeyDSP

#endif // SOUNDKEY_AUDIO_ENGINE_H
