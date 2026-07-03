# Implement DBPS Platform Stubs

## Problem
All DBPS_* stubs return zeros/false. Core needs real info: gamepad presence, joypad bindings, config overrides, OSD frame submission.

## Solution
Implement each stub with real frontend queries.

## Specs
- openspec/specs/platform/spec.md

## Affected Files
- dosbox-uwp/Content/dosbox_pure_sta.cpp
- dosbox-uwp/Content/SdlInput.h
- dosbox-uwp/Content/RetroCore.cpp
- dosbox-uwp/Content/RetroCore.h
