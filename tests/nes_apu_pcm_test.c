#include "nes.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint16_t address, bytes, cpu_cycles;
    uint8_t bits, shift, sample, out, silence, enabled, irq, irq_line;
    uint16_t lfsr;
    uint32_t timer_acc, p1_phase, p2_phase, tri_phase, noise_acc;
    uint64_t clock;
} DmaEvent;

typedef struct {
    DmaEvent* events;
    size_t count, capacity;
} DmaTrace;

static nes_t cores[2];
static DmaTrace traces[2];
static int active;
static unsigned sound_calls[2];
static uint8_t last_sound[2][NES_APU_SAMPLE_PER_SYNC_MAX];
static size_t last_sound_size[2];
static unsigned dma_action;
static uint64_t segments, writes, reads, dma_reads, pcm_outputs;
static uint32_t random_state = 0x93b1d74fu;

static uint32_t next_random(void) {
    uint32_t value = random_state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    return random_state = value;
}

int nes_sound_output(uint8_t* buffer, size_t length) {
    assert(length <= sizeof(last_sound[active]));
    memcpy(last_sound[active], buffer, length);
    last_sound_size[active] = length;
    ++sound_calls[active];
    return NES_OK;
}

/* Record the state visible to a mapper read, including deferred phase writes. */
uint8_t nes_cpu_dma_read(nes_t* nes, uint16_t address) {
    assert(nes == &cores[active]);
    DmaTrace* trace = &traces[active];
    if (trace->count == trace->capacity) {
        trace->capacity = trace->capacity ? trace->capacity * 2 : 64;
        trace->events = realloc(trace->events, trace->capacity * sizeof(*trace->events));
        assert(trace->events);
    }
    DmaEvent event;
    memset(&event, 0, sizeof(event));
    const dmc_t* dmc = &nes->nes_apu.dmc;
    event.address = address;
    event.bytes = dmc->bytes_remaining;
    event.cpu_cycles = nes->nes_cpu.cycles;
    event.bits = dmc->bits_remaining;
    event.shift = dmc->shift_reg;
    event.sample = dmc->sample_buffer;
    event.out = dmc->out_level;
    event.silence = dmc->silence;
    event.enabled = dmc->enabled;
    event.irq = dmc->irq_flag;
    event.irq_line = nes->nes_apu.irq_line;
    event.timer_acc = dmc->timer_acc;
    event.p1_phase = nes->nes_apu.pulse1.phase_acc;
    event.p2_phase = nes->nes_apu.pulse2.phase_acc;
    event.tri_phase = nes->nes_apu.triangle.phase_acc;
    event.noise_acc = nes->nes_apu.noise.lfsr_acc;
    event.lfsr = nes->nes_apu.noise.lfsr;
    event.clock = nes->nes_apu.clock_count;
    memcpy(&trace->events[trace->count++], &event, sizeof(event));

    /* Mapper reads can have side effects and can change DMC registers. */
    ++nes->nes_cpu.cpu_ram[0x3d];
    if (dma_action == 1 && trace->count == 2)
        nes_write_apu_register(nes, 0x4015, 0);
    if (dma_action == 2 && trace->count == 2)
        nes_write_apu_register(nes, 0x4010, 0x8f);
    return (uint8_t)(address ^ (address >> 8) ^ trace->count);
}

static void check_state(void) {
    nes_t reference, candidate;
    memcpy(&reference, &cores[0], sizeof(reference));
    memcpy(&candidate, &cores[1], sizeof(candidate));
    candidate.host_pcm_enabled = reference.host_pcm_enabled;
    candidate.host_pcm_valid = reference.host_pcm_valid;
    memcpy(candidate.nes_apu.sample_buffer, reference.nes_apu.sample_buffer,
           sizeof(reference.nes_apu.sample_buffer));
    if (memcmp(&reference, &candidate, sizeof(reference))) {
        const uint8_t* expected = (const uint8_t*)&reference;
        const uint8_t* actual = (const uint8_t*)&candidate;
        for (size_t i = 0; i < sizeof(reference); ++i) {
            if (expected[i] != actual[i]) {
                fprintf(stderr, "APU state mismatch: segment=%llu offset=%zu values=%u,%u\n",
                        (unsigned long long)segments, i, expected[i], actual[i]);
                break;
            }
        }
        abort();
    }
}

