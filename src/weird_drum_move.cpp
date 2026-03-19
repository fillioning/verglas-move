// Move-Anything WeirdDrum — drum synthesizer based on dfilaretti/WeirdDrums (MIT)
//
// Analog-style drum synthesis: oscillator + filtered noise + distortion.
// Wraps into Move-Anything audio_fx_api_v2 interface with MIDI triggering.
// Audio: 44100 Hz, 128 frames/block, stereo interleaved int16.

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>

#include "weird_drum/voice.h"

// ---- Move-Anything API headers (must match chain_host ABI exactly) ----
extern "C" {

typedef int (*move_mod_emit_value_fn)(void *ctx,
                                      const char *source_id,
                                      const char *target,
                                      const char *param,
                                      float signal,
                                      float depth,
                                      float offset,
                                      int bipolar,
                                      int enabled);
typedef void (*move_mod_clear_source_fn)(void *ctx, const char *source_id);

typedef struct host_api_v1 {
    uint32_t api_version;
    int sample_rate;
    int frames_per_block;
    uint8_t *mapped_memory;
    int audio_out_offset;
    int audio_in_offset;
    void (*log)(const char *msg);
    int (*midi_send_internal)(const uint8_t *msg, int len);
    int (*midi_send_external)(const uint8_t *msg, int len);
    int (*get_clock_status)(void);
    move_mod_emit_value_fn mod_emit_value;
    move_mod_clear_source_fn mod_clear_source;
    void *mod_host_ctx;
} host_api_v1_t;

typedef struct audio_fx_api_v2 {
    uint32_t api_version;
    void* (*create_instance)(const char *module_dir, const char *config_json);
    void  (*destroy_instance)(void *instance);
    void  (*process_block)(void *instance, int16_t *audio_inout, int frames);
    void  (*set_param)(void *instance, const char *key, const char *val);
    int   (*get_param)(void *instance, const char *key, char *buf, int buf_len);
    void  (*on_midi)(void *instance, const uint8_t *msg, int len, int source);
} audio_fx_api_v2_t;

}  // extern "C"

// ---- Constants ----
static const int kNumVoices = 8;
static const float kSampleRate = 44100.0f;

// ---- Instance state ----
struct WDInstance {
    weird_drum::Voice voices[kNumVoices];
    weird_drum::DrumParams params;
    int next_voice;  // round-robin allocation
    int preset;      // 0=Custom, 1=Kick, ..., 7=Cymbal

    // Page tracking (0=Tone, 1=Noise/Master)
    int current_page;
};

static const host_api_v1_t *g_host = NULL;

