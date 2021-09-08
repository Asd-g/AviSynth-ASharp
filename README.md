## Description

ASharp is an adaptive sharpening filter.

Only the luma plane will be processed.

This is [a port of the VapourSynth plugin ASharp](https://github.com/dubhater/vapoursynth-asharp).

### Requirements:

- AviSynth 2.60 / AviSynth+ 3.4 or later

- Microsoft VisualC++ Redistributable Package 2022 (can be downloaded from [here](https://github.com/abbodi1406/vcredist/releases))

### Usage:

```
ASharp (clip, float "T", float "D", float "B", bool "hqbf")
```

### Parameters:

- clip\
    A clip to process. It must be in YUV 8..16-bit planar format.
    
- T\
    Unsharp masking threshold.\
    0 will do nothing, 1 will enhance contrast 1x.\
    Must be between 0 and 32.\
    Default: 2.0.

- D\
    Adaptive sharpening strength to avoid sharpening noise.\
    If greater than 0, the threshold is adapted for each pixel (bigger for edges) and t acts like a maximum.\
    Set to 0 to disable it.\
    Must be between 0 and 16.\
    Default: 4.0.
    
- B\
    Block adaptive sharpening.\
    It avoids sharpening the edges of DCT blocks by lowering the threshold around them.\
    Use with blocky videos. If cropping the video before ASharp the top and left edges must be cropped by multiples of 8.\
    Set to a negative value to disable it.\
    Must be less than 4.\
    Default: -1.0.
    
- hqbf\
    High quality block filtering.\
    Default: False.