static void prepare(int pal) {
    memset(cores, 0, sizeof(cores));
    for (int i = 0; i < 2; ++i) {
        nes_t* nes = &cores[i];
        nes->timing.cpu_clock = pal ? 1662607u : 1789773u;
        nes->timing.frame_rate = pal ? 50 : 60;
        nes->timing.samples_per_frame = pal ? 480 : 400;
        nes->timing.apu_frame_divisor = pal ? 78 : 66;
        nes->nes_cpu.A = 0x39;
        nes->nes_cpu.X = 0x27;
        nes->nes_cpu.Y = 0xb1;
        nes->nes_cpu.PC = 0xca39;
        nes->nes_cpu.cycles = 0x153;
        nes->nes_cpu.irq_pending = 0xa5;
        nes->nes_cpu.cpu_ram[0x40] = 0x71;
        nes->nes_ppu.v_reg = 0x45c3;
        nes->nes_ppu.t_reg = 0x369a;
        nes->nes_ppu.oam_data[13] = 0x79;
        nes->host_pcm_enabled = i == 0;
        nes->host_pcm_valid = i == 0 ? 15 : 0;
        nes_apu_init(nes);
        memset(nes->nes_apu.sample_buffer, i ? 0x5a : 0xa5,
               sizeof(nes->nes_apu.sample_buffer));
    }
    dma_action = 0;
    check_state();
}

static void write_both(uint16_t address, uint8_t value) {
    for (int i = 0; i < 2; ++i) nes_write_apu_register(&cores[i], address, value);
    ++writes;
    check_state();
}

static uint8_t read_both(void) {
    const uint8_t reference = nes_read_apu_register(&cores[0], 0x4015);
    const uint8_t candidate = nes_read_apu_register(&cores[1], 0x4015);
    assert(reference == candidate);
    assert(cores[0].nes_cpu.irq_pending == 0xa5);
    assert(cores[1].nes_cpu.irq_pending == 0xa5);
    ++reads;
    check_state();
    return reference;
}

static void check_segment(void) {
    const unsigned quarter = (unsigned)(cores[0].nes_apu.clock_count & 3u);
    const size_t begin = quarter * cores[0].timing.samples_per_frame / 4;
    const size_t end = (quarter + 1) * cores[0].timing.samples_per_frame / 4;
    for (active = 0; active < 2; ++active) {
        traces[active].count = 0;
        sound_calls[active] = 0;
        last_sound_size[active] = 0;
        nes_apu_frame(&cores[active]);
    }
    ++segments;
    assert(traces[0].count == traces[1].count);
    if (traces[0].count)
        assert(memcmp(traces[0].events, traces[1].events,
                      traces[0].count * sizeof(DmaEvent)) == 0);
    dma_reads += traces[0].count;
    check_state();
    if (!cores[1].host_pcm_enabled) {
        assert(sound_calls[1] == 0);
        assert(cores[1].host_pcm_valid == 0);
    } else {
        assert(memcmp(cores[0].nes_apu.sample_buffer + begin,
                      cores[1].nes_apu.sample_buffer + begin, end - begin) == 0);
        if (sound_calls[1]) {
            assert(quarter == 3 && cores[1].host_pcm_valid == 15);
            assert(sound_calls[0] == 1 && sound_calls[1] == 1);
            assert(last_sound_size[0] == last_sound_size[1]);
            assert(memcmp(last_sound[0], last_sound[1], last_sound_size[0]) == 0);
            ++pcm_outputs;
        } else if (quarter == 3) {
            assert(cores[1].host_pcm_valid != 15);
        }
    }
}

static void enable_pcm(void) { cores[1].host_pcm_enabled = 1; }
static void disable_pcm(void) { cores[1].host_pcm_enabled = 0; }

static void configure_channels(void) {
    write_both(0x4015, 0x1f);
    write_both(0x4000, 0xff);
    write_both(0x4001, 0);
    write_both(0x4002, 0x36);
    write_both(0x4003, 0x18);
    write_both(0x4004, 0x7b);
    write_both(0x4005, 0);
    write_both(0x4006, 0x47);
    write_both(0x4007, 0x20);
    write_both(0x4008, 0xff);
    write_both(0x400a, 2);
    write_both(0x400b, 0x18);
    write_both(0x400c, 0x3f);
    write_both(0x400e, 0);
    write_both(0x400f, 0x18);
}

