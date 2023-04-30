#define LD_DEFAULT_MIN_ZOOM 3.5f
#define LD_DEFAULT_MAX_ZOOM 4.5f
#define LD_TOTAL_MAX_ZOOM   28.5f

#define LD_AMBIENT_MUSIC_TAG 0x3322

#define LD_SHORT_RADIO_TAG   0x4821
#define LD_LONG_RADIO_TAG    0x4822

#define LD_STATIC_LONG_TIMER_THRESHOLD 5.5f

#define LD_NUM_RADIO_STATIC 7

enum ldMusicType {
    LD_MUSIC_TYPE_CHILL = 0,
    LD_MUSIC_TYPE_COUNTRY,
    LD_MUSIC_TYPE_FUNK,
    LD_MUSIC_TYPE_HIPHOP,
    LD_MUSIC_TYPE_ROCK,

    LD_MUSIC_TYPE_COUNT
};

static const u32   ld_music_type_counts[LD_MUSIC_TYPE_COUNT]   = { 1,       1,         1,      2,        1      };
static const char *ld_music_type_prefixes[LD_MUSIC_TYPE_COUNT] = { "chill", "country", "funk", "hiphop", "rock" };

typedef union ldMusicTag {
    struct {
        u16 type; // ldMusicLayer
        b8  radio;
        u8  index;
    };

    u32 value;
} ldMusicTag;

struct ldModePlay {
    ldContext *ld;

    xiArena *arena;

    xiRandomState rng;

    v2 min_camera;
    v2 max_camera;
    v2 camera_p;

    f32 max_zoom;
    f32 zoom;

    struct {
        b32 waiting; // for static to finish playing
        b32 playing;
        f32 timer; // since last switch
        u32 plays;

        ldMusicTag current;
    } music;

    xiSoundHandle static_long;
    xiSoundHandle static_short;

    v2 p;
    f32 angle;

    xiLogger log;
};

static void ludum_music_layers_configure(ldModePlay *play, xiContext *xi) {
    xiAssetManager *assets = &xi->assets;
    xiAudioPlayer  *audio  = &xi->audio_player;

    xiArena *temp = xi_temp_get();

    // the base layer is always the ambient sound
    //
    xiSoundHandle ambient = xi_sound_get_by_name(assets, "ambient_01");
    xi_music_layer_add(audio, ambient, LD_AMBIENT_MUSIC_TAG);

    for (u32 i = 0; i < LD_MUSIC_TYPE_COUNT; ++i) {
        ldMusicTag normal, radio;

        radio.type   = i;
        radio.radio  = true;

        normal.type  = i;
        normal.radio = false;

        for (u32 j = 0; j < ld_music_type_counts[i]; ++j) {
            radio.index  = j;
            normal.index = j;

            xi_string radio_name  = xi_str_format(temp, "%s_%d_radio", ld_music_type_prefixes[i], j + 1);
            xi_string normal_name = xi_str_format(temp, "%s_%d", ld_music_type_prefixes[i], j + 1);

            xiSoundHandle rh = xi_sound_get_by_name_str(assets, radio_name);
            xiSoundHandle nh = xi_sound_get_by_name_str(assets, normal_name);

            XI_ASSERT(rh.value != 0);
            XI_ASSERT(nh.value != 0);

            xi_music_layer_add(audio, rh, radio.value);
            xi_music_layer_add(audio, nh, normal.value);
        }
    }
}

static void ludum_music_layer_random_play(ldModePlay *play, xiAudioPlayer *audio) {
    // we don't want to try and play a new piece if the static sound is playing because it means we are in
    // between 'tuning'
    //
    if (!play->music.waiting) {
        // stop any currently playing music
        //
        b32 static_long = false;
        if (play->music.playing) {
            // otherwise check if music timer has exceeded threshold for long static
            //
            static_long = (play->music.timer > LD_STATIC_LONG_TIMER_THRESHOLD);

            u32 tag = play->music.current.value;
            xi_music_layer_disable_by_tag(audio, tag, XI_MUSIC_LAYER_EFFECT_INSTANT, 0);

            play->music.playing = false;
        }
        else {
            // if no music is playing we always use long static
            //
            static_long = true;
        }

        play->music.plays = 0;
        play->music.timer = 0;

        ldMusicTag tag;
        tag.type  = xi_rng_choice_u32(&play->rng, LD_MUSIC_TYPE_COUNT);
        tag.radio = true;
        tag.index = xi_rng_choice_u32(&play->rng, ld_music_type_counts[tag.type]);

        xi_log(&play->log, "music", "tag { %d, true, %d }", tag.type, tag.index);

        play->music.current = tag;
        play->music.waiting = true;

        if (static_long) {
            xi_sound_effect_play(audio, play->static_long, LD_LONG_RADIO_TAG, 0.85f);
            xi_music_layer_enable_by_tag(audio, tag.value, XI_MUSIC_LAYER_EFFECT_FADE, 0.125f);
        }
        else {
            xi_sound_effect_play(audio, play->static_short, LD_SHORT_RADIO_TAG, 0.9f);
        }
    }
}

