#include "AGM.h"
#include "VCL2/vectorclass.h"
#include "VCL2/vectormath_exp.h"

template <typename T, int peak>
AVS_FORCEINLINE float average_plane_avx512(const T* srcp, const int src_pitch, const int width, const int height) noexcept
{
    Vec16f accum{ zero_16f() };

    if constexpr (std::is_same_v<T, uint8_t>)
    {
        for (int y{ 0 }; y < height; ++y)
        {
            for (int x{ 0 }; x < width; x += 16)
                accum += to_float(Vec16i().load_16uc(srcp + x));

            srcp += src_pitch;
        }

        accum = (accum / (width * height)) / peak;
    }
    else if constexpr (std::is_same_v<T, uint16_t>)
    {
        for (int y{ 0 }; y < height; ++y)
        {
            for (int x{ 0 }; x < width; x += 16)
                accum += to_float(Vec16i().load_16us(srcp + x));

            srcp += src_pitch;
        }

        accum = (accum / (width * height)) / peak;
    }
    else
    {
        for (int y{ 0 }; y < height; ++y)
        {
            for (int x{ 0 }; x < width; x += 16)
                accum += Vec16f().load(srcp + x);

            srcp += src_pitch;
        }

        accum = accum / (width * height);
    }

    return horizontal_add(accum);
}

template <typename T, bool fade, int peak, int ymin, int y1, int y2, int ymax, int d0, int d1>
void process_avx512(PVideoFrame& dst, PVideoFrame& src, const float luma_scaling, std::vector<float>& lut, IScriptEnvironment* env) noexcept
{
    const int height{ src->GetHeight() };

    env->BitBlt(dst->GetWritePtr(), dst->GetPitch(), src->GetReadPtr(), src->GetPitch(), src->GetRowSize(), height);

    const size_t src_pitch{ src->GetPitch() / sizeof(T) };
    const size_t dst_pitch{ dst->GetPitch() / sizeof(T) };
    const size_t width{ src->GetRowSize() / sizeof(T) };
    const T* srcp{ reinterpret_cast<const T*>(src->GetReadPtr()) };
    T* __restrict dstp{ reinterpret_cast<T*>(dst->GetWritePtr()) };

    const float avg{ average_plane_avx512<T, peak>(srcp, src_pitch, width, height) };
    const Vec16f temp{ avg * avg * luma_scaling };

    for (int y{ 0 }; y < height; ++y)
    {
        for (size_t x{ 0 }; x < width; x += 16)
        {
            if constexpr (std::is_same_v<T, uint8_t>)
            {
                Vec16f srcp_d;
                srcp_d.insert(0, lut[srcp[x]]);
                srcp_d.insert(1, lut[srcp[x + 1]]);
                srcp_d.insert(2, lut[srcp[x + 2]]);
                srcp_d.insert(3, lut[srcp[x + 3]]);
                srcp_d.insert(4, lut[srcp[x + 4]]);
                srcp_d.insert(5, lut[srcp[x + 5]]);
                srcp_d.insert(6, lut[srcp[x + 6]]);
                srcp_d.insert(7, lut[srcp[x + 7]]);
                srcp_d.insert(8, lut[srcp[x + 8]]);
                srcp_d.insert(9, lut[srcp[x + 9]]);
                srcp_d.insert(10, lut[srcp[x + 10]]);
                srcp_d.insert(11, lut[srcp[x + 11]]);
                srcp_d.insert(12, lut[srcp[x + 12]]);
                srcp_d.insert(13, lut[srcp[x + 13]]);
                srcp_d.insert(14, lut[srcp[x + 14]]);
                srcp_d.insert(15, lut[srcp[x + 15]]);

                if constexpr (fade)
                {
                    const auto srcp_vi{ Vec16uc().load(srcp + x) };

                    select(!(srcp_vi > ymin), Vec16uc().load(srcp + x),
                        select(!(srcp_vi != y1), Vec16uc(d0),
                            select(!(srcp_vi != y2), Vec16uc(d1),
                                select(!(srcp_vi < ymax), zero_si128(),
                                    compress_saturated_s2u(compress_saturated(min(max(truncatei(pow(srcp_d, temp) * peak + 0.5f), zero_si512()), peak), zero_si512()), zero_si512()).get_low().get_low())))).store_nt(dstp + x);
                }
                else
                    compress_saturated_s2u(compress_saturated(min(max(truncatei(pow(srcp_d, temp) * peak + 0.5f), zero_si512()), peak), zero_si512()), zero_si512()).get_low().get_low().store_nt(dstp + x);
            }
            else if constexpr (std::is_same_v<T, uint16_t>)
            {
                Vec16f srcp_d;
                srcp_d.insert(0, lut[srcp[x]]);
                srcp_d.insert(1, lut[srcp[x + 1]]);
                srcp_d.insert(2, lut[srcp[x + 2]]);
                srcp_d.insert(3, lut[srcp[x + 3]]);
                srcp_d.insert(4, lut[srcp[x + 4]]);
                srcp_d.insert(5, lut[srcp[x + 5]]);
                srcp_d.insert(6, lut[srcp[x + 6]]);
                srcp_d.insert(7, lut[srcp[x + 7]]);
                srcp_d.insert(8, lut[srcp[x + 8]]);
                srcp_d.insert(9, lut[srcp[x + 9]]);
                srcp_d.insert(10, lut[srcp[x + 10]]);
                srcp_d.insert(11, lut[srcp[x + 11]]);
                srcp_d.insert(12, lut[srcp[x + 12]]);
                srcp_d.insert(13, lut[srcp[x + 13]]);
                srcp_d.insert(14, lut[srcp[x + 14]]);
                srcp_d.insert(15, lut[srcp[x + 15]]);

                if constexpr (fade)
                {
                    const auto srcp_vi{ Vec16us().load(srcp + x) };

                    select(!(srcp_vi > ymin), Vec16us().load(srcp + x),
                        select(!(srcp_vi != y1), Vec16us(d0),
                            select(!(srcp_vi != y2), Vec16us(d1),
                                select(!(srcp_vi < ymax), zero_si256(),
                                    compress_saturated_s2u(min(max(truncatei(pow(srcp_d, temp) * peak + 0.5f), zero_si512()), peak), zero_si512()).get_low())))).store_nt(dstp + x);
                }
                else
                    compress_saturated_s2u(min(max(truncatei(pow(srcp_d, temp) * peak + 0.5f), zero_si512()), peak), zero_si512()).get_low().store_nt(dstp + x);
            }
            else
            {
                const auto srcp_d{ Vec16f().load(srcp + x) };

                if constexpr (fade)
                    select(!(srcp_d != 0.0f), srcp_d,
                        select(!(srcp_d != 1.0f), Vec16f(0.0f),
                            min(max(pow(1.0f - (srcp_d * ((srcp_d * ((srcp_d * ((srcp_d * ((srcp_d * 18.188f) - 45.47f)) + 36.624f)) - 9.466f)) + 1.124f)),
                                temp), zero_16f()), 1.0f))).store_nt(dstp + x);
                else
                    min(max(pow(1.0f - (srcp_d * ((srcp_d * ((srcp_d * ((srcp_d * ((srcp_d * 18.188f) - 45.47f)) + 36.624f)) - 9.466f)) + 1.124f)),
                        temp), zero_16f()), 1.0f).store_nt(dstp + x);
            }
        }

        srcp += src_pitch;
        dstp += dst_pitch;
    }
}