static void check_noise(void) {
    const uint32_t accumulators[] = {0, 1, 65535, 65536, 0xffff0000u, 0xffffffffu};
    for (int pal = 0; pal < 2; ++pal) {
        for (int mode = 0; mode < 2; ++mode) {
            for (unsigned period = 0; period < 16; ++period) {
                for (unsigned a = 0; a < sizeof(accumulators) / sizeof(accumulators[0]); ++a) {
                    prepare(pal);
                    configure_channels();
                    write_both(0x400e, (uint8_t)(period | (mode << 7)));
                    for (int i = 0; i < 2; ++i) {
                        nes_apu_t* apu = &cores[i].nes_apu;
                        apu->pulse1.phase_acc = 0xfffffffdu;
                        apu->pulse2.phase_acc = 0x80000000u;
                        apu->triangle.phase_acc = 0xffffffffu;
                        apu->noise.lfsr_acc = accumulators[a];
                        apu->noise.lfsr = (uint16_t)(a % 2 ? 0xffff : 1);
                    }
                    check_segment();
                    check_segment();
                    write_both(0x4015, 0);
                    const uint32_t accumulator = cores[0].nes_apu.noise.lfsr_acc;
                    const uint16_t lfsr = cores[0].nes_apu.noise.lfsr;
                    check_segment();
                    assert(cores[0].nes_apu.noise.lfsr_acc == accumulator);
                    assert(cores[0].nes_apu.noise.lfsr == lfsr);
                    enable_pcm();
                    for (int i = 0; i < 8; ++i) check_segment();
                }
            }
        }
    }
}

static void check_periods(void) {
    const uint16_t periods[] = {0, 1, 2, 7, 8, 0x7ff, 0x800, 0xffff};
    for (int pal = 0; pal < 2; ++pal) {
        for (unsigned p = 0; p < sizeof(periods) / sizeof(periods[0]); ++p) {
            for (int enabled = 0; enabled < 2; ++enabled) {
                prepare(pal);
                configure_channels();
                for (int i = 0; i < 2; ++i) {
                    nes_apu_t* apu = &cores[i].nes_apu;
                    apu->pulse1.cur_period = periods[p];
                    apu->pulse2.cur_period = periods[p];
                    apu->triangle.cur_period = periods[p];
                    apu->pulse1.phase_acc = 0xfffffff0u;
                    apu->pulse2.phase_acc = 0xfffffedcu;
                    apu->triangle.phase_acc = 0xfffffffcu;
                    if (!enabled) {
                        apu->status = 0;
                        apu->triangle.linear_counter = 0;
                    }
                }
                for (int i = 0; i < 12; ++i) check_segment();
                enable_pcm();
                for (int i = 0; i < 8; ++i) check_segment();
            }
        }
    }
}

