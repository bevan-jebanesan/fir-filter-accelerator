# FIR Filter Hardware Accelerator (SystemC + High-Level Synthesis)

A 16-tap, 16-bit FIR filter accelerator written in SystemC, synthesized to RTL with Siemens Catapult HLS, and driven by a RISC-V core through DMA in a full system simulation (Spike ISA simulator + SystemC TLM platform, then Verilog co-simulation of the HLS output).

## Architecture

```
 RISC-V (Spike)  ──MMIO──►  DMA engine  ──►  Accelerator
    fir.c                   0x70000000       0x70010000
                                              ├─ CTRL  : command register
                                              ├─ W     : coefficient stream  (64-bit, 4 taps / beat)
                                              ├─ X     : sample stream       (64-bit, 4 samples / beat)
                                              └─ Z     : result stream       (64-bit, 4 outputs / beat)
```

The accelerator is a single clocked `SC_THREAD` using MatchLib latency-insensitive `Connections` channels, so the same source runs in SystemC simulation and goes straight into HLS.

| Command | Action |
|---------|--------|
| `0x10` | Load 16 coefficients (4 beats), echo them back, clear sample history |
| `0x20` | Start a new filter: clear history, filter a 16-sample batch |
| `0x21` | Continue: filter the next 16-sample batch using the saved 15-sample history |

The history buffer lets a long input stream be processed as back-to-back 16-sample batches with no seams between them. The C driver in `sw/fir.c` programs the DMA to move coefficients and samples into the accelerator and pull results back, then checks every output against a golden reference (`sw/expected.inc`) across two filters of 32 and 48 time-steps.

## Micro-architecture choices

- All 16 multiply-accumulates for an output are fully unrolled (`hls_unroll`), and four outputs are computed per result beat.
- The batch loop is pipelined (`hls_pipeline_init_interval 1`) and the main loop gets a pipelining directive in `hls/go_hls.tcl`.
- Coefficients and the sample history live in registers, so there is no memory port contention.

## HLS results (5 ns clock target)

| Metric | Value |
|--------|-------|
| Critical path | 4.81 ns (+0.19 ns slack) |
| Latency / throughput | 18 / 16 cycles per command |
| Real operations | 1,094 |
| Total area score (after assignment) | 48,798 |

Earlier iterations with fewer parallel MACs had a smaller area score but were slower; the final version trades area for a fully parallel datapath. Full numbers are in `hls/results_final.csv` and `reports/hls_summary.rpt`.

## Repository layout

```
sc/        Accelerator.h    SystemC source of the accelerator (HLS input)
sw/        fir.c            RISC-V bare-metal driver + golden test vectors
hls/       go_hls.tcl       Catapult directives, final results row
reports/   hls_summary.rpt  Latency, area and critical path from Catapult
```

The surrounding SoC platform (DMA, memory controller, TLM bridges) and the MatchLib/Catapult flow scripts are third-party and not included.

## Tools

SystemC / TLM 2.0 · NVIDIA MatchLib · Siemens Catapult HLS · Synopsys VCS · Spike RISC-V ISA simulator · C