static void ludum_mode_play_init(ldContext *ld) {
    xi_arena_reset(&ld->mode_arena);

    ldModePlay *play = xi_arena_push_type(&ld->mode_arena, ldModePlay);
    if (play) {
        play->arena = &ld->mode_arena;

        xiContext *xi = ld->xi;

        xiArena *temp = xi_temp_get();

        xi_logger_create(play->arena, &play->log, xi->system.out, 512);

        play->ld    = ld;
        play->angle = 0;

        play->zoom     = LD_DEFAULT_MIN_ZOOM;
        play->max_zoom = LD_TOTAL_MAX_ZOOM;
        play->camera_p = xi_v2_create(0, 0);

        v2u window_dim = xi->renderer.setup.window_dim;
        f32 aspect = 1280.0f / 720.0f; // :engine_work incomplete, renderer window_dim isn't available in init

        v3 x = xi_v3_create(1, 0, 0);
        v3 y = xi_v3_create(0, 1, 0);
        v3 z = xi_v3_create(0, 0, 1);

        v3 p = xi_v3_create(0, 0, LD_TOTAL_MAX_ZOOM * 1.25f); // only be able to see 80% of the full map

        xiCameraTransform full_map;
        ludum_camera_transform_get_from_axes(&full_map, aspect, x, y, z, p, 0);

        rect3 bounds = xi_camera_bounds_get(&full_map);

        play->min_camera = bounds.min.xy;
        play->max_camera = bounds.max.xy;

        play->static_long  = xi_sound_get_by_name(&xi->assets, "static_long");
        play->static_short = xi_sound_get_by_name(&xi->assets, "static_short");

        ludum_music_layers_configure(play, xi);

        ld->mode = LD_MODE_PLAY;
    }

    ld->play = play;
}

