#include <algorithm>

#include "avisynth.h"

template <typename PixelType>
static void asharp_run_c(const uint8_t* srcp8, uint8_t* dstp8, int pitch, int dst_pitch, int height, int width, int T, int D, int B, int B2, bool bf, int bits)
{
    const PixelType* srcp = reinterpret_cast<const PixelType*>(srcp8);
    PixelType* dstp = reinterpret_cast<PixelType*>(dstp8);   

    memcpy(dstp, srcp, width);

    pitch /= sizeof(PixelType);
    dst_pitch /= sizeof(PixelType);
    width /= sizeof(PixelType);
    int pixel_max = (1 << bits) - 1;

    srcp += pitch;
    dstp += dst_pitch;

    for (int y = 1; y < height - 1; ++y)
    {
        dstp[0] = srcp[0];

        for (int x = 1; x < width - 1; ++x)
        {
            int dev = 0;

            float avg = srcp[x - pitch - 1];
            avg += srcp[x - pitch];
            avg += srcp[x - pitch + 1];
            avg += srcp[x - 1];
            avg += srcp[x];
            avg += srcp[x + 1];
            avg += srcp[x + pitch - 1];
            avg += srcp[x + pitch];
            avg += srcp[x + pitch + 1];

            avg /= 9;

#define CHECK(A) if (abs(A - srcp[x]) > dev) dev = abs(A - srcp[x]);

            if (bf)
            {
                if (y % 8 > 0)
                {
                    if (x % 8 > 0)
                        CHECK(srcp[x - pitch - 1])
                        CHECK(srcp[x - pitch])
                        if (x % 8 < 7)
                            CHECK(srcp[x - pitch + 1])
                }
                if (x % 8 > 0)
                    CHECK(srcp[x - 1])
                    if (x % 8 < 7)
                        CHECK(srcp[x + 1])
                        if (y % 8 < 7)
                        {
                            if (x % 8 > 0)
                                CHECK(srcp[x + pitch - 1])
                                CHECK(srcp[x + pitch])
                                if (x % 8 < 7)
                                    CHECK(srcp[x + pitch + 1])
                        }

            }
            else
            {
                CHECK(srcp[x - pitch - 1])
                CHECK(srcp[x - pitch])
                CHECK(srcp[x - pitch + 1])
                CHECK(srcp[x - 1])
                CHECK(srcp[x + 1])
                CHECK(srcp[x + pitch - 1])
                CHECK(srcp[x + pitch])
                CHECK(srcp[x + pitch + 1])
            }
#undef CHECK

            int T2 = T;
            int64_t diff = srcp[x] - llrintf(avg);
            int64_t D2 = D;

            if (B2 != 256 && B != 256)
            {
                if (x % 8 == 6)
                    D2 = (D2 * B2) >> 8;
                if (x % 8 == 7)
                    D2 = (D2 * B) >> 8;
                if (x % 8 == 0)
                    D2 = (D2 * B) >> 8;
                if (x % 8 == 1)
                    D2 = (D2 * B2) >> 8;

                if (y % 8 == 6)
                    D2 = (D2 * B2) >> 8;
                if (y % 8 == 7)
                    D2 = (D2 * B) >> 8;
                if (y % 8 == 0)
                    D2 = (D2 * B) >> 8;
                if (y % 8 == 1)
                    D2 = (D2 * B2) >> 8;
            }

            int Da = -32 + (D >> 7);
            if (D > 0)
                T2 = ((((static_cast<int64_t>(dev) << 7) * D2) >> (16 + (bits - 8))) + Da) * 16;

            if (T2 > T)
                T2 = T;
            if (T2 < -32)
                T2 = -32;

            int tmp = (((diff * 128) * T2) >> 16) + srcp[x];

            if (tmp < 0)
                tmp = 0;
            if (tmp > pixel_max)
                tmp = pixel_max;

            dstp[x] = tmp;
        }

        dstp[width - 1] = srcp[width - 1];

        srcp += pitch;
        dstp += dst_pitch;
    }

    memcpy(dstp, srcp, width * sizeof(PixelType));
}

