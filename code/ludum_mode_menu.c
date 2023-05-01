typedef struct ldModeMenu {
    ldContext *ld;
    xiArena *arena;

    b32 playing;
} ldModeMenu;

static void ludum_mode_menu_init(ldContext *ld) {
    xiContext *xi = ld->xi;

    xi_arena_reset(&ld->mode_arena);

    ldModeMenu *menu = xi_arena_push_type(&ld->mode_arena, ldModeMenu);
    if (menu) {
        menu->ld    = ld;
        menu->arena = &ld->mode_arena;

        xiAssetManager *assets = &xi->assets;
        xiAudioPlayer *audio   = &xi->audio_player;

        xiSoundHandle ambient = xi_sound_get_by_name(assets, "main_theme_1");
        xi_music_layer_add(audio, ambient, 0x333);

        xi_music_layer_enable_by_index(audio, 0, XI_MUSIC_LAYER_EFFECT_INSTANT, 0);

        ld->mode = LD_MODE_MENU;
        ld->menu = menu;
    }
}

static void ludum_mode_menu_simulate(ldModeMenu *menu) {
    ldContext *ld = menu->ld;
    xiContext *xi = ld->xi;

    xiAudioPlayer *audio = &xi->audio_player;

    xiInputKeyboard *kb = &xi->keyboard;
    if (!menu->playing && kb->active) {
        // any ke pressed
        //
        xiSoundHandle bing = xi_sound_get_by_name(&xi->assets, "finish");

        xi_sound_effect_play(audio, bing, 0x222, 0.8);
        xi_music_layer_disable_by_index(audio, 0, XI_MUSIC_LAYER_EFFECT_FADE, 4);

        menu->playing = true;
    }

    for (u32 it = 0; it < audio->event_count; ++it) {
        xiAudioEvent *e = &audio->events[it];
        if (e->type == XI_AUDIO_EVENT_TYPE_STOPPED && e->tag == 0x333) {
            ludum_mode_play_init(ld);
        }
    }
}

static void ludum_mode_menu_render(ldModeMenu *menu, xiRenderer *renderer) {
    ldContext *ld = menu->ld;
    xiContext *xi = ld->xi;

    xiImageHandle main = xi_image_get_by_name_str(&xi->assets, xi_str_wrap_const("main_menu"));

    xi_v3 x = xi_v3_create(2, 0, 0);
    xi_v3 y = xi_v3_create(0, 2, 0);
    xi_v3 z = xi_v3_create(0, 0, 2);

    xi_v3 p = xi_v3_create(0, 0, 2);

    xi_camera_transform_set_from_axes(renderer, x, y, z, p, XI_CAMERA_TRANSFORM_FLAG_ORTHOGRAPHIC);

    xi_sprite_draw_xy_scaled(renderer, main, xi_v2_create(0, 0), 1, 0);
}
