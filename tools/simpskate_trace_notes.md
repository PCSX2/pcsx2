# SimpSkate Trace Build Notes

This build includes targeted trace hooks intended for Simpsons Skateboarding load-chain reconstruction.

## Trace environment variables

- `PCSX2_SIMPTRACE=1`
- `PCSX2_SIMPTRACE_OUT=<path to jsonl>`
- optional:
  - `PCSX2_SIMPTRACE_EE_FUNCS=0x00131410,0x00131510,0x0013e510,0x0011c4d8,0x002d5150,0x0030fd00`

## Fast launch helper

Use:

```powershell
powershell -ExecutionPolicy Bypass -File .\simpskate\run_simpskate_trace.ps1
```

from the artifact root (or pass `-Pcsx2Exe` explicitly).

## What gets logged

- EE tracepoint hits for loader-related functions.
- ISO open/map metadata.
- Aggregated ISO read runs with:
  - start LSN
  - sector count
  - mode
  - current EE PC / IOP PC
  - owning file path and file-relative offset when available

## Analyze trace

```powershell
python .\simpskate\analyze_simpskate_trace.py <trace.jsonl>
```

