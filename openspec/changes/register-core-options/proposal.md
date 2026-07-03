# Register core options: SET_CORE_OPTIONS_V2 + GET_VARIABLE

## Problem
Core calls `SET_CORE_OPTIONS_V2` but `RetroCore` returns 0 (unsupported). `GET_VARIABLE` also returns 0. Core runs with hardcoded defaults — no options configurable. Launcher menu (Puremenu) can't read or change settings.

## Solution
Handle `SET_CORE_OPTIONS_V2` by storing the option definitions. Handle `GET_VARIABLE` by returning stored values. Handle `GET_VARIABLE_UPDATE` for change notifications.

## Spec
`openspec/specs/platform/spec.md` (options section)

## Files
- `RetroCore.cpp`
- Knowledge of `core_options.h` (~52 vars across 6 categories)
