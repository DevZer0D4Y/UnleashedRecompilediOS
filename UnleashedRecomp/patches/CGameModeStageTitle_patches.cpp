#include <api/SWA.h>
#include <gpu/video.h>
#include <patches/CTitleStateIntro_patches.h>

// SWA::CGameModeStageTitle::Update
PPC_FUNC_IMPL(__imp__sub_825518B8);
PPC_FUNC(sub_825518B8)
{
    static bool s_lastWasAdvertiseMovie = false;

    auto pGameModeStageTitle = (SWA::CGameModeStageTitle*)g_memory.Translate(ctx.r3.u32);

    __imp__sub_825518B8(ctx, base);

    // Free the pipelines of the previous gameplay session to reduce memory usage on iOS.
    bool isAdvertiseMovie = pGameModeStageTitle->m_IsPlayingAdvertiseMovie;
#ifdef UNLEASHED_RECOMP_IOS
    if (isAdvertiseMovie && !s_lastWasAdvertiseMovie)
        Video::QueueTrimRuntimeCaches();
#endif

    s_lastWasAdvertiseMovie = isAdvertiseMovie;

    if (g_quitMessageOpen)
        pGameModeStageTitle->m_AdvertiseMovieWaitTime = 0;
}
