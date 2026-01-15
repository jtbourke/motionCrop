# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

MotionCrop is a Windows C++ application that automatically stabilizes and crops aircraft videos. It uses OpenCV to detect and track aircraft in video footage, then crops each frame to keep the aircraft centered in the output.

## Build Commands

Requires [vcpkg](https://github.com/microsoft/vcpkg) with system-wide integration (`vcpkg integrate install`).

```bash
msbuild "src/MotionCrop.sln" -p:Configuration=Release -p:Platform=x64
```

For Debug builds:
```bash
msbuild "src/MotionCrop.sln" -p:Configuration=Debug -p:Platform=x64
```

First build will be slow as vcpkg downloads and compiles OpenCV.

## Dependencies

- **OpenCV**: Managed via vcpkg (see `vcpkg.json`)
- **vcpkg**: Run `vcpkg integrate install` for MSBuild integration

## Architecture

Single-file application (`src/main.cpp`) with straightforward pipeline:

1. Read video frame via `VideoCapture`
2. Convert BGR → HSV, extract Value channel
3. Compute Otsu threshold → Canny edge detection
4. Morphological dilation to close edges
5. Find contours, select largest (aircraft)
6. Calculate centroid via image moments
7. Crop ROI around centroid
8. Write to output video with DivX codec

## Usage

```
motioncrop filename [windowSize] [threshold] [iterations] [verbose]
```

- `windowSize`: Output crop size in pixels (default: 400)
- `threshold`: Edge detection threshold scalar (default: 1.0)
- `iterations`: Dilation iterations (default: 2)
- `verbose`: Debug mode 0/1 (default: 0)

Output: `{filename}_mcrop.avi`
