#include <jbase.h>
#include <math.h>

float jb_cents_hz(jb_cents_t cents) {
    return powf(2, (float)(cents - JB_A4_MIDI) / JB_SEMIS(12.)) * JB_A4_HZ;
}

static int rand_between(int min, int max) {
    return rand() % (max - min + 1) + min;
}

float fold(float x, float threshold) {
    float sign = 1.0f;
    if (x < 0.0f) sign = -1.0;

    x *= sign;
    if (x > threshold) {
        const float remainder = fmod(x, threshold);
        const int numFolds = (int)floor(x / threshold);

        float y;
        if (numFolds % 2 == 0)
            y = remainder;
        else
            y = threshold - remainder;
        return y * sign;
    }
    return x * sign;
}

float jb_wave_sin(float x, float bias) {
    float samp = sinf(x);

    return fold(samp, 1.0 - bias);
}

float jb_wave_square(float x, float bias) {
    return sinf(x) > bias ? 1.0 : -1.0;
}

float jb_wave_triangle(float x, float bias) {
    return asinf(jb_wave_sin(x, bias));
}

float jb_wave_saw(float x, float bias) {
    (void)bias;

    float s = 2 * (fmod(x, 1) - 0.5);

    return fold(s, bias);
}

float jb_wave_noise(float x, float bias) {
    return jb_wave_sin(x, bias) * (float)rand_between(-100, 100) / 100.f;
}

float jb_osc_sample(jb_osc_t *osc, jb_cents_t note, size_t idx, size_t srate) {
    size_t phase = idx % srate;
    float step = (2 * M_PI * jb_cents_hz(note + osc->detune)) / srate;

    return osc->fn(step * phase, osc->bias) * osc->amp;
}

float jb_chain_sample(jb_osc_link_t *link, jb_cents_t note, size_t idx, size_t srate) {
    if (!link->next) return jb_osc_sample(link->osc, note, idx, srate);

    float mod_samp = jb_chain_sample(link->next, note, idx, srate);

    jb_wave_fn_t fn = link->osc->fn;
    float amp = link->osc->amp;
    jb_cents_t detune = link->osc->detune;
    float bias = link->osc->bias;

    size_t phase = idx % srate;
    float step = (2 * M_PI * jb_cents_hz(note + detune)) / srate;

    switch (link->mod) {
        case JB_MOD_AM:
            return amp * fn(step * phase, bias) * mod_samp;
        case JB_MOD_FM:
            return amp * fn((step * mod_samp) * phase, bias);
        case JB_MOD_PM:
            return amp * fn(step * phase + mod_samp, bias);
        case JB_MOD_BM:
            return amp * fn(step * phase, (mod_samp + 1.0) / 2. - 0.0005);
        default:
            jb_warn("unknown modulation method '%d'", link->mod);
            return 0.0;
    }
}