static void ludum_mode_play_simulate(ldModePlay *play) {
    ldContext *ld = play->ld;
    xiContext *xi = ld->xi;

    xiInputKeyboard *kb = &xi->keyboard;
    xiInputMouse *mouse = &xi->mouse;

    xiAudioPlayer *audio = &xi->audio_player;

    // :engine_work time ticks not available in init
    //
    if (play->rng.state == 0) { play->rng.state = xi->time.ticks; }

    play->music.timer += xi->time.delta.s;

    if (kb->keys['m'].pressed) {
        ludum_music_layer_random_play(play, audio);
    }

    // handle camera movement
    //
    play->zoom += mouse->delta.wheel.y;
    play->zoom  = XI_CLAMP(play->zoom, LD_DEFAULT_MIN_ZOOM, play->max_zoom);

    if (mouse->left.down) {
        v2 dist = xi_v2_mul_f32(xi_v2_neg(mouse->delta.clip), 0.88f * play->zoom);
        play->camera_p = xi_v2_add(play->camera_p, dist);
    }

    v2u window_dim = xi->renderer.setup.window_dim;
    f32 aspect = (f32) window_dim.w / (f32) window_dim.h;
    v3 x = xi_v3_create(1, 0, 0);
    v3 y = xi_v3_create(0, 1, 0);
    v3 z = xi_v3_create(0, 0, 1);

    v3 p = xi_v3_from_v2(play->camera_p, play->zoom);

    xiCameraTransform camera;
    ludum_camera_transform_get_from_axes(&camera, aspect, x, y, z, p, 0);

    // prevent the camera from leaving the map area
    //
    rect3 bounds = xi_camera_bounds_get(&camera);

    if (bounds.min.x < play->min_camera.x) { play->camera_p.x += (play->min_camera.x - bounds.min.x); }
    if (bounds.max.x > play->max_camera.x) { play->camera_p.x += (play->max_camera.x - bounds.max.x); }

    if (bounds.min.y < play->min_camera.y) { play->camera_p.y += (play->min_camera.y - bounds.min.y); }
    if (bounds.max.y > play->max_camera.y) { play->camera_p.y += (play->max_camera.y - bounds.max.y); }

    play->angle += xi->time.delta.s;

    // handle audio events to play music correctly
    //
    for (u32 it = 0; it < audio->event_count; ++it) {
        xiAudioEvent *e = &audio->events[it];
        switch (e->type) {
            case XI_AUDIO_EVENT_TYPE_STARTED: {} break;
            case XI_AUDIO_EVENT_TYPE_STOPPED: {
                if (e->from_music) {
                    if (e->tag == play->music.current.value) {
                        play->music.playing = false;
                    }
                }
                else {
                    play->music.playing = true;

                    if (e->tag == LD_SHORT_RADIO_TAG || e->tag == LD_LONG_RADIO_TAG) {
                        play->music.waiting = false;

                        if (e->tag == LD_SHORT_RADIO_TAG) {
                            u32 tag = play->music.current.value;
                            xi_music_layer_enable_by_tag(audio, tag, XI_MUSIC_LAYER_EFFECT_INSTANT, 0);
                        }
                    }
                }
            }
            break;
            case XI_AUDIO_EVENT_TYPE_LOOP_RESET: {
                xi_log(&play->log, "audio", "music looped!");

                if (e->tag == play->music.current.value) {
                    play->music.plays += 1;

                    ldMusicTag tag;
                    tag.value = e->tag;

                    if (tag.radio && (play->music.plays == 1)) {
                        // disable the currently plaing 'radio' variant
                        //
                        xi_music_layer_disable_by_tag(audio, tag.value, XI_MUSIC_LAYER_EFFECT_FADE, 1.2f);

                        // enable the new 'high quality' version
                        //
                        tag.radio = false;
                        xi_music_layer_enable_by_tag(audio, tag.value, XI_MUSIC_LAYER_EFFECT_FADE, 1.2f);

                        play->music.current.radio = false;
                    }

                    if (play->music.plays > 2) {
                        xi_music_layer_disable_by_tag(audio, e->tag, XI_MUSIC_LAYER_EFFECT_FADE, 0.5f);
                        play->music.playing = false;
                        play->music.plays   = 0;

                        xi_log(&play->log, "audio", "disabling music...");
                    }
                }
                else {
                    xi_log(&play->log, "audio", "we looped music that isn't current!");
                }
            }
            break;
        }
    }
}

static void ludum_mode_play_render(ldModePlay *play, xiRenderer *renderer) {
    // basic camera transform
    //
    v3 x = xi_v3_create(1, 0, 0);
    v3 y = xi_v3_create(0, 1, 0);
    v3 z = xi_v3_create(0, 0, 1);

    v3 p = xi_v3_from_v2(play->camera_p, play->zoom);

    xi_camera_transform_set_axes(renderer, x, y, z, p, 0);

    rect3 bounds = xi_camera_bounds_get(&renderer->camera);

    v4 bg     = xi_v4_create(0.18f, 0.18f, 0.18f, 1.0f);
    v2 center = xi_v2_mul_f32(xi_v2_add(bounds.max.xy, bounds.min.xy), 0.5f);
    v2 dim    = xi_v2_mul_f32(xi_v2_sub(bounds.max.xy, bounds.min.xy), 1.25f);

    xi_quad_draw_xy(renderer, bg, center, dim, 0);

    {
        xi_v2 map_center = xi_v2_mul_f32(xi_v2_add(play->min_camera, play->max_camera), 0.5f);
        xi_v2 map_bounds = xi_v2_sub(play->max_camera, play->min_camera);

        xi_quad_outline_draw_xy(renderer, xi_v4_create(1, 0, 0, 1), map_center, map_bounds, 0, 0.5f);
    }

    xi_quad_draw_xy(renderer, xi_v4_create(1, 1, 0, 1), play->p, xi_v2_create(1, 1), play->angle);

    xi_logger_flush(&play->log);
}
