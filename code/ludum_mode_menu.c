typedef struct ldModeMenu {
    ldContext *ld;
    xiArena *arena;

    b32 game_over; // true if this is the game over screen, otherwise main menu

    xiImageHandle bg;
    xiImageHandle text[2];

    f32 text_dt[2];
    f32 text_s[2];

    v2 text_p;

    f32 dir;

    f32 alpha;

    b32 playing;
} ldModeMenu;

static void ludum_mode_menu_exit(ldModeMenu *menu, xiContext *xi) {
    xiSoundHandle bing = xi_sound_get_by_name(&xi->assets, "finish");

    xi_sound_effect_play(&xi->audio_player, bing, 0x222, 0.5);
    if (!menu->game_over) {
        xi_music_layer_disable_by_index(&xi->audio_player, 0, XI_MUSIC_LAYER_EFFECT_FADE, 4);
    }

    menu->playing = true;
}

static void ludum_mode_menu_init(ldContext *ld, b32 game_over, b32 first) {
    xiContext *xi = ld->xi;

    xi_arena_reset(&ld->mode_arena);

    ldModeMenu *menu = xi_arena_push_type(&ld->mode_arena, ldModeMenu);
    if (menu) {
        menu->ld    = ld;
        menu->arena = &ld->mode_arena;

        menu->game_over = game_over;

        xiAssetManager *assets = &xi->assets;
        xiAudioPlayer *audio   = &xi->audio_player;

        if (game_over) {
            xi_music_layers_clear(audio);
        }

        if (game_over || first) {
            xiSoundHandle ambient = xi_sound_get_by_name(assets, "main_theme_1");
            xi_music_layer_add(audio, ambient, 0x333);

            xi_music_layer_enable_by_index(audio, 0, XI_MUSIC_LAYER_EFFECT_INSTANT, 0);
        }

        menu->dir   = -1;
        menu->alpha =  1;

        if (game_over) {
            menu->bg      = xi_image_get_by_name(assets, "game_over");
            menu->text[0] = xi_image_get_by_name(assets, "start_again");

            menu->text_p = xi_v2_create(0.2, -0.18f);

            menu->text_s[0] = 0.4f;
        }
        else {
            menu->bg   = xi_image_get_by_name(assets, "main_menu");

            menu->text[0] = xi_image_get_by_name(assets, "start_game");
            menu->text[1] = xi_image_get_by_name(assets, "exit_game");

            menu->text_p = xi_v2_create(0.2, -0.08f);

            menu->text_s[0] = 0.28f;
            menu->text_s[1] = 0.10f;
        }

        ld->mode = LD_MODE_MENU;
        ld->menu = menu;
    }
}

static void ludum_mode_menu_simulate(ldModeMenu *menu) {
    ldContext *ld = menu->ld;
    xiContext *xi = ld->xi;

    xiAudioPlayer *audio = &xi->audio_player;
    f32 dt = xi->time.delta.s;

    xiInputKeyboard *kb = &xi->keyboard;
    xiInputMouse    *mouse = &xi->mouse;
    if (!menu->playing && kb->active) {
        // any key pressed
        //
        ludum_mode_menu_exit(menu, xi);
    }

    if (menu->text[0].value != 0) {
        xiaImageInfo *info = xi_image_info_get(&xi->assets, menu->text[0]);

        xi_v3 x = xi_v3_create(2, 0, 0);
        xi_v3 y = xi_v3_create(0, 2, 0);
        xi_v3 z = xi_v3_create(0, 0, 2);

        xi_v3 p = xi_v3_create(0, 0, 2);

        f32 aspect = (f32) xi->renderer.setup.window_dim.w / (f32) xi->renderer.setup.window_dim.h;
        xiCameraTransform camera;
        xi_camera_transform_get_from_axes(&camera, aspect, x, y, z, p, XI_CAMERA_TRANSFORM_FLAG_ORTHOGRAPHIC);

        v2 dim;
        dim.x = menu->text_s[0];
        dim.y = menu->text_s[0] * (info->height / (f32) info->width);

        rect2 bounds;
        bounds.min.x = menu->text_p.x - 0.5f * dim.x;
        bounds.min.y = menu->text_p.y - 0.5f * dim.y;

        bounds.max.x = menu->text_p.x + 0.5f * dim.x;
        bounds.max.y = menu->text_p.y + 0.5f * dim.y;

        v2 world_p = xi_unproject_xy(&camera, xi->mouse.position.clip).xy;

        if (pointInRect(world_p, bounds)) {
            menu->text_dt[0] -= dt;

            if (mouse->left.pressed) {
                ludum_mode_menu_exit(menu, xi);
            }
        }
        else {
            menu->text_dt[0] += dt;
        }

        if (!menu->game_over) {
            xiaImageInfo *info = xi_image_info_get(&xi->assets, menu->text[1]);

            v2 p = menu->text_p;
            p.y -= (dim.h + 0.02);

            dim.x = menu->text_s[1];
            dim.y = menu->text_s[1] * (info->height / (f32) info->width);

            bounds.min.x = p.x - 0.5f * dim.x;
            bounds.min.y = p.y - 0.5f * dim.y;

            bounds.max.x = p.x + 0.5f * dim.x;
            bounds.max.y = p.y + 0.5f * dim.y;

            if (pointInRect(world_p, bounds)) {
                menu->text_dt[1] -= dt;

                if (mouse->left.pressed) {
                    xi->quit = true;
                }
            }
            else {
                menu->text_dt[1] += dt;
            }
        }

        menu->text_dt[0] = XI_CLAMP(menu->text_dt[0], 0, 1);
        menu->text_dt[1] = XI_CLAMP(menu->text_dt[1], 0, 1);
    }

    menu->alpha += menu->dir * dt;
    if (menu->alpha <= 0) {
        menu->alpha = 0;
        menu->dir   = 1;
    }
    else if (menu->alpha >= 1) {
        menu->alpha =  1;
        menu->dir   = -1;
    }

    for (u32 it = 0; it < audio->event_count; ++it) {
        xiAudioEvent *e = &audio->events[it];
        if (e->type == XI_AUDIO_EVENT_TYPE_STOPPED && e->tag == 0x222) {
            if (menu->game_over) {
                ludum_mode_menu_init(ld, false, false);
            }
            else {
                ludum_mode_play_init(ld);
            }
        }
    }
}

