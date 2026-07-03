---
id: save-states
title: "Implement save states: serialize, unserialize, DBPS stubs"
status: proposed
---

# Implement save states: serialize, unserialize, DBPS stubs

## Problem
`retro_serialize`/`retro_unserialize` exist in core but frontend never calls them. `DBPS_RequestSaveLoad` and `DBPS_HaveSaveSlot` are no-ops. No save/load functionality.

## Solution
Call `retro_serialize` after RunFrame or on demand. Store to LocalFolder. Implement DBPS stubs for slot management. F5/F7 hotkeys.

## Spec
openspec/specs/save-states/spec.md

## Files
- dosbox_pure_sta.cpp
- Content/RetroCore.cpp/.h
- dosbox_uwpMain.cpp
