/*
 * Verglas — Chain UI
 *
 * 2-page parameter browser with jog navigation and editing.
 * Modeled on Super Boum's working ui_chain.js pattern.
 */

import {
    MoveMainKnob,
    MoveBack,
    LightGrey
} from '/data/UserData/move-anything/shared/constants.mjs';

import { decodeDelta, decodeAcceleratedDelta } from '/data/UserData/move-anything/shared/input_filter.mjs';

import {
    drawMenuHeader as drawHeader,
    drawMenuFooter as drawFooter
} from '/data/UserData/move-anything/shared/menu_layout.mjs';

const W = 128;
const H = 64;
const CC_JOG_CLICK = 3;
const KNOB_CC_BASE = 71;

/* ============================================ Page Definitions == */

const PAGES = [
    {
        name: "Verglas",
        params: [
            { key: "position", name: "Position", min: 0,   max: 1,  step: 0.01, fmt: v => `${(v*100).toFixed(0)}%` },
            { key: "size",     name: "Size",     min: 0,   max: 1,  step: 0.01, fmt: v => `${(v*100).toFixed(0)}%` },
            { key: "pitch",    name: "Pitch",    min: -24, max: 24, step: 1,    fmt: v => `${v >= 0 ? "+" : ""}${Math.round(v)} st` },
            { key: "density",  name: "Density",  min: 0,   max: 1,  step: 0.01, fmt: v => `${(v*100).toFixed(0)}%` },
            { key: "texture",  name: "Texture",  min: 0,   max: 1,  step: 0.01, fmt: v => `${(v*100).toFixed(0)}%` },
            { key: "feedback", name: "Feedback", min: 0,   max: 1,  step: 0.01, fmt: v => `${(v*100).toFixed(0)}%` },
            { key: "reverb",   name: "Reverb",   min: 0,   max: 1,  step: 0.01, fmt: v => `${(v*100).toFixed(0)}%` },
            { key: "dry_wet",  name: "Mix",      min: 0,   max: 1,  step: 0.01, fmt: v => `${(v*100).toFixed(0)}%` },
            { key: "mode",     name: "Mode",     min: 0,   max: 3,  step: 1,    enum: ["Granular", "Stretch", "Looper", "Spectral"] },
            { key: "freeze",   name: "Freeze",   min: 0,   max: 1,  step: 1,    enum: ["Off", "On"] },
            { key: "quality",  name: "Quality",  min: 0,   max: 1,  step: 1,    enum: ["16-bit", "Lo-Fi"] },
            { key: "stereo_spread", name: "Spread", min: 0, max: 1, step: 0.01, fmt: v => `${(v*100).toFixed(0)}%` }
        ]
    },
    {
        name: "Filters",
        params: [
            { key: "filter_hp",    name: "HPF",       min: 0,    max: 1000,  step: 10, fmt: v => v < 1 ? "OFF" : `${Math.round(v)} Hz` },
            { key: "filter_lp",    name: "LPF",       min: 1000, max: 20000, step: 10, fmt: v => v > 19999 ? "OFF" : `${Math.round(v)} Hz` },
            { key: "limiter_on",   name: "Limiter",   min: 0,    max: 1,     step: 1,  enum: ["Off", "On"] },
            { key: "limiter_pre",  name: "Pre Gain",  min: -6,   max: 6,     step: 0.5, fmt: v => `${v >= 0 ? "+" : ""}${v.toFixed(1)} dB` },
            { key: "limiter_post", name: "Post Gain", min: -6,   max: 6,     step: 0.5, fmt: v => `${v >= 0 ? "+" : ""}${v.toFixed(1)} dB` }
        ]
    }
];

/* ============================================ State == */

let selectedPage = 0;
let insidePage = false;
let selectedParam = 0;
let editMode = false;
let needsRedraw = true;

let values = {};

/* ============================================ Helpers == */

function formatValue(p, v) {
    if (p.enum) return p.enum[Math.round(v)] || "?";
    return p.fmt(v);
}

function clampValue(p, v) {
    if (p.enum) {
        v = Math.round(v);
        if (v > p.max) v = p.max;
        if (v < p.min) v = p.min;
    } else {
        v = Math.max(p.min, Math.min(p.max, v));
    }
    return v;
}

function setParam(p, value) {
    value = clampValue(p, value);
    values[p.key] = value;
    let valStr;
    if (p.enum) {
        valStr = p.enum[Math.round(value)];
    } else if (p.step >= 1) {
        valStr = String(Math.round(value));
    } else {
        valStr = value.toFixed(4);
    }
    host_module_set_param(p.key, valStr);
    needsRedraw = true;
}

function fetchAllParams() {
    for (const page of PAGES) {
        for (const p of page.params) {
            const raw = host_module_get_param(p.key);
            if (raw !== null && raw !== undefined && raw !== "") {
                if (p.enum) {
                    const idx = p.enum.indexOf(raw);
                    if (idx >= 0) {
                        values[p.key] = idx;
                    } else {
                        values[p.key] = parseFloat(raw) || 0;
                    }
                } else {
                    const num = parseFloat(raw);
                    if (!isNaN(num)) values[p.key] = num;
                }
            }
        }
    }
}