static void check_dmc(void) {
    for (int pal = 0; pal < 2; ++pal) {
        for (unsigned rate = 0; rate < 16; ++rate) {
            for (int loop = 0; loop < 2; ++loop) {
                for (int irq = 0; irq < 2; ++irq) {
                    prepare(pal);
                    configure_channels();
                    write_both(0x4010, (uint8_t)(rate | (loop << 6) | (irq << 7)));
                    write_both(0x4011, (uint8_t)(rate % 2 ? 0x7f : 0));
                    write_both(0x4012, 0xff);
                    write_both(0x4013, 0);
                    for (int i = 0; i < 2; ++i) {
                        dmc_t* dmc = &cores[i].nes_apu.dmc;
                        dmc->cur_address = 0xffff;
                        dmc->bytes_remaining = 3;
                        dmc->bits_remaining = 0;
                        dmc->silence = 0;
                        dmc->sample_buffer = (uint8_t)(rate % 2 ? 0xff : 0);
                        dmc->out_level = (uint8_t)(rate % 2 ? 127 : 0);
                    }
                    int wrapped = 0, saw_irq = 0;
                    for (int i = 0; i < 16; ++i) {
                        check_segment();
                        for (size_t e = 0; e < traces[0].count; ++e)
                            if (traces[0].events[e].address == 0x8000) wrapped = 1;
                        if (cores[0].nes_apu.dmc.irq_flag) saw_irq = 1;
                    }
                    assert(wrapped);
                    assert(loop ? !saw_irq : (irq ? saw_irq : !saw_irq));
                    const uint8_t status = read_both();
                    if (!loop && irq) assert(status & 0x80);
                    assert(cores[0].nes_apu.irq_line == 0);
                    write_both(0x4015, 0);
                    check_segment();
                    assert(!cores[0].nes_apu.dmc.enabled);
                    write_both(0x4015, 0x10);
                    check_segment();
                    assert(cores[0].nes_apu.dmc.enabled);
                    enable_pcm();
                    for (int i = 0; i < 8; ++i) check_segment();
                }
            }
        }
    }
    for (int pal = 0; pal < 2; ++pal) {
        for (int overflow = 0; overflow < 2; ++overflow) {
            prepare(pal);
            configure_channels();
            write_both(0x4010, 0x8f);
            for (int i = 0; i < 2; ++i) {
                dmc_t* dmc = &cores[i].nes_apu.dmc;
                dmc->timer_acc = overflow ? 0xfff80000u : 0xfffffff0u;
                dmc->cur_address = 0xffff;
                dmc->bytes_remaining = 3;
                dmc->bits_remaining = 0;
                dmc->silence = 0;
            }
            check_segment();
            read_both();
            enable_pcm();
            for (int i = 0; i < 8; ++i) check_segment();
        }
    }
    for (unsigned action = 1; action <= 2; ++action) {
        prepare(0);
        configure_channels();
        write_both(0x4010, 0x40);
        write_both(0x4013, 0);
        dma_action = action;
        for (int i = 0; i < 8; ++i) check_segment();
        enable_pcm();
        for (int i = 0; i < 8; ++i) check_segment();
    }
    prepare(0);
    configure_channels();
    for (int i = 0; i < 2; ++i) cores[i].nes_apu.dmc.timer_period = 0;
    check_segment();
    enable_pcm();
    for (int i = 0; i < 8; ++i) check_segment();
}

static void check_register_sequences(void) {
    const uint16_t addresses[] = {
        0x4000, 0x4001, 0x4002, 0x4003, 0x4004, 0x4005, 0x4006, 0x4007,
        0x4008, 0x400a, 0x400b, 0x400c, 0x400e, 0x400f, 0x4010, 0x4011,
        0x4012, 0x4013, 0x4015, 0x4017
    };
    for (int pal = 0; pal < 2; ++pal) {
        prepare(pal);
        configure_channels();
        for (unsigned i = 0; i < 10000; ++i) {
            if (i % 37 == 0) disable_pcm();
            if (i % 37 == 13) enable_pcm();
            for (unsigned w = 0; w < 3; ++w)
                write_both(addresses[next_random() % (sizeof(addresses) / sizeof(addresses[0]))],
                           (uint8_t)next_random());
            if (i % 5 == 0) read_both();
            check_segment();
        }
        enable_pcm();
        for (int i = 0; i < 8; ++i) check_segment();
    }
}

static void check_pcm_resume(void) {
    const uint16_t lengths[] = {0, 1, 3, 7, 399, 400, 401, 479, 480};
    for (unsigned length = 0; length < sizeof(lengths) / sizeof(lengths[0]); ++length) {
        for (unsigned start = 0; start < 4; ++start) {
            prepare(0);
            configure_channels();
            for (int i = 0; i < 2; ++i) {
                cores[i].timing.samples_per_frame = lengths[length];
                cores[i].nes_apu.clock_count = UINT64_MAX - start;
            }
            for (int i = 0; i < 5; ++i) check_segment();
            enable_pcm();
            const uint64_t before = pcm_outputs;
            for (int i = 0; i < 12; ++i) check_segment();
            assert(pcm_outputs > before);
            disable_pcm();
            check_segment();
            assert(cores[1].host_pcm_valid == 0);
            enable_pcm();
            for (int i = 0; i < 8; ++i) check_segment();
        }
    }
}

int main(void) {
    check_noise();
    check_periods();
    check_dmc();
    check_register_sequences();
    check_pcm_resume();
    printf("APU PCM differential checks passed: %llu segments, %llu writes, "
           "%llu status reads, %llu DMA reads, %llu resumed outputs\n",
           (unsigned long long)segments, (unsigned long long)writes,
           (unsigned long long)reads, (unsigned long long)dma_reads,
           (unsigned long long)pcm_outputs);
    for (int i = 0; i < 2; ++i) free(traces[i].events);
    return 0;
}
