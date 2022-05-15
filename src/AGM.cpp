#include <algorithm>
#include <cmath>

#include "AGM.h"

template <typename T, int peak>
AVS_FORCEINLINE float average_plane_c(const T* srcp, const int src_pitch, const int width, const int height) noexcept
{
    typedef typename std::conditional < sizeof(T) == 4, float, int64_t>::type sum_t;
    sum_t accum{ 0 }; // int32 holds sum of maximum 16 Mpixels for 8 bit, and 65536 pixels for uint16_t pixels

    for (size_t y{ 0 }; y < height; ++y)
    {
        for (size_t x{ 0 }; x < width; ++x)
            accum += srcp[x];

        srcp += src_pitch;
    }

    if constexpr (std::is_integral_v<T>)
        return (static_cast<float>(accum) / (height * width)) / peak;
    else
        return accum / (height * width);
}

template <typename T, bool fade, int peak, int ymin, int y1, int y2, int ymax, int d0, int d1>
void process_c(PVideoFrame& dst, PVideoFrame& src, const float luma_scaling, std::vector<float>& lut, IScriptEnvironment* env) noexcept
{
    const int height{ src->GetHeight() };

    env->BitBlt(dst->GetWritePtr(), dst->GetPitch(), src->GetReadPtr(), src->GetPitch(), src->GetRowSize(), height);

    const size_t src_pitch{ src->GetPitch() / sizeof(T) };
    const size_t dst_pitch{ dst->GetPitch() / sizeof(T) };
    const size_t width{ src->GetRowSize() / sizeof(T) };
    const T* srcp{ reinterpret_cast<const T*>(src->GetReadPtr()) };
    T* __restrict dstp{ reinterpret_cast<T*>(dst->GetWritePtr()) };

    const float avg{ average_plane_c<T, peak>(srcp, src_pitch, width, height) };
    const float temp{ avg * avg * luma_scaling };

    for (int y{ 0 }; y < height; ++y)
    {
        for (size_t x{ 0 }; x < width; ++x)
        {
            if constexpr (std::is_integral_v<T>)
            {
                if constexpr (fade)
                {
                    if (srcp[x] <= ymin)
                        continue;
                    else if (srcp[x] <= y1)
                    {
                        dstp[x] = d0;
                        continue;
                    }
                    else if (srcp[x] <= y2)
                    {
                        dstp[x] = d1;
                        continue;
                    }
                    else if (srcp[x] >= ymax)
                    {
                        dstp[x] = 0;
                        continue;
                    }
                }

                dstp[x] = std::clamp(static_cast<int>(std::pow(lut[srcp[x]], temp) * peak + 0.5f), 0, peak);
            }
            else
            {
                if constexpr (fade)
                {
                    if (srcp[x] == 0.0f)
                        continue;
                    else if (srcp[x] == 1.0f)
                    {
                        dstp[x] = 0.0f;
                        continue;
                    }
                }

                dstp[x] = std::clamp(std::pow(1.0f - (srcp[x] * ((srcp[x] * ((srcp[x] * ((srcp[x] * ((srcp[x] * 18.188f) - 45.47f)) + 36.624f)) - 9.466f)) + 1.124f)), temp), 0.0f, 1.0f);
            }
        }

        srcp += src_pitch;
        dstp += dst_pitch;
    }
}

