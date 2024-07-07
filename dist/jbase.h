/*
 * jbase - C utility library
 * Copyright (C) 2024  Jacob Sinclair <jcbsnclr@outlook.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma  once

// config
#include "dirent.h"
#undef JBASE_AUDIO
#undef JBASE_LOG_META

// includes
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifdef JBASE_AUDIO
#include "jack/types.h"
#endif

//
// misc. defines
//
#define JB_TLOCAL _Thread_local

#define JB_MAX(x, y) ((x) > (y) ? (x) : (y))
#define JB_MIN(x, y) ((x) < (y) ? (x) : (y))

//
// logging facilities: log.c
//

typedef enum {
    JB_TRACE,   // extra verbose info
    JB_DEBUG,   // mostly unimportant
    JB_INFO,    // general info
    JB_WARN,    // something might be wrong
    JB_ERROR    // something is catastrophically wrong
} jb_llevel_t;

// initialise logging, read filter from `LOG_FILTER` env var
void jb_log_init();

// log an individual message to stderr w/ metadata
void jb_log_inner(jb_llevel_t level, const char *filename, uint32_t line, const char *func, char *fmt, ...);
// log an individual line, without any error metadata
void jb_log_line(char *fmt, ...);

// utility macros for logging messages with a given level and printf-formatted message
#define jb_trace(...) jb_log_inner(JB_TRACE, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define jb_debug(...) jb_log_inner(JB_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define jb_info(...)  jb_log_inner(JB_INFO,  __FILE__, __LINE__, __func__, __VA_ARGS__)
#define jb_warn(...)  jb_log_inner(JB_WARN,  __FILE__, __LINE__, __func__, __VA_ARGS__)
#define jb_error(...) jb_log_inner(JB_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)

//
// hashing: hash.c
//

typedef uint64_t jb_hash_t;

jb_hash_t jb_fnv1a(const void *buf, size_t bytes);
jb_hash_t jb_fnv1a_str(const char *str);

//
// error handling: err.c
//

typedef enum {
    JB_OK,        // nothing wrong
    JB_ERR_JACK,  // JACK operation failed
    JB_ERR_LIBC,  // libc operation failed
    JB_ERR_OOM,   // out of memory (currently unused)
    JB_ERR_USER,  // user-defined error info, for users of library

    JB_ERR_PARSER
} jb_err_t;

typedef struct{
    jb_err_t kind;    // status of result
    char *msg;        // err message (if kind != JB_OK0)
    int libc;        // errno value (if libc error)

    const char *file; // file the error originates from (in-source, or at runtime)
    const char *func; // function the error originates in
    size_t line;      // line causing the error
} jb_res_t;

// macros for constructing/checking a jb_res_t
#define JB_OK_VAL ((jb_res_t){ .kind = JB_OK })
#define JB_ERR(k, ...) jb_err_impl((k), 0, __LINE__, __FILE__, __func__, __VA_ARGS__)

#define JB_ERR_LIBC(e, ...) jb_err_impl(JB_ERR_LIBC, (e), __LINE__, __FILE__, __func__, __VA_ARGS__)

#define JB_IS_OK .kind == JB_OK
#define JB_IS_ERR .kind != JB_OK

// if result is error, return result
#define JB_TRY(res) {jb_res_t result = (res); if ((result) JB_IS_ERR) return result;}

#ifdef JBASE_ASSERT

#define JB_ASSERT(x) {if (!(x)) {jb_error("assertion `" #x "` failed"); exit(1);}}
#define JB_ASSERT_PTR(x) JB_ASSERT((x) != NULL)

#else

#define JB_ASSERT(x)
#define JB_ASSERT_PTR(x)

#endif

// log a result on the terminal
void jb_report_result(jb_res_t info); 
// construct jb_res_t with printf-formatted message
jb_res_t jb_err_impl(jb_err_t kind, int libc, size_t line, const char *file, const char *func, char *fmt, ...); 

//
// lexing: lexer.c
//

typedef struct {
    const char *src;
    size_t pos, len;
} jb_lexer_t;

typedef bool (*jb_ccond_t)(char);

void jb_lx_init_str(jb_lexer_t *lx, const char *src);
void jb_lx_init(jb_lexer_t *lx, const char *src, size_t len);

char jb_lx_peek(jb_lexer_t *lx);
bool jb_lx_take_if(jb_lexer_t *lx, jb_ccond_t cond);
bool jb_lx_take_ifc(jb_lexer_t *lx, char c);
bool jb_lx_take(jb_lexer_t *lx, char *c);
bool jb_lx_take_while(jb_lexer_t *lx, jb_ccond_t cond);
bool jb_lx_eof(jb_lexer_t *lx);

bool jb_lx_tok(char c);
bool jb_lx_ws(char c);

//
// iterators: iter.c
//

typedef struct {
    uint64_t magic;
    bool (*next)(void *iter, void *out);
} jb_iter_t;

typedef struct {
    jb_iter_t iter;
    jb_lexer_t lx;
} jb_path_parts_t;

bool jb_path_parts(jb_path_parts_t *parts, const char *path);
bool jb_dir_parts(jb_path_parts_t *parts, const char *path);

void jb_iter_init(void *iter);
bool jb_iter_next(void *iter, void *out);

//
// input/output: io.c
//

typedef int jb_errno_t; // errno, 0 for success

typedef struct {
    size_t len, cap;
    uint8_t *buf;
} jb_io_buf_t;

jb_errno_t jb_io_buf_init(jb_io_buf_t *buf, size_t cap);

jb_errno_t jb_io_buf_write(jb_io_buf_t *buf, uint8_t *data, size_t len);
jb_errno_t jb_io_buf_flush(jb_io_buf_t *buf, FILE *f);
void jb_io_buf_clear(jb_io_buf_t *buf);
void jb_io_buf_free(jb_io_buf_t *buf);

jb_errno_t jb_load_file(const char *path, uint8_t **data, size_t *len);
jb_errno_t jb_store_file(const char *path, uint8_t *data, size_t len);

jb_errno_t jb_read_line(FILE *f, jb_io_buf_t *buf);

jb_errno_t jb_basename(const char *path, char *out, size_t size);
jb_errno_t jb_dirname(const char *path, char *out, size_t size);

jb_errno_t jb_path_cat(const char *p1, const char *p2, char *out);

jb_errno_t jb_mkdir_rec(const char *path);

#define JB_TRY_IO(res, ...) {int result = (res); if ((result) != 0) return JB_ERR_LIBC(result, __VA_ARGS__);}

//
// virtual machine: vm.c
//

typedef struct jb_cmd {
    struct jb_val *body; 
    size_t start, end;
} jb_cmd_t;

typedef struct jb_val {
    enum {
        JB_VAL_STR,
        JB_VAL_INT,
        JB_VAL_REF,

        JB_VAL_QUOTE,
        JB_VAL_INLINE
    } kind;

    size_t start, end; // span in source file

    union {
        char *str_val;
        int64_t int_val;
        jb_cmd_t *body;
    };
} jb_val_t;

jb_res_t jb_parse(char *src, jb_val_t *val);
void jb_write_val(FILE *f, jb_val_t *val, size_t indent);

//
// utilities: util.c
//


typedef struct {
	size_t len;
	size_t cap;
	char buf[];
} jb_buf_hdr_t;

#define JB_BUF NULL

// Get a pointer to the header of a buffer
#define jb_buf_hdr(b) ((jb_buf_hdr_t *)((char *)(b) - offsetof(jb_buf_hdr_t, buf)))

// Length, capacity and end index of a buffer
#define jb_buf_len(b) ((b) ? jb_buf_hdr(b)->len : 0)
#define jb_buf_cap(b) ((b) ? jb_buf_hdr(b)->cap : 0)
#define jb_buf_end(b) ((b) + jb_buf_len(b))

#define jb_buf_last(b) (jb_buf_len(b) != 0 ? &b[jb_buf_len(b) - 1] : NULL)

#define jb_buf_free(b) ((b) ? (free(jb_buf_hdr(b)), (b) = NULL) : 0)
#define jb_buf_fit(b, n) ((n) <= jb_buf_cap(b) ? 0 : ((b) = jb_buf_grow((b), (n), sizeof(*(b)))))

#define jb_buf_push(b, v) (jb_buf_fit((b), 1 + jb_buf_len(b)), (b)[jb_buf_hdr(b)->len++] = ((v)))
#define jb_buf_pop(b, v) (jb_buf_len((b)) != 0 ? (v = (b)[--jb_buf_hdr(b)->len], true) : false)

void *jb_buf_grow(const void *buf, size_t new_len, size_t elem_size);

// 
// audio client 
//

#ifdef JBASE_AUDIO 

// individual audio sample
typedef jack_default_audio_sample_t jb_sample_t;

// constants to access MIDI event params 
enum {
    // note event
    JB_NOTE = 0,
    JB_VELOCITY = 1,

    // control event
    JB_CONTROLLER = 0,
    JB_VALUE = 1
};

typedef struct {
    enum {
        JB_NOTE_OFF = 0x80,
        JB_NOTE_ON = 0x90,
        JB_CTRL = 0xB0
    } kind;          // MIDI event kind (first 4 bits of status byte)

    uint8_t chan;    // MIDI channel
    uint8_t args[2]; // 2 argument bytes
} jb_midi_t;

typedef struct {
    size_t srate;              // sample rate
    size_t cur_sample;         // current base sample (samples processed up to start of current cycle)

    jack_nframes_t cur_frames; // precise time at start of current cycle (?)
    jack_time_t time;          // absolute time in usecs 
    jack_time_t next_usecs;    // best estimate of time of next sample 
    float period_usecs;        // roughly difference between next_usecs and time (?)
} jb_ctx_t;

// MIDI event processing callback
typedef void (*jb_midi_fn_t)(void *state, jb_midi_t ev);
// audio buffer generating callback
typedef void (*jb_audio_fn_t)(void *state, jb_ctx_t ctx, size_t nframes, jb_sample_t *buf);

typedef struct {
    char *name;             // name of JACK client
    void *state;            // pointer to user-supplied state (accessible in callbacks)

    jb_midi_fn_t midi_cb;   // callback to process MIDI events
    jb_audio_fn_t audio_cb; // callback to generate audio
} jb_client_config_t;

typedef struct {
    jb_client_config_t cfg; // client configuration
    
    jack_client_t *jack;    // JACK client
    jack_port_t *midi_in;   //  MIDI input port
    jack_port_t *audio_out; // audio output port

    jb_ctx_t ctx;           // current context (timing information)
} jb_client_t;

jb_res_t jb_client_init(jb_client_t *cl, jb_client_config_t cfg); // initialise client with config

jb_res_t jb_client_connect_midi(jb_client_t *cl, char *pat);      // connect MIDI input to ports matching pattern
jb_res_t jb_client_connect_audio(jb_client_t *cl, char *pat);     // connect audio output to ports matching pattern

jb_res_t jb_client_start(jb_client_t *cl);                        // activate JACK client

#endif

// 
// audio synthesis: synth.c
//

typedef float (*jb_wave_fn_t)(float x, float bias); // wave function (0 <= x < 2pi)
typedef int32_t jb_cents_t;                         // 1 cent = 1/100th of a semitone

typedef enum {
    JB_MOD_AM, // amplitude modulation
    JB_MOD_FM, // frequency modulation
    JB_MOD_PM, // phase modulation
    JB_MOD_BM, // "bias" modulation (pulse-width or wave folding modulation)
    JB_MOD_MAX
} jb_mod_t;

typedef struct {
    jb_wave_fn_t fn;   // wave function
    jb_cents_t detune; // detune (in cents)
    float amp;         // wave amplitude
    float bias;        // amount of folding, or pulse width
} jb_osc_t;

typedef struct jb_osc_link {
    jb_osc_t *osc;            // oscillator
    jb_mod_t mod;             // type of modulation
    struct jb_osc_link *next; // next in chain
} jb_osc_link_t;

// macro to convert semitones to cents
#define JB_SEMIS(semi) ((semi) * 100)

// A4 note (relative to MIDI) in cents
#define JB_A4_MIDI JB_SEMIS(69)
// A4 note in Hz
#define JB_A4_HZ 440.f

float jb_cents_hz(jb_cents_t cents);                                                   // convert cents (in MIDI space) to Hz 
float jb_osc_sample(jb_osc_t *osc, jb_cents_t note, size_t idx, size_t srate);         // sample an oscillator at a given note in cents
float jb_chain_sample(jb_osc_link_t *link, jb_cents_t note, size_t idx, size_t srate); // sample a chain of oscillators modulating eachother

float jb_wave_sin(float x, float bias);
float jb_wave_square(float x, float bias);
float jb_wave_triangle(float x, float bias);
float jb_wave_saw(float x, float bias);
float jb_wave_noise(float x, float bias);

// terminal control
//

#define JB_RESET              "\x1b[0m"
#define JB_BOLD               "\x1b[1m"

#define JB_FG_BLACK           "\x1b[30m"
#define JB_FG_RED             "\x1b[31m"
#define JB_FG_GREEN           "\x1b[32m"
#define JB_FG_YELLOW          "\x1b[33m"
#define JB_FG_BLUE            "\x1b[34m"
#define JB_FG_MAGENTA         "\x1b[35m"
#define JB_FG_CYAN            "\x1b[36m"
#define JB_FG_WHITE           "\x1b[37m"

#define JB_FG_BLACK_BRIGHT    "\x1b[30;1m"
#define JB_FG_RED_BRIGHT      "\x1b[31;1m"
#define JB_FG_GREEN_BRIGHT    "\x1b[32;1m"
#define JB_FG_YELLOW_BRIGHT   "\x1b[33;1m"
#define JB_FG_BLUE_BRIGHT     "\x1b[34;1m"
#define JB_FG_MAGENTA_BRIGHT  "\x1b[35;1m"
#define JB_FG_CYAN_BRIGHT     "\x1b[36;1m"
#define JB_FG_WHITE_BRIGHT    "\x1b[37;1m"


#define JB_BG_BLACK           "\x1b[40m" 
#define JB_BG_RED             "\x1b[41m" 
#define JB_BG_GREEN           "\x1b[42m" 
#define JB_BG_YELLOW          "\x1b[43m" 
#define JB_BG_BLUE            "\x1b[44m" 
#define JB_BG_MAGENTA         "\x1b[45m" 
#define JB_BG_CYAN            "\x1b[46m" 
#define JB_BG_WHITE           "\x1b[47m" 


#define JB_BG_BLACK_BRIGHT    "\x1b[40;1m"
#define JB_BG_RED_BRIGHT      "\x1b[41;1m"
#define JB_BG_GREEN_BRIGHT    "\x1b[42;1m"
#define JB_BG_YELLOW_BRIGHT   "\x1b[43;1m"
#define JB_BG_BLUE_BRIGHT     "\x1b[44;1m"
#define JB_BG_MAGENTA_BRIGHT  "\x1b[45;1m"
#define JB_BG_CYAN_BRIGHT     "\x1b[46;1m"
#define JB_BG_WHITE_BRIGHT    "\x1b[47;1m"
