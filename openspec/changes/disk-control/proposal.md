# Implement SET_DISK_CONTROL_EXT_INTERFACE for Multi-Disc Games

## Problem
Core queries disk control interface for multi-disc games (eject/swap). Returns 0 → multi-disc games broken.

## Solution
Handle SET_DISK_CONTROL_EXT_INTERFACE in retro_env. Implement all 10 callbacks.

## Specs
- openspec/specs/platform/spec.md

## Affected Files
- dosbox-uwp/Content/RetroCore.cpp
- dosbox-uwp/Content/RetroCore.h