// ---- Helpers ----
static inline float clampf(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

static inline int clampi(int x, int lo, int hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

// ---- Knob definitions ----
static const char *WAVE_NAMES[] = { "Sine", "Saw", "Square" };
static const char *FILTER_NAMES[] = { "LP", "HP", "BP" };

// ---- Preset definitions ----
enum Preset {
    PRESET_CUSTOM = 0,
    PRESET_KICK,
    PRESET_SNARE,
    PRESET_TOM,
    PRESET_CLAP,
    PRESET_RIMSHOT,
    PRESET_HIHAT,
    PRESET_CYMBAL,
    PRESET_COUNT
};

static const char *PRESET_NAMES[] = {
    "Custom", "Kick", "Snare", "Tom", "Clap", "Rimshot", "Hi Hat", "Cymbal"
};

//                                   freq    atk     dec    wave  pEA   pER    pLA   pLR   fT  fC      fR   nA     nD    mix  dist  lvl
static const weird_drum::DrumParams PRESETS[] = {
    // Custom — default init values
    { 55.0f,  0.001f, 0.5f,  0, 0.0f,  0.1f,   0.0f, 0.45f, 0, 400.0f,  1.0f, 0.01f,  0.4f,  0.5f, 0.0f, 0.0f },
    // Kick — deep sine, strong pitch drop, minimal noise
    { 55.0f,  0.001f, 0.4f,  0, 0.8f,  0.05f,  0.0f, 0.45f, 0, 400.0f,  1.0f, 0.001f, 0.05f, 0.05f, 2.0f, 0.0f },
    // Snare — mid body + noise burst, bandpass filter
    { 180.0f, 0.001f, 0.15f, 0, 0.3f,  0.03f,  0.0f, 0.45f, 2, 3000.0f, 1.2f, 0.001f, 0.15f, 0.6f,  3.0f, 0.0f },
    // Tom — mid-low sine, pitch env, light noise
    { 100.0f, 0.001f, 0.3f,  0, 0.5f,  0.04f,  0.0f, 0.45f, 0, 800.0f,  1.0f, 0.001f, 0.08f, 0.15f, 1.0f, 0.0f },
    // Clap — noise-heavy, bandpass, snappy
    { 800.0f, 0.001f, 0.01f, 2, 0.0f,  0.1f,   0.0f, 0.45f, 2, 1500.0f, 1.5f, 0.001f, 0.2f,  0.9f,  5.0f, 0.0f },
    // Rimshot — bright short click, square + noise
    { 500.0f, 0.001f, 0.04f, 2, 0.2f,  0.01f,  0.0f, 0.45f, 2, 3500.0f, 2.0f, 0.001f, 0.03f, 0.4f,  4.0f, 0.0f },
    // Hi Hat — short noise, highpass
    { 400.0f, 0.001f, 0.005f,2, 0.0f,  0.1f,   0.0f, 0.45f, 1, 7000.0f, 1.5f, 0.001f, 0.08f, 0.95f, 0.0f, 0.0f },
    // Cymbal — long noise, highpass, shimmer
    { 300.0f, 0.001f, 0.01f, 2, 0.0f,  0.1f,   0.05f,5.0f,  1, 5000.0f, 2.0f, 0.001f, 0.8f,  0.9f,  0.0f, 0.0f },
};

static void apply_preset(WDInstance *inst, int preset_idx) {
    if (preset_idx < 0 || preset_idx >= PRESET_COUNT) return;
    if (preset_idx == PRESET_CUSTOM) return;  // "Custom" = no change
    inst->params = PRESETS[preset_idx];
    inst->params.level = 0.0f;  // always keep user's level
}

struct KnobDef {
    const char *key;
    const char *label;
    float min, max, step;
    bool is_int;
};

// Page 0 (Tone) knobs
static const KnobDef TONE_KNOBS[8] = {
    { "freq",           "Freq",       20,    2000, 1,     false },
    { "attack",         "Attack",     0.001f, 1.0f, 0.001f, false },
    { "decay",          "Decay",      0.001f, 2.0f, 0.01f, false },
    { "wave_type",      "Wave",       0,     2,    1,     true  },
    { "pitch_env_amt",  "P.Env Amt",  0,     1,    0.01f, false },
    { "pitch_env_rate", "P.Env Rate", 0.001f, 1.0f, 0.001f, false },
    { "pitch_lfo_amt",  "LFO Amt",    0,     1,    0.01f, false },
    { "pitch_lfo_rate", "LFO Rate",   0.1f,  80.0f, 0.1f, false },
};

// Page 1 (Noise/Master) knobs
static const KnobDef NOISE_KNOBS[8] = {
    { "filter_type",    "Filter",     0,     2,     1,     true  },
    { "filter_cutoff",  "Cutoff",     20,    18000, 10,    false },
    { "filter_res",     "Reso",       0.1f,  5.0f,  0.1f,  false },
    { "noise_attack",   "N.Attack",   0.001f, 1.0f, 0.001f, false },
    { "noise_decay",    "N.Decay",    0.001f, 1.0f, 0.01f, false },
    { "mix",            "Mix",        0,     1,     0.01f, false },
    { "distortion",     "Distort",    0,     50,    0.5f,  false },
    { "level",          "Level",      -96,   24,    0.5f,  false },
};

// ---- Parameter accessor helpers ----
static float* wd_param_ptr(WDInstance *inst, const char *key) {
    weird_drum::DrumParams &p = inst->params;
    if (strcmp(key, "freq") == 0)           return &p.freq;
    if (strcmp(key, "attack") == 0)         return &p.attack;
    if (strcmp(key, "decay") == 0)          return &p.decay;
    if (strcmp(key, "pitch_env_amt") == 0)  return &p.pitch_env_amt;
    if (strcmp(key, "pitch_env_rate") == 0) return &p.pitch_env_rate;
    if (strcmp(key, "pitch_lfo_amt") == 0)  return &p.pitch_lfo_amt;
    if (strcmp(key, "pitch_lfo_rate") == 0) return &p.pitch_lfo_rate;
    if (strcmp(key, "filter_cutoff") == 0)  return &p.filter_cutoff;
    if (strcmp(key, "filter_res") == 0)     return &p.filter_res;
    if (strcmp(key, "noise_attack") == 0)   return &p.noise_attack;
    if (strcmp(key, "noise_decay") == 0)    return &p.noise_decay;
    if (strcmp(key, "mix") == 0)            return &p.mix;
    if (strcmp(key, "distortion") == 0)     return &p.distortion;
    if (strcmp(key, "level") == 0)          return &p.level;
    return NULL;
}

static int* wd_int_param_ptr(WDInstance *inst, const char *key) {
    weird_drum::DrumParams &p = inst->params;
    if (strcmp(key, "wave_type") == 0)   return &p.wave_type;
    if (strcmp(key, "filter_type") == 0) return &p.filter_type;
    return NULL;
}

static const KnobDef* find_knob_def(const char *key) {
    for (int i = 0; i < 8; i++) {
        if (strcmp(TONE_KNOBS[i].key, key) == 0) return &TONE_KNOBS[i];
        if (strcmp(NOISE_KNOBS[i].key, key) == 0) return &NOISE_KNOBS[i];
    }
    return NULL;
}

// ---- API callbacks ----
static void* wd_create(const char *module_dir, const char *config_json) {
    WDInstance *inst = (WDInstance*)calloc(1, sizeof(WDInstance));
    if (!inst) return NULL;

    // Init voices with different noise seeds
    for (int i = 0; i < kNumVoices; i++) {
        inst->voices[i].init(kSampleRate, 0x12345678 + i * 0x9ABCDEF);
    }

    // Default parameters (matching WeirdDrums)
    inst->params.freq           = 55.0f;
    inst->params.attack         = 0.001f;
    inst->params.decay          = 0.5f;
    inst->params.wave_type      = 0;
    inst->params.pitch_env_amt  = 0.0f;
    inst->params.pitch_env_rate = 0.1f;
    inst->params.pitch_lfo_amt  = 0.0f;
    inst->params.pitch_lfo_rate = 0.45f;
    inst->params.filter_type    = 0;
    inst->params.filter_cutoff  = 400.0f;
    inst->params.filter_res     = 1.0f;
    inst->params.noise_attack   = 0.01f;
    inst->params.noise_decay    = 0.4f;
    inst->params.mix            = 0.5f;
    inst->params.distortion     = 0.0f;
    inst->params.level          = 0.0f;

    inst->next_voice = 0;
    inst->current_page = 0;

    if (g_host && g_host->log) g_host->log("[weird_drum] instance created");
    return inst;
}

static void wd_destroy(void *instance) {
    if (instance) free(instance);
}

static void wd_process(void *instance, int16_t *audio_inout, int frames) {
    WDInstance *inst = (WDInstance*)instance;
    if (!inst || frames <= 0) return;
    if (frames > 128) frames = 128;

    for (int i = 0; i < frames; i++) {
        float sample = 0.0f;

        // Sum all active voices
        for (int v = 0; v < kNumVoices; v++) {
            if (inst->voices[v].is_active()) {
                sample += inst->voices[v].render();
            }
        }

        // Soft clip to prevent overflow
        if (sample > 1.5f) sample = 1.5f;
        if (sample < -1.5f) sample = -1.5f;
        sample = sample - (sample * sample * sample) * 0.1481f;  // gentle cubic

        // Convert to int16 stereo (same signal both channels)
        int32_t s = (int32_t)(sample * 32767.0f);
        if (s > 32767) s = 32767;
        if (s < -32768) s = -32768;
        audio_inout[i * 2]     = (int16_t)s;
        audio_inout[i * 2 + 1] = (int16_t)s;
    }
}

static void wd_set_param(void *instance, const char *key, const char *val) {
    WDInstance *inst = (WDInstance*)instance;
    if (!inst || !key || !val) return;

    // Page tracking
    if (strcmp(key, "_level") == 0) {
        if (strcmp(val, "Noise/Master") == 0) inst->current_page = 1;
        else inst->current_page = 0;
        return;
    }

    // Preset parameter — loads all params from preset template
    if (strcmp(key, "preset") == 0) {
        int found = 0;
        for (int i = 0; i < PRESET_COUNT; i++) {
            if (strcmp(val, PRESET_NAMES[i]) == 0) { inst->preset = i; found = 1; break; }
        }
        if (!found) inst->preset = clampi(atoi(val), 0, PRESET_COUNT - 1);
        apply_preset(inst, inst->preset);
        return;
    }

    // Enum parameters
    if (strcmp(key, "wave_type") == 0) {
        int found = 0;
        for (int i = 0; i < 3; i++) {
            if (strcmp(val, WAVE_NAMES[i]) == 0) { inst->params.wave_type = i; found = 1; break; }
        }
        if (!found) inst->params.wave_type = clampi(atoi(val), 0, 2);
        return;
    }
    if (strcmp(key, "filter_type") == 0) {
        int found = 0;
        for (int i = 0; i < 3; i++) {
            if (strcmp(val, FILTER_NAMES[i]) == 0) { inst->params.filter_type = i; found = 1; break; }
        }
        if (!found) inst->params.filter_type = clampi(atoi(val), 0, 2);
        return;
    }

    // Float parameters
    float *fp = wd_param_ptr(inst, key);
    if (fp) {
        const KnobDef *def = find_knob_def(key);
        float v = (float)atof(val);
        if (def) v = clampf(v, def->min, def->max);
        *fp = v;
        return;
    }

    // Knob adjust (relative CC from Shadow UI)
    if (strncmp(key, "knob_", 5) == 0 && strstr(key, "_adjust")) {
        int knob_num = atoi(key + 5);  // 1-indexed
        int delta = atoi(val);
        const KnobDef *knobs = (inst->current_page == 1) ? NOISE_KNOBS : TONE_KNOBS;
        int idx = knob_num - 1;
        if (idx >= 0 && idx < 8) {
            const KnobDef &k = knobs[idx];
            int *ip = wd_int_param_ptr(inst, k.key);
            if (ip) {
                *ip = clampi(*ip + delta * (int)k.step, (int)k.min, (int)k.max);
            } else {
                float *fpp = wd_param_ptr(inst, k.key);
                if (fpp) *fpp = clampf(*fpp + delta * k.step, k.min, k.max);
            }
        }
        return;
    }

    // JSON state restore
    if (strcmp(key, "state") == 0) {
        float freq = 55, atk = 0.001f, dec = 0.5f;
        int wt = 0;
        float pea = 0, per = 0.1f, pla = 0, plr = 0.45f;
        int ft = 0;
        float fc = 400, fr = 1, na = 0.01f, nd = 0.4f;
        float mx = 0.5f, dist = 0, lvl = 0;
        int prs = 0;

        sscanf(val, "{\"freq\":%f,\"attack\":%f,\"decay\":%f,"
               "\"wave_type\":%d,\"pitch_env_amt\":%f,\"pitch_env_rate\":%f,"
               "\"pitch_lfo_amt\":%f,\"pitch_lfo_rate\":%f,"
               "\"filter_type\":%d,\"filter_cutoff\":%f,\"filter_res\":%f,"
               "\"noise_attack\":%f,\"noise_decay\":%f,"
               "\"mix\":%f,\"distortion\":%f,\"level\":%f,"
               "\"preset\":%d}",
               &freq, &atk, &dec, &wt, &pea, &per, &pla, &plr,
               &ft, &fc, &fr, &na, &nd, &mx, &dist, &lvl, &prs);

        inst->params.freq           = clampf(freq, 20, 2000);
        inst->params.attack         = clampf(atk, 0.001f, 1.0f);
        inst->params.decay          = clampf(dec, 0.001f, 2.0f);
        inst->params.wave_type      = clampi(wt, 0, 2);
        inst->params.pitch_env_amt  = clampf(pea, 0, 1);
        inst->params.pitch_env_rate = clampf(per, 0.001f, 1.0f);
        inst->params.pitch_lfo_amt  = clampf(pla, 0, 1);
        inst->params.pitch_lfo_rate = clampf(plr, 0.1f, 80.0f);
        inst->params.filter_type    = clampi(ft, 0, 2);
        inst->params.filter_cutoff  = clampf(fc, 20, 18000);
        inst->params.filter_res     = clampf(fr, 0.1f, 5.0f);
        inst->params.noise_attack   = clampf(na, 0.001f, 1.0f);
        inst->params.noise_decay    = clampf(nd, 0.001f, 1.0f);
        inst->params.mix            = clampf(mx, 0, 1);
        inst->params.distortion     = clampf(dist, 0, 50);
        inst->params.level          = clampf(lvl, -96, 24);
        inst->preset                = clampi(prs, 0, PRESET_COUNT - 1);
    }
}

static int wd_get_param(void *instance, const char *key, char *buf, int buf_len) {
    WDInstance *inst = (WDInstance*)instance;
    if (!inst || !key || !buf || buf_len < 1) return 0;

    if (strcmp(key, "name") == 0) {
        return snprintf(buf, buf_len, "Weird Drum");
    }

    if (strcmp(key, "ui_hierarchy") == 0) {
        static const char *hier =
        "{\"modes\":null,"
        "\"levels\":{"
          "\"root\":{\"name\":\"Weird Drum\","
            "\"knobs\":[\"freq\",\"attack\",\"decay\",\"wave_type\",\"pitch_env_amt\",\"pitch_env_rate\",\"pitch_lfo_amt\",\"pitch_lfo_rate\"],"
            "\"params\":["
              "{\"level\":\"Tone\",\"label\":\"Tone\"},"
              "{\"level\":\"Noise/Master\",\"label\":\"Noise/Master\"}"
            "]},"
          "\"Tone\":{\"label\":\"Tone\","
            "\"knobs\":[\"freq\",\"attack\",\"decay\",\"wave_type\",\"pitch_env_amt\",\"pitch_env_rate\",\"pitch_lfo_amt\",\"pitch_lfo_rate\"],"
            "\"params\":[\"freq\",\"attack\",\"decay\",\"wave_type\",\"pitch_env_amt\",\"pitch_env_rate\",\"pitch_lfo_amt\",\"pitch_lfo_rate\",\"preset\"]},"
          "\"Noise/Master\":{\"label\":\"Noise/Master\","
            "\"knobs\":[\"filter_type\",\"filter_cutoff\",\"filter_res\",\"noise_attack\",\"noise_decay\",\"mix\",\"distortion\",\"level\"],"
            "\"params\":[\"filter_type\",\"filter_cutoff\",\"filter_res\",\"noise_attack\",\"noise_decay\",\"mix\",\"distortion\",\"level\"]}"
        "}}";
        int len = (int)strlen(hier);
        if (len >= buf_len) return -1;
        memcpy(buf, hier, len + 1);
        return len;
    }

    // Preset getter
    if (strcmp(key, "preset") == 0)
        return snprintf(buf, buf_len, "%s", PRESET_NAMES[clampi(inst->preset, 0, PRESET_COUNT - 1)]);

    // Simple parameter getters
    if (strcmp(key, "freq") == 0)           return snprintf(buf, buf_len, "%.1f", inst->params.freq);
    if (strcmp(key, "attack") == 0)         return snprintf(buf, buf_len, "%.4f", inst->params.attack);
    if (strcmp(key, "decay") == 0)          return snprintf(buf, buf_len, "%.4f", inst->params.decay);
    if (strcmp(key, "wave_type") == 0)      return snprintf(buf, buf_len, "%s", WAVE_NAMES[clampi(inst->params.wave_type, 0, 2)]);
    if (strcmp(key, "pitch_env_amt") == 0)  return snprintf(buf, buf_len, "%.4f", inst->params.pitch_env_amt);
    if (strcmp(key, "pitch_env_rate") == 0) return snprintf(buf, buf_len, "%.4f", inst->params.pitch_env_rate);
    if (strcmp(key, "pitch_lfo_amt") == 0)  return snprintf(buf, buf_len, "%.4f", inst->params.pitch_lfo_amt);
    if (strcmp(key, "pitch_lfo_rate") == 0) return snprintf(buf, buf_len, "%.4f", inst->params.pitch_lfo_rate);
    if (strcmp(key, "filter_type") == 0)    return snprintf(buf, buf_len, "%s", FILTER_NAMES[clampi(inst->params.filter_type, 0, 2)]);
    if (strcmp(key, "filter_cutoff") == 0)  return snprintf(buf, buf_len, "%.1f", inst->params.filter_cutoff);
    if (strcmp(key, "filter_res") == 0)     return snprintf(buf, buf_len, "%.1f", inst->params.filter_res);
    if (strcmp(key, "noise_attack") == 0)   return snprintf(buf, buf_len, "%.4f", inst->params.noise_attack);
    if (strcmp(key, "noise_decay") == 0)    return snprintf(buf, buf_len, "%.4f", inst->params.noise_decay);
    if (strcmp(key, "mix") == 0)            return snprintf(buf, buf_len, "%.4f", inst->params.mix);
    if (strcmp(key, "distortion") == 0)     return snprintf(buf, buf_len, "%.1f", inst->params.distortion);
    if (strcmp(key, "level") == 0)          return snprintf(buf, buf_len, "%.1f", inst->params.level);

    // Knob name/value overlay
    if (strncmp(key, "knob_", 5) == 0 && strstr(key, "_name")) {
        int knob_num = atoi(key + 5);
        int idx = knob_num - 1;
        if (idx >= 0 && idx < 8) {
            const KnobDef *knobs = (inst->current_page == 1) ? NOISE_KNOBS : TONE_KNOBS;
            return snprintf(buf, buf_len, "%s", knobs[idx].label);
        }
        return 0;
    }

    if (strncmp(key, "knob_", 5) == 0 && strstr(key, "_value")) {
        int knob_num = atoi(key + 5);
        int idx = knob_num - 1;
        if (idx < 0 || idx >= 8) return 0;

        const weird_drum::DrumParams &p = inst->params;
        if (inst->current_page == 1) {
            switch (idx) {
                case 0: return snprintf(buf, buf_len, "%s", FILTER_NAMES[clampi(p.filter_type, 0, 2)]);
                case 1: return snprintf(buf, buf_len, "%.0f Hz", p.filter_cutoff);
                case 2: return snprintf(buf, buf_len, "%.1f", p.filter_res);
                case 3: return snprintf(buf, buf_len, "%.0f ms", p.noise_attack * 1000);
                case 4: return snprintf(buf, buf_len, "%.0f ms", p.noise_decay * 1000);
                case 5: return snprintf(buf, buf_len, "%d%%", (int)(p.mix * 100));
                case 6: return snprintf(buf, buf_len, "%.1f", p.distortion);
                case 7: return snprintf(buf, buf_len, "%+.1f dB", p.level);
            }
        } else {
            switch (idx) {
                case 0: return snprintf(buf, buf_len, "%.0f Hz", p.freq);
                case 1: return snprintf(buf, buf_len, "%.0f ms", p.attack * 1000);
                case 2: return snprintf(buf, buf_len, "%.0f ms", p.decay * 1000);
                case 3: return snprintf(buf, buf_len, "%s", WAVE_NAMES[clampi(p.wave_type, 0, 2)]);
                case 4: return snprintf(buf, buf_len, "%d%%", (int)(p.pitch_env_amt * 100));
                case 5: return snprintf(buf, buf_len, "%.0f ms", p.pitch_env_rate * 1000);
                case 6: return snprintf(buf, buf_len, "%d%%", (int)(p.pitch_lfo_amt * 100));
                case 7: return snprintf(buf, buf_len, "%.1f Hz", p.pitch_lfo_rate);
            }
        }
        return 0;
    }

    // chain_params
    if (strcmp(key, "chain_params") == 0) {
        static const char *cp =
        "["
        "{\"key\":\"freq\",\"name\":\"Freq\",\"type\":\"float\",\"min\":20,\"max\":2000,\"step\":1},"
        "{\"key\":\"attack\",\"name\":\"Attack\",\"type\":\"float\",\"min\":0.001,\"max\":1,\"step\":0.001},"
        "{\"key\":\"decay\",\"name\":\"Decay\",\"type\":\"float\",\"min\":0.001,\"max\":2,\"step\":0.01},"
        "{\"key\":\"wave_type\",\"name\":\"Wave\",\"type\":\"enum\",\"options\":[\"Sine\",\"Saw\",\"Square\"]},"
        "{\"key\":\"pitch_env_amt\",\"name\":\"P.Env Amt\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
        "{\"key\":\"pitch_env_rate\",\"name\":\"P.Env Rate\",\"type\":\"float\",\"min\":0.001,\"max\":1,\"step\":0.001},"
        "{\"key\":\"pitch_lfo_amt\",\"name\":\"LFO Amt\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
        "{\"key\":\"pitch_lfo_rate\",\"name\":\"LFO Rate\",\"type\":\"float\",\"min\":0.1,\"max\":80,\"step\":0.1},"
        "{\"key\":\"filter_type\",\"name\":\"Filter\",\"type\":\"enum\",\"options\":[\"LP\",\"HP\",\"BP\"]},"
        "{\"key\":\"filter_cutoff\",\"name\":\"Cutoff\",\"type\":\"float\",\"min\":20,\"max\":18000,\"step\":10},"
        "{\"key\":\"filter_res\",\"name\":\"Reso\",\"type\":\"float\",\"min\":0.1,\"max\":5,\"step\":0.1},"
        "{\"key\":\"noise_attack\",\"name\":\"N.Attack\",\"type\":\"float\",\"min\":0.001,\"max\":1,\"step\":0.001},"
        "{\"key\":\"noise_decay\",\"name\":\"N.Decay\",\"type\":\"float\",\"min\":0.001,\"max\":1,\"step\":0.01},"
        "{\"key\":\"mix\",\"name\":\"Mix\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},"
        "{\"key\":\"distortion\",\"name\":\"Distort\",\"type\":\"float\",\"min\":0,\"max\":50,\"step\":0.5},"
        "{\"key\":\"level\",\"name\":\"Level\",\"type\":\"float\",\"min\":-96,\"max\":24,\"step\":0.5},"
        "{\"key\":\"preset\",\"name\":\"Preset\",\"type\":\"enum\",\"options\":[\"Custom\",\"Kick\",\"Snare\",\"Tom\",\"Clap\",\"Rimshot\",\"Hi Hat\",\"Cymbal\"]}"
        "]";
        int len = (int)strlen(cp);
        if (len >= buf_len) return -1;
        memcpy(buf, cp, len + 1);
        return len;
    }

    // State serialization
    if (strcmp(key, "state") == 0) {
        const weird_drum::DrumParams &p = inst->params;
        return snprintf(buf, buf_len,
            "{\"freq\":%.1f,\"attack\":%.4f,\"decay\":%.4f,"
            "\"wave_type\":%d,\"pitch_env_amt\":%.4f,\"pitch_env_rate\":%.4f,"
            "\"pitch_lfo_amt\":%.4f,\"pitch_lfo_rate\":%.4f,"
            "\"filter_type\":%d,\"filter_cutoff\":%.1f,\"filter_res\":%.1f,"
            "\"noise_attack\":%.4f,\"noise_decay\":%.4f,"
            "\"mix\":%.4f,\"distortion\":%.1f,\"level\":%.1f,"
            "\"preset\":%d}",
            p.freq, p.attack, p.decay,
            p.wave_type, p.pitch_env_amt, p.pitch_env_rate,
            p.pitch_lfo_amt, p.pitch_lfo_rate,
            p.filter_type, p.filter_cutoff, p.filter_res,
            p.noise_attack, p.noise_decay,
            p.mix, p.distortion, p.level,
            inst->preset);
    }

    return -1;
}

static void wd_on_midi(void *instance, const uint8_t *msg, int len, int source) {
    WDInstance *inst = (WDInstance*)instance;
    if (!inst || len < 3) return;

    uint8_t status = msg[0] & 0xF0;
    uint8_t velocity = msg[2];

    if (status == 0x90 && velocity > 0) {
        // Note On — trigger next voice
        float vel = velocity / 127.0f;
        int v = inst->next_voice;

        // If current slot is active, find oldest voice to steal
        if (inst->voices[v].is_active()) {
            uint32_t oldest_age = 0;
            int oldest_idx = v;
            for (int i = 0; i < kNumVoices; i++) {
                if (!inst->voices[i].is_active()) {
                    oldest_idx = i;
                    break;
                }
                if (inst->voices[i].age() > oldest_age) {
                    oldest_age = inst->voices[i].age();
                    oldest_idx = i;
                }
            }
            v = oldest_idx;
        }

        inst->voices[v].note_on(vel, inst->params);
        inst->next_voice = (v + 1) % kNumVoices;
    }
    // Note Off (0x80 or 0x90 with vel=0) — AD envelope, no action needed
}

// ---- Static API struct ----
static audio_fx_api_v2_t g_api = {
    .api_version = 2,
    .create_instance = wd_create,
    .destroy_instance = wd_destroy,
    .process_block = wd_process,
    .set_param = wd_set_param,
    .get_param = wd_get_param,
    .on_midi = wd_on_midi,
};

// ---- Entry point (dlsym) ----
extern "C" {

__attribute__((visibility("default")))
audio_fx_api_v2_t* move_audio_fx_init_v2(const host_api_v1_t *host) {
    g_host = host;
    if (host && host->log) host->log("[weird_drum] Weird Drum v1.0.0 loaded");
    return &g_api;
}

}  // extern "C"