/* ============================================ Drawing == */

function drawRootView() {
    clear_screen();
    drawHeader("Verglas");

    const activePage = PAGES[selectedPage];
    print(2, 12, activePage.name, 1);

    const lh = 11;
    const y0 = 24;

    for (let i = 0; i < PAGES.length; i++) {
        const y = y0 + i * lh;
        const sel = i === selectedPage;

        if (sel) fill_rect(0, y - 1, W, lh, 1);

        const color = sel ? 0 : 1;
        const prefix = sel ? "> " : "  ";
        print(2, y, `${prefix}${PAGES[i].name}`, color);

        const firstP = PAGES[i].params[0];
        const v = values[firstP.key];
        if (v !== undefined) {
            const vs = formatValue(firstP, v);
            print(W - vs.length * 6 - 4, y, vs, color);
        }
    }

    drawFooter({ left: "Jog:page", right: "Click:enter" });
}

function drawPageView() {
    clear_screen();

    const page = PAGES[selectedPage];
    drawHeader(`Verglas: ${page.name}`);

    const lh = 11;
    const y0 = 16;
    const visible = 4;
    let startIdx = Math.max(0, Math.min(selectedParam - 1, page.params.length - visible));

    for (let vi = 0; vi < visible; vi++) {
        const i = startIdx + vi;
        if (i >= page.params.length) break;

        const y = y0 + vi * lh;
        const p = page.params[i];
        const sel = i === selectedParam;

        if (sel) fill_rect(0, y - 1, W, lh, 1);

        const color = sel ? 0 : 1;
        const prefix = sel ? (editMode ? "* " : "> ") : "  ";
        print(2, y, `${prefix}${p.name}`, color);

        const v = values[p.key];
        if (v !== undefined) {
            const vs = formatValue(p, v);
            print(W - vs.length * 6 - 4, y, vs, color);
        }
    }

    if (editMode) {
        drawFooter({ left: "Jog:value", right: "Click:done" });
    } else {
        drawFooter({ left: "Jog:param", right: "Back:pages" });
    }
}

function draw() {
    if (insidePage) {
        drawPageView();
    } else {
        drawRootView();
    }
    needsRedraw = false;
}

/* ============================================ Knob Handling == */

function handleKnob(knobIdx, delta) {
    const page = PAGES[selectedPage];
    if (knobIdx >= page.params.length) return;

    const p = page.params[knobIdx];
    const v = values[p.key] !== undefined ? values[p.key] : p.min;
    setParam(p, v + delta * p.step);
}

/* ============================================ Input == */

function init() {
    fetchAllParams();
    needsRedraw = true;
}

function tick() {
    fetchAllParams();
    if (needsRedraw) draw();
}

function onMidiMessageInternal(data) {
    const status = data[0] & 0xF0;
    const d1 = data[1];
    const d2 = data[2];

    if (status !== 0xB0) return;

    /* Jog wheel rotate */
    if (d1 === MoveMainKnob) {
        const delta = decodeDelta(d2);
        if (delta === 0) return;

        if (!insidePage) {
            selectedPage = Math.max(0, Math.min(PAGES.length - 1, selectedPage + delta));
            needsRedraw = true;
        } else if (editMode) {
            const page = PAGES[selectedPage];
            const p = page.params[selectedParam];
            const v = values[p.key] !== undefined ? values[p.key] : p.min;
            setParam(p, v + delta * p.step);
        } else {
            const page = PAGES[selectedPage];
            selectedParam = Math.max(0, Math.min(page.params.length - 1, selectedParam + delta));
            needsRedraw = true;
        }
        return;
    }

    /* Jog click */
    if (d1 === CC_JOG_CLICK && d2 >= 64) {
        if (!insidePage) {
            insidePage = true;
            selectedParam = 0;
            editMode = false;
        } else if (editMode) {
            editMode = false;
        } else {
            editMode = true;
        }
        needsRedraw = true;
        return;
    }

    /* Back button */
    if (d1 === MoveBack && d2 >= 64) {
        if (editMode) {
            editMode = false;
            needsRedraw = true;
        } else if (insidePage) {
            insidePage = false;
            needsRedraw = true;
        }
        return;
    }

    /* Knobs 1-8 always map to highlighted page */
    const knobIdx = d1 - KNOB_CC_BASE;
    if (knobIdx >= 0 && knobIdx < 8) {
        const delta = decodeDelta(d2);
        if (delta !== 0) handleKnob(knobIdx, delta);
        return;
    }
}

/* Export for chain UI */
globalThis.chain_ui = {
    init,
    tick,
    onMidiMessageInternal
};

globalThis.init = init;
globalThis.tick = tick;
globalThis.onMidiMessageInternal = onMidiMessageInternal;
