---
id: video-enhancements
title: "Video enhancements: scale modes and core video options"
status: proposed
---

# Video enhancements: scale modes and core video options

## Problem
Current renderer only does stretch-to-fit. No pixel-perfect, integer, or aspect-ratio modes. Core video options (machine, cga, svga, aspect, overscan) are not exposed.

## Solution
Add scale mode selection to RetroScreenRenderer. Expose video options via GET_VARIABLE.

## Spec
openspec/specs/video/spec.md

## Files
- Content/RetroScreenRenderer.cpp/.h (scale modes)
- Content/register-core-options provides the options