class ASharp : public GenericVideoFilter
{
    int t_, d_, b_, b2;
    bool hqbf_;
    bool has_at_least_v8;
    int bits;

    void (*asharp)(const uint8_t*, uint8_t*, int, int, int, int, int, int, int, int, bool, int);

public:
    ASharp(PClip _child, double T, double D, double B, bool hqbf, IScriptEnvironment* env)
        : GenericVideoFilter(_child), hqbf_(hqbf)
    {
        if (vi.IsRGB() || vi.BitsPerComponent() == 32 || !vi.IsPlanar())
            env->ThrowError("ASharp: the input clip must be in YUV 8..16-bit planar format.");
        if (T < 0.0 || T > 32.0)
            env->ThrowError("ASharp: T must be between 0.0..32.0.");
        if (D < 0.0 || D > 16.0)
            env->ThrowError("ASharp: D must be between 0.0..16.0.");
        if (B > 4)
            env->ThrowError("ASharp: B must be less than 4.0.");

        t_ = static_cast<int>(T * (static_cast<int64_t>(4) << 7));
        d_ = static_cast<int>(D * (static_cast<int64_t>(4) << 7));
        b_ = static_cast<int>(256 - B * 64);
        b2 = static_cast<int>(256 - B * 48);


        t_ = std::max(-(4 << 7), std::min(t_, 32 * (4 << 7)));
        d_ = std::max(0, std::min(d_, 16 * (4 << 7)));
        b_ = std::max(0, std::min(b_, 256));
        b2 = std::max(0, std::min(b2, 256));

        has_at_least_v8 = true;
        try { env->CheckVersion(8); }
        catch (const AvisynthError&) { has_at_least_v8 = false; }
        
        bits = vi.BitsPerComponent();

        asharp = bits == 8 ? asharp_run_c<uint8_t> : asharp_run_c<uint16_t>;
    }

    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env)
    {
        PVideoFrame src = child->GetFrame(n, env);
        PVideoFrame dst = has_at_least_v8 ? env->NewVideoFrameP(vi, &src) : env->NewVideoFrame(vi);

        int planes_y[4] = { PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A };
        const int planecount = std::min(vi.NumComponents(), 3);
        for (int i = 0; i < planecount; ++i)
        {
            int plane = planes_y[i];

            int pitch = src->GetPitch(plane);
            int dst_pitch = dst->GetPitch(plane);
            int height = src->GetHeight(plane);
            int width = src->GetRowSize(plane);
            const uint8_t* srcp = src->GetReadPtr(plane);
            uint8_t* dstp = dst->GetWritePtr(plane);

            if (i == 0)
                asharp(srcp, dstp, pitch, dst_pitch, height, width, t_, d_, b_, b2, hqbf_, bits);
            else
                env->BitBlt(dstp, dst_pitch, srcp, pitch, width, height);
        }

        return dst;
    }

    int __stdcall SetCacheHints(int cachehints, int frame_range)
    {
        return cachehints == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0;
    }
};

AVSValue __cdecl Create_ASharp(AVSValue args, void* user_data, IScriptEnvironment* env)
{
    return new ASharp(
        args[0].AsClip(),
        args[1].AsFloat(2.f),
        args[2].AsFloat(4.f),
        args[3].AsFloat(-1.f),
        args[4].AsBool(false),
        env);
}

const AVS_Linkage* AVS_linkage;

extern "C" __declspec(dllexport)
const char* __stdcall AvisynthPluginInit3(IScriptEnvironment * env, const AVS_Linkage* const vectors)
{
    AVS_linkage = vectors;

    env->AddFunction("ASharp", "c[T]f[D]f[B]f[hqbf]b", Create_ASharp, 0);

    return "ASharp";
}
