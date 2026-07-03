# Implement GET_CURRENT_SOFTWARE_FRAMEBUFFER for OSD overlay

## Problem
Core OSD (Puremenu) needs to draw overlay on top of game framebuffer. Without software framebuffer exposure, OSD can't composite correctly — overlay renders blind or not at all.

## Solution
Handle `GET_CURRENT_SOFTWARE_FRAMEBUFFER` in `retro_env` by returning pointer to current SW framebuffer. Core reads framebuffer pixels, draws OSD text/menus, then calls `video_cb` with the modified buffer.

## Spec
`openspec/specs/video/spec.md`

## Files
- `RetroCore.cpp`