template void process_avx512<uint8_t, true, 255, 16, 17, 18, 235, 85, 170>(PVideoFrame& dst, PVideoFrame& src, const float luma_scaling, std::vector<float>& lut, IScriptEnvironment* env) noexcept;
template void process_avx512<uint8_t, false, 255, 16, 17, 18, 235, 85, 170>(PVideoFrame& dst, PVideoFrame& src, const float luma_scaling, std::vector<float>& lut, IScriptEnvironment* env) noexcept;

template void process_avx512<uint16_t, true, 1023, 64, 68, 72, 940, 340, 680>(PVideoFrame& dst, PVideoFrame& src, const float luma_scaling, std::vector<float>& lut, IScriptEnvironment* env) noexcept;
template void process_avx512<uint16_t, false, 1023, 64, 68, 72, 940, 340, 680>(PVideoFrame& dst, PVideoFrame& src, const float luma_scaling, std::vector<float>& lut, IScriptEnvironment* env) noexcept;

template void process_avx512<uint16_t, true, 4095, 256, 272, 288, 3760, 1360, 2720>(PVideoFrame& dst, PVideoFrame& src, const float luma_scaling, std::vector<float>& lut, IScriptEnvironment* env) noexcept;
template void process_avx512<uint16_t, false, 4095, 256, 272, 288, 3760, 1360, 2720>(PVideoFrame& dst, PVideoFrame& src, const float luma_scaling, std::vector<float>& lut, IScriptEnvironment* env) noexcept;

template void process_avx512<uint16_t, true, 16383, 1024, 1088, 1152, 15040, 5440, 10880>(PVideoFrame& dst, PVideoFrame& src, const float luma_scaling, std::vector<float>& lut, IScriptEnvironment* env) noexcept;
template void process_avx512<uint16_t, false, 16383, 1024, 1088, 1152, 15040, 5440, 10880>(PVideoFrame& dst, PVideoFrame& src, const float luma_scaling, std::vector<float>& lut, IScriptEnvironment* env) noexcept;

template void process_avx512<uint16_t, true, 65535, 4096, 4352, 4608, 60160, 21760, 43520>(PVideoFrame& dst, PVideoFrame& src, const float luma_scaling, std::vector<float>& lut, IScriptEnvironment* env) noexcept;
template void process_avx512<uint16_t, false, 65535, 4096, 4352, 4608, 60160, 21760, 43520>(PVideoFrame& dst, PVideoFrame& src, const float luma_scaling, std::vector<float>& lut, IScriptEnvironment* env) noexcept;

template void process_avx512<float, true, 0, 0, 0, 0, 0, 0, 0>(PVideoFrame& dst, PVideoFrame& src, const float luma_scaling, std::vector<float>& lut, IScriptEnvironment* env) noexcept;
template void process_avx512<float, false, 0, 0, 0, 0, 0, 0, 0>(PVideoFrame& dst, PVideoFrame& src, const float luma_scaling, std::vector<float>& lut, IScriptEnvironment* env) noexcept;
