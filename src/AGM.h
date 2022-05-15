#pragma once

#include <vector>

#include "avisynth.h"

class AGM : public GenericVideoFilter
{
    float luma_scaling;
    std::vector<float> lut;
    bool v8;

    void (*process)(PVideoFrame& dst, PVideoFrame& src, const float luma_scaling, std::vector<float>& lut, IScriptEnvironment* env) noexcept;

public:
    AGM(PClip child, float luma_scaling_, bool fade, int opt, IScriptEnvironment* env);
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;

    int __stdcall SetCacheHints(int cachehints, int frame_range) override
    {
        return cachehints == CACHE_GET_MTMODE ? MT_MULTI_INSTANCE : 0;
    }
};

template <typename T, bool fade, int peak, int ymin, int y1, int y2, int ymax, int d0, int d1>
void process_sse2(PVideoFrame& dst, PVideoFrame& src, const float luma_scaling, std::vector<float>& lut, IScriptEnvironment* env) noexcept;
template <typename T, bool fade, int peak, int ymin, int y1, int y2, int ymax, int d0, int d1>
void process_avx2(PVideoFrame& dst, PVideoFrame& src, const float luma_scaling, std::vector<float>& lut, IScriptEnvironment* env) noexcept;
template <typename T, bool fade, int peak, int ymin, int y1, int y2, int ymax, int d0, int d1>
void process_avx512(PVideoFrame& dst, PVideoFrame& src, const float luma_scaling, std::vector<float>& lut, IScriptEnvironment* env) noexcept;
