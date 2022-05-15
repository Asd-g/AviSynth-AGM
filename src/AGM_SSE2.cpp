#include "AGM.h"
#include "VCL2/vectorclass.h"
#include "VCL2/vectormath_exp.h"

template <typename T, int peak>
AVS_FORCEINLINE float average_plane_sse2(const T* srcp, const int src_pitch, const int width, const int height) noexcept
{
    Vec4f accum{ zero_4f() };

    if constexpr (std::is_same_v<T, uint8_t>)
    {
        for (int y{ 0 }; y < height; ++y)
        {
            for (int x{ 0 }; x < width; x += 4)
                accum += to_float(Vec4i().load_4uc(srcp + x));

            srcp += src_pitch;
        }

        accum = (accum / (width * height)) / peak;
    }
    else if constexpr (std::is_same_v<T, uint16_t>)
    {
        for (int y{ 0 }; y < height; ++y)
        {
            for (int x{ 0 }; x < width; x += 4)
                accum += to_float(Vec4i().load_4us(srcp + x));

            srcp += src_pitch;
        }

        accum = (accum / (width * height)) / peak;
    }
    else
    {
        for (int y{ 0 }; y < height; ++y)
        {
            for (int x{ 0 }; x < width; x += 4)
                accum += Vec4f().load(srcp + x);

            srcp += src_pitch;
        }

        accum = accum / (width * height);
    }

    return horizontal_add(accum);
}

template <typename T, bool fade, int peak, int ymin, int y1, int y2, int ymax, int d0, int d1>
void process_sse2(PVideoFrame& dst, PVideoFrame& src, const float luma_scaling, std::vector<float>& lut, IScriptEnvironment* env) noexcept
{
    const int height{ src->GetHeight() };

    env->BitBlt(dst->GetWritePtr(), dst->GetPitch(), src->GetReadPtr(), src->GetPitch(), src->GetRowSize(), height);

    const size_t src_pitch{ src->GetPitch() / sizeof(T) };
    const size_t dst_pitch{ dst->GetPitch() / sizeof(T) };
    const size_t width{ src->GetRowSize() / sizeof(T) };
    const T* srcp{ reinterpret_cast<const T*>(src->GetReadPtr()) };
    T* __restrict dstp{ reinterpret_cast<T*>(dst->GetWritePtr()) };

    const float avg = average_plane_sse2<T, peak>(srcp, src_pitch, width, height);
    const Vec4f temp{ avg * avg * luma_scaling };

    for (int y{ 0 }; y < height; ++y)
    {
        for (size_t x{ 0 }; x < width; x += 4)
        {
            if constexpr (std::is_same_v<T, uint8_t>)
            {
                Vec4f srcp_d;
                srcp_d.insert(0, lut[srcp[x]]);
                srcp_d.insert(1, lut[srcp[x + 1]]);
                srcp_d.insert(2, lut[srcp[x + 2]]);
                srcp_d.insert(3, lut[srcp[x + 3]]);

                if constexpr (fade)
                {
                    const auto srcp_vi{ Vec16uc().load(srcp + x) };

                    select(!(srcp_vi > ymin), Vec16uc().load(srcp + x),
                        select(!(srcp_vi != y1), Vec16uc(d0),
                            select(!(srcp_vi != y2), Vec16uc(d1),
                                select(!(srcp_vi < ymax), zero_si128(),
                                    compress_saturated_s2u(compress_saturated(min(max(truncatei(pow(srcp_d, temp) * peak + 0.5f), zero_si128()), peak), zero_si128()), zero_si128()))))).store_si32(dstp + x);
                }
                else
                    compress_saturated_s2u(compress_saturated(min(max(truncatei(pow(srcp_d, temp) * peak + 0.5f), zero_si128()), peak), zero_si128()), zero_si128()).store_si32(dstp + x);
            }
            else if constexpr (std::is_same_v<T, uint16_t>)
            {
                Vec4f srcp_d;
                srcp_d.insert(0, lut[srcp[x]]);
                srcp_d.insert(1, lut[srcp[x + 1]]);
                srcp_d.insert(2, lut[srcp[x + 2]]);
                srcp_d.insert(3, lut[srcp[x + 3]]);

                if constexpr (fade)
                {
                    const auto srcp_vi{ Vec8us().load(srcp + x) };

                    select(!(srcp_vi > ymin), Vec8us().load(srcp + x),
                        select(!(srcp_vi != y1), Vec8us(d0),
                            select(!(srcp_vi != y2), Vec8us(d1),
                                select(!(srcp_vi < ymax), zero_si128(),
                                    compress_saturated_s2u(min(max(truncatei(pow(srcp_d, temp) * peak + 0.5f), zero_si128()), peak), zero_si128()))))).storel(dstp + x);
                }
                else
                    compress_saturated_s2u(min(max(truncatei(pow(srcp_d, temp) * peak + 0.5f), zero_si128()), peak), zero_si128()).storel(dstp + x);
            }
            else
            {
                const auto srcp_d{ Vec4f().load(srcp + x) };

                if constexpr (fade)
                    select(!(srcp_d != 0.0f), srcp_d,
                        select(!(srcp_d != 1.0f), Vec4f(0.0f),
                            min(max(pow(1.0f - (srcp_d * ((srcp_d * ((srcp_d * ((srcp_d * ((srcp_d * 18.188f) - 45.47f)) + 36.624f)) - 9.466f)) + 1.124f)),
                                avg * avg * luma_scaling), zero_4f()), 1.0f))).store_nt(dstp + x);
                else
                    min(max(pow(1.0f - (srcp_d * ((srcp_d * ((srcp_d * ((srcp_d * ((srcp_d * 18.188f) - 45.47f)) + 36.624f)) - 9.466f)) + 1.124f)),
                        avg * avg * luma_scaling), zero_4f()), 1.0f).store_nt(dstp + x);
            }
        }

        srcp += src_pitch;
        dstp += dst_pitch;
    }
}