AGM::AGM(PClip child, float luma_scaling_, bool fade, int opt, IScriptEnvironment* env)
    : GenericVideoFilter(child), luma_scaling(luma_scaling_), v8(true)
{
    if (!vi.IsPlanar())
        env->ThrowError("AGM: only planar input is supported!");
    if (vi.IsRGB())
        env->ThrowError("AGM: only YUV input is supported!");
    if (opt < -1 || opt > 3)
        env->ThrowError("AGM: opt must be between - 1..3.");

    const bool avx512{ !!(env->GetCPUFlags() & CPUF_AVX512F) };
    const bool avx2{ !!(env->GetCPUFlags() & CPUF_AVX2) };
    const bool sse2{ !!(env->GetCPUFlags() & CPUF_SSE2) };

    if (!avx512 && opt == 3)
        env->ThrowError("AGM: opt=3 requires AVX512F.");
    if (!avx2 && opt == 2)
        env->ThrowError("AGM: opt=2 requires AVX2.");
    if (!sse2 && opt == 1)
        env->ThrowError("AGM: opt=1 requires SSE2.");

    if ((avx512 && opt < 0) || opt == 3)
    {
        switch (vi.BitsPerComponent())
        {
            case 8:
            {
                process = (fade) ? process_avx512<uint8_t, true, 255, 16, 17, 18, 235, 85, 170> : process_avx512<uint8_t, false, 255, 16, 17, 18, 235, 85, 170>;
                vi.pixel_type = VideoInfo::CS_Y8;
                break;
            }
            case 10:
            {
                process = (fade) ? process_avx512<uint16_t, true, 1023, 64, 68, 72, 940, 340, 680> : process_avx512<uint16_t, false, 1023, 64, 68, 72, 940, 340, 680>;
                vi.pixel_type = VideoInfo::CS_Y10;
                break;
            }
            case 12:
            {
                process = (fade) ? process_avx512<uint16_t, true, 4095, 256, 272, 288, 3760, 1360, 2720> : process_avx512<uint16_t, false, 4095, 256, 272, 288, 3760, 1360, 2720>;
                vi.pixel_type = VideoInfo::CS_Y12;
                break;
            }
            case 14:
            {
                process = (fade) ? process_avx512<uint16_t, true, 16383, 1024, 1088, 1152, 15040, 5440, 10880> : process_avx512<uint16_t, false, 16383, 1024, 1088, 1152, 15040, 5440, 10880>;
                vi.pixel_type = VideoInfo::CS_Y14;
                break;
            }
            case 16:
            {
                process = (fade) ? process_avx512<uint16_t, true, 65535, 4096, 4352, 4608, 60160, 21760, 43520> : process_avx512<uint16_t, false, 65535, 4096, 4352, 4608, 60160, 21760, 43520>;
                vi.pixel_type = VideoInfo::CS_Y16;
                break;
            }
            default:
            {
                process = (fade) ? process_avx512<float, true, 0, 0, 0, 0, 0, 0, 0> : process_avx512<float, false, 0, 0, 0, 0, 0, 0, 0>;
                vi.pixel_type = VideoInfo::CS_Y32;
                break;
            }
        }
    }
    else if ((avx2 && opt < 0) || opt == 2)
    {
        switch (vi.BitsPerComponent())
        {
            case 8:
            {
                process = (fade) ? process_avx2<uint8_t, true, 255, 16, 17, 18, 235, 85, 170> : process_avx2<uint8_t, false, 255, 16, 17, 18, 235, 85, 170>;
                vi.pixel_type = VideoInfo::CS_Y8;
                break;
            }
            case 10:
            {
                process = (fade) ? process_avx2<uint16_t, true, 1023, 64, 68, 72, 940, 340, 680> : process_avx2<uint16_t, false, 1023, 64, 68, 72, 940, 340, 680>;
                vi.pixel_type = VideoInfo::CS_Y10;
                break;
            }
            case 12:
            {
                process = (fade) ? process_avx2<uint16_t, true, 4095, 256, 272, 288, 3760, 1360, 2720> : process_avx2<uint16_t, false, 4095, 256, 272, 288, 3760, 1360, 2720>;
                vi.pixel_type = VideoInfo::CS_Y12;
                break;
            }
            case 14:
            {
                process = (fade) ? process_avx2<uint16_t, true, 16383, 1024, 1088, 1152, 15040, 5440, 10880> : process_avx2<uint16_t, false, 16383, 1024, 1088, 1152, 15040, 5440, 10880>;
                vi.pixel_type = VideoInfo::CS_Y14;
                break;
            }
            case 16:
            {
                process = (fade) ? process_avx2<uint16_t, true, 65535, 4096, 4352, 4608, 60160, 21760, 43520> : process_avx2<uint16_t, false, 65535, 4096, 4352, 4608, 60160, 21760, 43520>;
                vi.pixel_type = VideoInfo::CS_Y16;
                break;
            }
            default:
            {
                process = (fade) ? process_avx2<float, true, 0, 0, 0, 0, 0, 0, 0> : process_avx2<float, false, 0, 0, 0, 0, 0, 0, 0>;
                vi.pixel_type = VideoInfo::CS_Y32;
                break;
            }
        }
    }
    else if ((sse2 && opt < 0) || opt == 1)
    {
        switch (vi.BitsPerComponent())
        {
            case 8:
            {
                process = (fade) ? process_sse2<uint8_t, true, 255, 16, 17, 18, 235, 85, 170> : process_sse2<uint8_t, false, 255, 16, 17, 18, 235, 85, 170>;
                vi.pixel_type = VideoInfo::CS_Y8;
                break;
            }
            case 10:
            {
                process = (fade) ? process_sse2<uint16_t, true, 1023, 64, 68, 72, 940, 340, 680> : process_sse2<uint16_t, false, 1023, 64, 68, 72, 940, 340, 680>;
                vi.pixel_type = VideoInfo::CS_Y10;
                break;
            }
            case 12:
            {
                process = (fade) ? process_sse2<uint16_t, true, 4095, 256, 272, 288, 3760, 1360, 2720> : process_sse2<uint16_t, false, 4095, 256, 272, 288, 3760, 1360, 2720>;
                vi.pixel_type = VideoInfo::CS_Y12;
                break;
            }
            case 14:
            {
                process = (fade) ? process_sse2<uint16_t, true, 16383, 1024, 1088, 1152, 15040, 5440, 10880> : process_sse2<uint16_t, false, 16383, 1024, 1088, 1152, 15040, 5440, 10880>;
                vi.pixel_type = VideoInfo::CS_Y14;
                break;
            }
            case 16:
            {
                process = (fade) ? process_sse2<uint16_t, true, 65535, 4096, 4352, 4608, 60160, 21760, 43520> : process_sse2<uint16_t, false, 65535, 4096, 4352, 4608, 60160, 21760, 43520>;
                vi.pixel_type = VideoInfo::CS_Y16;
                break;
            }
            default:
            {
                process = (fade) ? process_sse2<float, true, 0, 0, 0, 0, 0, 0, 0> : process_sse2<float, false, 0, 0, 0, 0, 0, 0, 0>;
                vi.pixel_type = VideoInfo::CS_Y32;
                break;
            }
        }
    }
    else
    {
        switch (vi.BitsPerComponent())
        {
            case 8:
            {
                process = (fade) ? process_c<uint8_t, true, 255, 16, 17, 18, 235, 85, 170> : process_c<uint8_t, false, 255, 16, 17, 18, 235, 85, 170>;
                vi.pixel_type = VideoInfo::CS_Y8;
                break;
            }
            case 10:
            {
                process = (fade) ? process_c<uint16_t, true, 1023, 64, 68, 72, 940, 340, 680> : process_c<uint16_t, false, 1023, 64, 68, 72, 940, 340, 680>;
                vi.pixel_type = VideoInfo::CS_Y10;
                break;
            }
            case 12:
            {
                process = (fade) ? process_c<uint16_t, true, 4095, 256, 272, 288, 3760, 1360, 2720> : process_c<uint16_t, false, 4095, 256, 272, 288, 3760, 1360, 2720>;
                vi.pixel_type = VideoInfo::CS_Y12;
                break;
            }
            case 14:
            {
                process = (fade) ? process_c<uint16_t, true, 16383, 1024, 1088, 1152, 15040, 5440, 10880> : process_c<uint16_t, false, 16383, 1024, 1088, 1152, 15040, 5440, 10880>;
                vi.pixel_type = VideoInfo::CS_Y14;
                break;
            }
            case 16:
            {
                process = (fade) ? process_c<uint16_t, true, 65535, 4096, 4352, 4608, 60160, 21760, 43520> : process_c<uint16_t, false, 65535, 4096, 4352, 4608, 60160, 21760, 43520>;
                vi.pixel_type = VideoInfo::CS_Y16;
                break;
            }
            default:
            {
                process = (fade) ? process_c<float, true, 0, 0, 0, 0, 0, 0, 0> : process_c<float, false, 0, 0, 0, 0, 0, 0, 0>;
                vi.pixel_type = VideoInfo::CS_Y32;
                break;
            }
        }
    }

    if (vi.ComponentSize() < 4)
    {
        const int range_max{ 1 << vi.BitsPerComponent() };
        const float peak{ static_cast<float>(range_max - 1) };
        lut.reserve(range_max);
        for (int i{ 0 }; i < range_max; ++i)
        {
            const float x{ i / peak };
            lut.emplace_back(1.0f - (x * ((x * ((x * ((x * ((x * 18.188f) - 45.47f)) + 36.624f)) - 9.466f)) + 1.124f)));
        }
    }

    try { env->CheckVersion(8); }
    catch (const AvisynthError&) { v8 = false; }
}

PVideoFrame __stdcall AGM::GetFrame(int n, IScriptEnvironment* env)
{
    PVideoFrame src{ child->GetFrame(n, env) };
    PVideoFrame dst{ (v8) ? env->NewVideoFrameP(vi, &src) : env->NewVideoFrame(vi) };

    process(dst, src, luma_scaling, lut, env);

    return dst;
}

AVSValue __cdecl Create_AGM(AVSValue args, void*, IScriptEnvironment* env)
{
    enum { CLIP, LUMA_SC, FADE, OPT };

    return new AGM(args[CLIP].AsClip(), args[LUMA_SC].AsFloatf(10.0f), args[FADE].AsBool(true), args[OPT].AsInt(-1), env);

}

const AVS_Linkage* AVS_linkage = nullptr;

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit3(IScriptEnvironment * env, const AVS_Linkage* const vectors)
{
    AVS_linkage = vectors;

    env->AddFunction("AGM", "c[luma_scaling]f[fade]b[opt]i", Create_AGM, 0);
    return "AGM";
}
