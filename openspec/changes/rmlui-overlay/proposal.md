---
id: rmlui-overlay
title: "Integrate RMLUI as modern overlay GUI"
status: proposed
---

# Integrate RMLUI as modern overlay GUI

## Problem
Current UX is bare: Windows file picker, no config UI, no visual save management. Puremenu OSD is functional but text-mode.

## Solution
Integrate RMLUI (Rocket Motion Library UI) as D2D overlay on top of DOSBox framebuffer. Build visual ROM picker, config UI, virtual keyboard, save manager.

## Spec
openspec/specs/ux/spec.md. Requires many other specs implemented first.

## Files
- New: RmlOverlay class (integration wrapper)
- Requires RMLUI library added to build
