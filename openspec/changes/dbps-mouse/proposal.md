# Implement DBPS_GetMouse with real mouse events

## Problem
DBPS_GetMouse returns 0,0. No mouse events connected. Core can't get relative mouse for DOS games or absolute pointer for OSD.

## Solution
Connect CoreWindow PointerPressed/Moved/Released events. Implement DBPS_GetMouse. Route to RETRO_DEVICE_MOUSE and POINTER.

## Specs
- openspec/specs/input/spec.md
- openspec/specs/platform/spec.md

## Affected Files
- dosbox-uwp/App.cpp
- dosbox-uwp/Content/dosbox_pure_sta.cpp
- dosbox-uwp/Content/RetroCore.cpp
- dosbox-uwp/Content/RetroCore.h