static void ludum_mode_menu_render(ldModeMenu *menu, xiRenderer *renderer) {
    ldContext *ld = menu->ld;
    xiContext *xi = ld->xi;

    xi_v3 x = xi_v3_create(2, 0, 0);
    xi_v3 y = xi_v3_create(0, 2, 0);
    xi_v3 z = xi_v3_create(0, 0, 2);

    xi_v3 p = xi_v3_create(0, 0, 2);

    xi_camera_transform_set_from_axes(renderer, x, y, z, p, XI_CAMERA_TRANSFORM_FLAG_ORTHOGRAPHIC);

    xi_sprite_draw_xy_scaled(renderer, menu->bg, xi_v2_create(0, 0), 1, 0);

    if (menu->game_over) {
        xiaImageInfo *info = xi_image_info_get(&xi->assets, menu->text[0]);

        v2 dim;
        dim.x = 0.4f;
        dim.y = 0.4f * (info->height / (f32) info->width);

        rect2 bounds;
        bounds.min.x = menu->text_p.x - 0.5f * dim.x;
        bounds.min.y = menu->text_p.y - 0.5f * dim.y;

        bounds.max.x = menu->text_p.x + 0.5f * dim.x;
        bounds.max.y = menu->text_p.y + 0.5f * dim.y;

        v4 alpha = xi_v4_create(1, 1, 1, menu->alpha);
        xi_coloured_sprite_draw_xy_scaled(renderer, menu->text[0], alpha, menu->text_p, 0.4f, 0);

        alpha.a = 1;

        f32 len = (bounds.max.x - bounds.min.x);

        v2 sp;
        sp.x = bounds.min.x + (menu->text_dt[0] * len);
        sp.y = bounds.min.y;

        v2 ep;
        ep.x = bounds.max.x;
        ep.y = bounds.min.y;

        xi_line_draw_xy(renderer, alpha, sp, alpha, ep, 0.005f);

        sp.x = bounds.max.x - (menu->text_dt[0] * len);
        sp.y = bounds.max.y;

        ep.x = bounds.min.x;
        ep.y = bounds.max.y;

        xi_line_draw_xy(renderer, alpha, sp, alpha, ep, 0.005f);
    }
    else {
        xiaImageInfo *info = xi_image_info_get(&xi->assets, menu->text[0]);

        v2 p = menu->text_p;

        v2 dim;
        dim.x = menu->text_s[0];
        dim.y = menu->text_s[0] * (info->height / (f32) info->width);

        rect2 bounds;
        bounds.min.x = p.x - 0.5f * dim.x;
        bounds.min.y = p.y - 0.5f * dim.y;

        bounds.max.x = p.x + 0.5f * dim.x;
        bounds.max.y = p.y + 0.5f * dim.y;

        v2 c = xi_v2_mul_f32(xi_v2_add(bounds.max, bounds.min), 0.5f);
        v2 d = xi_v2_sub(bounds.max, bounds.min);

        xi_sprite_draw_xy_scaled(renderer, menu->text[0], p, menu->text_s[0], 0);

        f32 len = (bounds.max.x - bounds.min.x);

        v2 sp;
        sp.x = bounds.min.x + (menu->text_dt[0] * len);
        sp.y = bounds.min.y;

        v2 ep;
        ep.x = bounds.max.x;
        ep.y = bounds.min.y;

        v4 colour = xi_v4_create(0, 0.65, 0.65, 1);

        xi_line_draw_xy(renderer, colour, sp, colour, ep, 0.002f);

        sp.x = bounds.max.x - (menu->text_dt[0] * len);
        sp.y = bounds.max.y;

        ep.x = bounds.min.x;
        ep.y = bounds.max.y;

        xi_line_draw_xy(renderer, colour, sp, colour, ep, 0.002f);

        p.y -= (d.h + 0.02);

        info = xi_image_info_get(&xi->assets, menu->text[1]);

        dim.x = menu->text_s[1];
        dim.y = menu->text_s[1] * (info->height / (f32) info->width);

        bounds.min.x = p.x - 0.5f * dim.x;
        bounds.min.y = p.y - 0.5f * dim.y;

        bounds.max.x = p.x + 0.5f * dim.x;
        bounds.max.y = p.y + 0.5f * dim.y;

        len = (bounds.max.x - bounds.min.x);

        c = xi_v2_mul_f32(xi_v2_add(bounds.max, bounds.min), 0.5f);
        d = xi_v2_mul_f32(xi_v2_sub(bounds.max, bounds.min), 1.15);

        xi_sprite_draw_xy_scaled(renderer, menu->text[1], p, menu->text_s[1], 0);

        sp.x = bounds.min.x + (menu->text_dt[1] * len);
        sp.y = bounds.min.y;

        ep.x = bounds.max.x;
        ep.y = bounds.min.y;

        xi_line_draw_xy(renderer, colour, sp, colour, ep, 0.002f);

        sp.x = bounds.max.x - (menu->text_dt[1] * len);
        sp.y = bounds.max.y;

        ep.x = bounds.min.x;
        ep.y = bounds.max.y;

        xi_line_draw_xy(renderer, colour, sp, colour, ep, 0.002f);
    }
}
