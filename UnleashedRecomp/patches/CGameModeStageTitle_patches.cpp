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

    bool isAdvertiseMovie = pGameModeStageTitle->m_IsPlayingAdvertiseMovie;
    if (isAdvertiseMovie && !s_lastWasAdvertiseMovie)
        Video::QueueTrimRuntimeCaches();

    s_lastWasAdvertiseMovie = isAdvertiseMovie;

    if (g_quitMessageOpen)
        pGameModeStageTitle->m_AdvertiseMovieWaitTime = 0;
}
