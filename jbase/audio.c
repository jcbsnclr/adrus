/*
 * midid - software MIDI synthesiser, utilising JACK
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

#include <jbase.h>

#ifdef JBASE_AUDIO

#include <jack/jack.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "jack/midiport.h"

static void jack_error_report(const char *msg) {
    jb_error("JACK: %s", msg);
}

static void jack_info_report(const char *msg) {
    jb_info("JACK: %s", msg);
}

static int jack_xrun(void *arg) {
    (void)arg;
    jb_warn("xrun detected");
    return 0;
}

enum { MIDI_STATUS = 0, MIDI_ARG0 = 1, MIDI_ARG1 = 2 };

static int jack_process(jack_nframes_t nframes, void *arg) {
    jb_client_t *cl = (jb_client_t *)arg;

    void *midi_buf = jack_port_get_buffer(cl->midi_in, nframes);
    jb_sample_t *audio_buf = (jb_sample_t *)jack_port_get_buffer(cl->audio_out, nframes);

    jb_midi_t ev;
    jack_midi_event_t raw_ev;
    size_t ev_count = jack_midi_get_event_count(midi_buf);

    for (size_t i = 0; i < ev_count && cl->cfg.midi_cb; i++) {
        jack_midi_event_get(&raw_ev, midi_buf, i);

        ev.kind = raw_ev.buffer[MIDI_STATUS] & 0xf0;
        ev.chan = raw_ev.buffer[MIDI_STATUS] & 0x0f;
        ev.args[0] = raw_ev.buffer[MIDI_ARG0];
        ev.args[1] = raw_ev.buffer[MIDI_ARG1];

        cl->cfg.midi_cb(cl->cfg.state, ev);
    }

    jack_get_cycle_times(
        cl->jack, &cl->ctx.cur_frames, &cl->ctx.time, &cl->ctx.next_usecs, &cl->ctx.period_usecs);

    if (cl->cfg.audio_cb) cl->cfg.audio_cb(cl->cfg.state, cl->ctx, nframes, audio_buf);

    bool is_nan = false;
    for (size_t i = 0; i < nframes; i++)
        // check for NaN (NaN comparisons should always be false; IEEE floats will
        // fail this condition if NaN)
        if (audio_buf[i] != audio_buf[i]) is_nan = true;

    if (is_nan) jb_warn("NaN samples detected");

    cl->ctx.cur_sample += nframes;

    return 0;
}

static int jack_srate(jack_nframes_t nframes, void *arg) {
    jb_client_t *cl = (jb_client_t *)arg;
    cl->ctx.srate = nframes;
    return 0;
}

jb_res_t jb_client_init(jb_client_t *cl, jb_client_config_t cfg) {
    cl->cfg = cfg;

    jb_debug("opening connection to JACK");

    jack_status_t status;
    cl->jack = jack_client_open(cfg.name, JackNullOption, &status);

    if (!cl->jack) return JB_ERR(JB_ERR_JACK, "failed  to open connection to JACK");

    jb_debug("opening MIDI input port");
    cl->midi_in =
        jack_port_register(cl->jack, "midi_in", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);

    if (!cl->midi_in) return JB_ERR(JB_ERR_JACK, "failed to open MIDI input port");

    jb_debug("opening audio output port");
    cl->audio_out =
        jack_port_register(cl->jack, "audio_out", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

    if (!cl->audio_out) return JB_ERR(JB_ERR_JACK, "failed to open audio output port");

    jb_debug("installing callbacks");

    jack_set_error_function(jack_error_report);
    jack_set_info_function(jack_info_report);

    jack_set_process_callback(cl->jack, jack_process, (void *)cl);
    jack_set_sample_rate_callback(cl->jack, jack_srate, (void *)cl);
    jack_set_xrun_callback(cl->jack, jack_xrun, NULL);

    cl->ctx.cur_frames = 0;
    cl->ctx.cur_sample = 0;
    cl->ctx.next_usecs = 0;
    cl->ctx.period_usecs = 0;
    cl->ctx.time = 0;

    return JB_OK_VAL;
}

jb_res_t jb_client_connect_midi(jb_client_t *cl, char *pat) {
    const char *in_port_name = jack_port_name(cl->midi_in);
    const char **ports = jack_get_ports(cl->jack, pat, JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput);

    if (!ports) return JB_ERR(JB_ERR_JACK, "failed to enumerate MIDI ports (pat = %s)", pat);

    for (const char **cur = ports; *cur; cur++) jack_connect(cl->jack, *cur, in_port_name);

    free(ports);

    return JB_OK_VAL;
}

jb_res_t jb_client_connect_audio(jb_client_t *cl, char *pat) {
    const char *out_port_name = jack_port_name(cl->audio_out);
    const char **ports = jack_get_ports(cl->jack, pat, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput);

    if (!ports) return JB_ERR(JB_ERR_JACK, "failed to enumerate audio ports (pat = %s)", pat);

    for (const char **cur = ports; *cur; cur++) {
        jack_connect(cl->jack, out_port_name, *cur);
    }

    free(ports);

    return JB_OK_VAL;
}

jb_res_t jb_client_start(jb_client_t *cl) {
    if (jack_activate(cl->jack) != 0) return JB_ERR(JB_ERR_JACK, "failed to activate JACK client");

    getc(stdin);

    return JB_OK_VAL;
}

#endif