template void process_sse2<uint8_t, true, 255, 16, 17, 18, 235, 85, 170>(PVideoFrame& dst, PVideoFrame& src, const float luma_scaling, std::vector<float>& lut, IScriptEnvironment* env) noexcept;
template void process_sse2<uint8_t, false, 255, 16, 17, 18, 235, 85, 170>(PVideoFrame& dst, PVideoFrame& src, const float luma_scaling, std::vector<float>& lut, IScriptEnvironment* env) noexcept;

template void process_sse2<uint16_t, true, 1023, 64, 68, 72, 940, 340, 680>(PVideoFrame& dst, PVideoFrame& src, const float luma_scaling, std::vector<float>& lut, IScriptEnvironment* env) noexcept;
template void process_sse2<uint16_t, false, 1023, 64, 68, 72, 940, 340, 680>(PVideoFrame& dst, PVideoFrame& src, const float luma_scaling, std::vector<float>& lut, IScriptEnvironment* env) noexcept;

template void process_sse2<uint16_t, true, 4095, 256, 272, 288, 3760, 1360, 2720>(PVideoFrame& dst, PVideoFrame& src, const float luma_scaling, std::vector<float>& lut, IScriptEnvironment* env) noexcept;
template void process_sse2<uint16_t, false, 4095, 256, 272, 288, 3760, 1360, 2720>(PVideoFrame& dst, PVideoFrame& src, const float luma_scaling, std::vector<float>& lut, IScriptEnvironment* env) noexcept;

template void process_sse2<uint16_t, true, 16383, 1024, 1088, 1152, 15040, 5440, 10880>(PVideoFrame& dst, PVideoFrame& src, const float luma_scaling, std::vector<float>& lut, IScriptEnvironment* env) noexcept;
template void process_sse2<uint16_t, false, 16383, 1024, 1088, 1152, 15040, 5440, 10880>(PVideoFrame& dst, PVideoFrame& src, const float luma_scaling, std::vector<float>& lut, IScriptEnvironment* env) noexcept;

template void process_sse2<uint16_t, true, 65535, 4096, 4352, 4608, 60160, 21760, 43520>(PVideoFrame& dst, PVideoFrame& src, const float luma_scaling, std::vector<float>& lut, IScriptEnvironment* env) noexcept;
template void process_sse2<uint16_t, false, 65535, 4096, 4352, 4608, 60160, 21760, 43520>(PVideoFrame& dst, PVideoFrame& src, const float luma_scaling, std::vector<float>& lut, IScriptEnvironment* env) noexcept;

template void process_sse2<float, true, 0, 0, 0, 0, 0, 0, 0>(PVideoFrame& dst, PVideoFrame& src, const float luma_scaling, std::vector<float>& lut, IScriptEnvironment* env) noexcept;
template void process_sse2<float, false, 0, 0, 0, 0, 0, 0, 0>(PVideoFrame& dst, PVideoFrame& src, const float luma_scaling, std::vector<float>& lut, IScriptEnvironment* env) noexcept;
