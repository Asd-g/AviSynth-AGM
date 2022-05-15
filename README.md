## Description

Adaptive Grain Mask.

Generally, the lower a frame's average luma, the more grain is applied even to the brighter areas. This abuses the fact that our eyes are instinctively drawn to the brighter part of any image, making the grain less necessary in images with an overall very high luma.

This is [a port of the VapourSynth adaptive_grain mask](https://git.kageru.moe/kageru/adaptivegrain).

### Requirements:

- AviSynth 2.60 / AviSynth+ 3.4 or later

- Microsoft VisualC++ Redistributable Package 2022 (can be downloaded from [here](https://github.com/abbodi1406/vcredist/releases)) (Windows only)

### Usage:

```
AGM (clip input, float "luma_scaling", bool "fade", int "opt")
```

### Parameters:

- input\
    A clip to process.\
    Must be in YUV planar format.

- luma_scaling\
    Grain opacity curve.\
    Lower values will generate more grain even in brighter scenes while higher values will generate less even in dark scenes.\
    Default: 10.0.

- fade\
    If the clip has bit depth less than 32-bit, the range must be tv.\
    True: Pure white and pure black pixels are copied (16/235 8-bit). Pixels with value of 18/17 (8-bit) fades out.\
    Default: True.

- opt\
    Sets which cpu optimizations to use.\
    -1: Auto-detect.\
    0: Use C++ code.\
    1: Use SSE2 code.\
    2: Use AVX2 code.\
    3: Use AVX512 code.\
    Default: -1.

### Building:

- Windows\
    Use solution files.

- Linux
    ```
    Requirements:
        - Git
        - C++17 compiler
        - CMake >= 3.16
    ```
    ```
    git clone https://github.com/Asd-g/AviSynth-AGM && \
    cd AviSynth-AGM && \
    mkdir build && \
    cd build && \

    cmake ..
    make -j$(nproc)
    sudo make install
    ```
