#include "ludum.h"

#include "nav_mesh.c"

#include "ludum_util.c"
#include "ludum_mode_play.c"

extern XI_EXPORT XI_GAME_INIT(xiContext *xi, xi_u32 type) {
    XI_ASSERT(xi->version.major == XI_VERSION_MAJOR);
    XI_ASSERT(xi->version.minor == XI_VERSION_MINOR);

    switch (type) {
        case XI_ENGINE_CONFIGURE: {
            xiArena *temp = xi_temp_get();

            xi->window.width  = 1280;
            xi->window.height = 720;

            xi->time.delta.fixed_hz = 60;

            xi->system.console_open = true;

            // setup assets
            //
            xiAssetManager *assets = &xi->assets;

            xi_string exe_path = xi->system.executable_path;

            assets->importer.enabled       = true;
            assets->importer.search_dir    = xi_str_format(temp, "%.*s/../assets", xi_str_unpack(exe_path));
            assets->importer.sprite_prefix = xi_str_wrap_const("sprite_");

            assets->animation_dt = 1.0f / 30.0f;

            assets->sample_buffer.limit = XI_MB(128);

            // setup renderer
            //
            xiRenderer *renderer = &xi->renderer;

            renderer->transfer_queue.limit   = XI_MB(512);

            renderer->sprite_array.dimension = 256;
            renderer->sprite_array.limit     = 256;

            renderer->setup.vsync  = true;
            renderer->layer_offset = 0.01f;

            // setup audio player
            //
            xiAudioPlayer *audio = &xi->audio_player;

            audio->volume = 0.5f;

            audio->music.playing     = true; // no music for now :enable
            audio->music.volume      = 0.8f;
            audio->music.layer_limit = 16;

            audio->sfx.volume = 1.0f;
            audio->sfx.limit  = 32;
        }
        break;
        case XI_GAME_INIT: {
            ldContext *ld;
            {
                xiArena perm = { 0 };
                xi_arena_init_virtual(&perm, XI_MB(64));

                ld = xi_arena_push_type(&perm, ldContext);
                ld->perm_arena = perm;
            }

            ld->xi = xi;
            xi_arena_init_virtual(&ld->mode_arena, XI_GB(4));

            ludum_mode_play_init(ld);
            xi->user = ld;

            // TODO: @todo remove this debug path finding call
            //
            XI_ASSERT(NAV_MESH.nodeCount > 0);
            generateRoute(&NAV_MESH, 0, NAV_MESH.nodeCount - 1);
        }
        break;
        case XI_GAME_RELOADED: {
        }
        break;
    }
}

extern XI_EXPORT XI_GAME_SIMULATE(xiContext *xi) {
    ldContext *ld = xi->user;

    if (ld) {
        switch (ld->mode) {
            case LD_MODE_NONE: { /* do nothing...        */ } break;
            case LD_MODE_MENU: { /* @incomplete: no menu */ } break;
            case LD_MODE_PLAY: {
                ludum_mode_play_simulate(ld->play);
            }
            break;
        }
    }
}

extern XI_EXPORT XI_GAME_RENDER(xiContext *xi, xiRenderer *renderer) {
    ldContext *ld = xi->user;

    if (ld) {
        switch (ld->mode) {
            case LD_MODE_NONE: { /* do nothing...        */ } break;
            case LD_MODE_MENU: { /* @incomplete: no menu */ } break;
            case LD_MODE_PLAY: {
                ludum_mode_play_render(ld->play, renderer);
            }
            break;
        }
    }
}
