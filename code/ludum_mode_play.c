//#define LD_DEFAULT_MIN_ZOOM 12.0f
#define LD_DEFAULT_MIN_ZOOM 0.70f
#define LD_DEFAULT_MAX_ZOOM 4.5f // @incomplete: this will break when we limit zoom
#define LD_TOTAL_MAX_ZOOM   4.0f
#define LD_DEBUG_MAX_ZOOM   12.0f

#define LD_AMBIENT_MUSIC_TAG 0x3322

#define LD_SHORT_RADIO_TAG   0x4821
#define LD_LONG_RADIO_TAG    0x4822

#define LD_CLOUD_MIN_MOVE_SPEED 0.05
#define LD_CLOUD_MAX_MOVE_SPEED 0.18

#define LD_CLOUD_MIN_SPAWN_TIME 4.8f
#define LD_CLOUD_MAX_SPAWN_TIME 10.2f
#define LD_CLOUD_SCALE 2.8f

#define LD_CLOUD_VARIATION_COUNT 6
#define LD_MAX_CLOUDS 4

#define LD_STATIC_LONG_TIMER_THRESHOLD 5.5f

#define LD_NUM_RADIO_STATIC 7

typedef enum ldCarDir {
    LD_CAR_DIR_FRONT = 0,
    LD_CAR_DIR_SIDE,
    LD_CAR_DIR_BACK,

    LD_CAR_DIR_COUNT
} ldCarDir;

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

typedef struct ldCar {
	v2 position;
	nav_route route;
	u32 currentRouteStopTarget;
	f32 speed;
	f32 nodeProgress;
	f32 onBreak;
    xiImageHandle images[3];
    ldCarDir direction;
} ldCar;

typedef union ldMusicTag {
    struct {
        u16 type; // ldMusicType
        b8  radio;
        u8  index;
    };

    u32 value;
} ldMusicTag;

typedef struct ldCloudState {
    b32 enabled;
    u32 index;

    v2 p;
    f32 speed;
    f32 scale;
    f32 darkness;

    u32 next;
} ldCloudState;

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

    u32 node_count;
    nav_mesh_node *nodes;
	ldCar cars[500];
	u32 carCount;

    struct {
        xiImageHandle  images[LD_CLOUD_VARIATION_COUNT];
        xiImageHandle shadows[LD_CLOUD_VARIATION_COUNT];

        f32 timer;

        f32 min_y;
        f32 step_y; // y = min_y + (index * step_y)

        v2 max_dim;

        u32 free;
        u32 count;
        ldCloudState states[LD_MAX_CLOUDS];
    } clouds;

    xiSoundHandle static_long;
    xiSoundHandle static_short;

    xiImageHandle map;

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

    xi_music_layer_enable_by_index(audio, 0, XI_MUSIC_LAYER_EFFECT_INSTANT, 0);

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

static void ludum_clouds_init(ldModePlay *play, xiAssetManager *assets) {
    xiArena *temp = xi_temp_get();

    play->clouds.max_dim.x = 0;
    play->clouds.max_dim.y = 0;

    for (u32 it = 0; it < LD_CLOUD_VARIATION_COUNT; ++it) {
        xi_string image_name  = xi_str_format(temp, "cloud_%d", it);
        xi_string shadow_name = xi_str_format(temp, "cloud_%d_shadow", it);

        play->clouds.images[it]  = xi_image_get_by_name_str(assets, image_name);
        play->clouds.shadows[it] = xi_image_get_by_name_str(assets, shadow_name);

        XI_ASSERT(play->clouds.images[it].value  != 0);
        XI_ASSERT(play->clouds.shadows[it].value != 0);

        xiaImageInfo *info = xi_image_info_get(assets, play->clouds.images[it]);
        XI_ASSERT(info != 0);

        // :engine_work we probably want a call to get the size of a sprite at a specific scale or something
        //
        f32 width, height;

        if (info->width > info->height) {
            width  = LD_CLOUD_SCALE;
            height = LD_CLOUD_SCALE * (info->height / (f32) info->width);
        }
        else {
            width  = LD_CLOUD_SCALE * (info->width / (f32) info->height);
            height = LD_CLOUD_SCALE;
        }

        if (width  > play->clouds.max_dim.w) { play->clouds.max_dim.w = width;  }
        if (height > play->clouds.max_dim.h) { play->clouds.max_dim.h = height; }
    }

    play->clouds.max_dim = xi_v2_mul_f32(play->clouds.max_dim, 0.5f);

    f32 min_y = 0.55f * play->min_camera.y;
    f32 max_y = 0.68f * play->max_camera.y;

    play->clouds.min_y  = min_y;
    play->clouds.step_y = (max_y - min_y) / LD_MAX_CLOUDS;

    for (u32 it = 0; it < LD_MAX_CLOUDS; ++it) {
        play->clouds.states[it].next = it + 1;
    }

    play->clouds.timer = xi_rng_range_f32(&play->rng, LD_CLOUD_MIN_SPAWN_TIME, LD_CLOUD_MAX_SPAWN_TIME);
    play->clouds.free = 0;
}

static void ludum_cloud_add(ldModePlay *play) {
    xiRandomState *rng = &play->rng;

    if (play->clouds.count < LD_MAX_CLOUDS) {

        u32 index = play->clouds.free;

        XI_ASSERT(index < LD_MAX_CLOUDS);

        ldCloudState *cloud = &play->clouds.states[index];
        play->clouds.free   = cloud->next;

        b32 left = (index & 1);

        f32 h = play->clouds.max_dim.h;

        cloud->p.y = play->clouds.min_y + (index * play->clouds.step_y); // + xi_rng_range_f32(rng, -h, h);
        cloud->p.x = left ? (play->min_camera.x - play->clouds.max_dim.w) :
                            (play->max_camera.x + play->clouds.max_dim.w);

        f32 dir = (left ? 1 : -1);

        cloud->scale   = (xi_rng_choice_u32(rng, 2) ? -1 : 1) * xi_rng_range_f32(rng, 0.88f, 1.11f);
        cloud->speed   = dir * xi_rng_range_f32(rng, LD_CLOUD_MIN_MOVE_SPEED, LD_CLOUD_MAX_MOVE_SPEED);
        cloud->index   = 3; // xi_rng_choice_u32(&play->rng, LD_CLOUD_VARIATION_COUNT);
        cloud->enabled = true;

        play->clouds.count += 1;
    }

    play->clouds.timer = xi_rng_range_f32(rng, LD_CLOUD_MIN_SPAWN_TIME, LD_CLOUD_MAX_SPAWN_TIME);
}

static void ludum_car_init(ldModePlay *play, xiAssetManager *assets) {
	for(u32 i = 0; i < play->carCount; i++) {
		ldCar *car = &play->cars[i];
		u32 start = xi_rng_choice_u32(&play->rng, play->node_count);
		u32 end = xi_rng_choice_u32(&play->rng, play->node_count);
		car->route = generateRoute(play->arena, &NAV_MESH, start, end);
		car->speed = 0.1f;
		car->currentRouteStopTarget = 1;
		car->nodeProgress = 0;
        nav_mesh_node *node = &play->nodes[car->route.stops[car->currentRouteStopTarget-1]];
		car->position = xi_v2_create(node->x, node->y);
        car->images[LD_CAR_DIR_FRONT] = xi_image_get_by_name_str(assets, xi_str_wrap_cstr("car_front"));
        car->images[LD_CAR_DIR_SIDE] = xi_image_get_by_name_str(assets, xi_str_wrap_cstr("car_side"));
        car->images[LD_CAR_DIR_BACK] = xi_image_get_by_name_str(assets, xi_str_wrap_cstr("car_back"));
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

        v2u window_dim   = xi->renderer.setup.window_dim;
        f32 aspect_ratio = (f32) window_dim.w / (f32) window_dim.h;

        v3 x = xi_v3_create(1, 0, 0);
        v3 y = xi_v3_create(0, 1, 0);
        v3 z = xi_v3_create(0, 0, 1);

        v3 p = xi_v3_create(0, 0, LD_TOTAL_MAX_ZOOM * 1.25f);

        play->rng.state = xi->time.ticks;

        xiCameraTransform full_map;
        xi_camera_transform_get_from_axes(&full_map, aspect_ratio, x, y, z, p, 0);

        rect3 bounds = xi_camera_bounds_get(&full_map);

        play->min_camera = bounds.min.xy;
        play->max_camera = bounds.max.xy;

        ludum_clouds_init(play, &xi->assets);

        // setup nav mesh nodes for test drawing
        //
        nav_mesh *mesh = &NAV_MESH;

        play->node_count = mesh->nodeCount;
        play->nodes      = xi_arena_push_array(play->arena, nav_mesh_node, play->node_count);

        for (u32 it = 0; it < play->node_count; ++it) {
            nav_mesh_node *src = &mesh->nodes[it];
            nav_mesh_node *dst = &play->nodes[it];

            v2 p  = xi_v2_create(-1 + (src->x / 1920.0f), 1 - (src->y / 1080.0f));
            v2 up = xi_unproject_xy(&full_map, p).xy;

            dst->x = up.x;
            dst->y = up.y;

            dst->connectionCount = src->connectionCount;
            for (u32 x = 0; x < dst->connectionCount; ++x) {
                dst->connections[x] = src->connections[x];
            }
        }

#if 0
        play->cloud_p.x = play->min_camera.x;
        play->cloud_p.y = 0.5f * (play->max_camera.y + play->min_camera.y);

        play->cloud = xi_image_get_by_name(&xi->assets, "cloud_3");
        play->cloud_shadow = xi_image_get_by_name(&xi->assets, "cloud_3_shadow");
#endif

        play->static_long  = xi_sound_get_by_name(&xi->assets, "static_long");
        play->static_short = xi_sound_get_by_name(&xi->assets, "static_short");

        ludum_music_layers_configure(play, xi);

        play->map = xi_image_get_by_name(&xi->assets, "map_full");
		play->carCount = 499;
		ludum_car_init(play, &xi->assets);

        ld->mode = LD_MODE_PLAY;
    }

    ld->play = play;
}

static void ludum_car_update(ldModePlay *play, f32 delta) {
	for(int i = 0; i < play->carCount; i++) {
		ldCar *car = &play->cars[i];
		if(car->onBreak > 0) {
			car->onBreak -= delta;
			continue;
		}
		car->nodeProgress += delta;
		u32 startStop = car->route.stops[car->currentRouteStopTarget-1];
		u32 endStop = car->route.stops[car->currentRouteStopTarget];
		nav_mesh_node *currentNode = &play->nodes[startStop];
		nav_mesh_node *targetNode = &play->nodes[endStop];
		v2 startV2 = xi_v2_create(currentNode->x, currentNode->y);
		v2 endV2 = xi_v2_create(targetNode->x, targetNode->y);
		v2 direction = xi_v2_sub(startV2, endV2);
		car->direction = LD_CAR_DIR_BACK;
		if(ABS(direction.x) > ABS(direction.y)){
			car->direction = LD_CAR_DIR_SIDE;
		} else if(direction.y > 0) {
			car->direction = LD_CAR_DIR_FRONT;
		}
		f32 distance = xi_v2_length(xi_v2_sub(startV2, endV2));
		f32 speedMultiplier = 1;
		if(targetNode->motorway) {
			speedMultiplier = 2;
		}
		f32 travelledDistance = car->speed * speedMultiplier * car->nodeProgress;
		f32 progress = travelledDistance/distance;
		if(progress >= 1){
			car->nodeProgress = 0;
			car->currentRouteStopTarget += 1;
			if(car->currentRouteStopTarget == car->route.stop_count){
				u32 end = xi_rng_choice_u32(&play->rng, play->node_count);
				car->route = generateRoute(play->arena, &NAV_MESH, end, endStop);
				car->currentRouteStopTarget = 1;
                xi_log(&play->log, "routing", "new route between %d and %d", endStop, end);
				car->onBreak = 1.5f;
			}
			startStop = car->route.stops[car->currentRouteStopTarget-1];
			endStop = car->route.stops[car->currentRouteStopTarget];
			currentNode = &play->nodes[startStop];
			targetNode = &play->nodes[endStop];
			progress = 0;
			startV2 = xi_v2_create(currentNode->x, currentNode->y);
			endV2 = xi_v2_create(targetNode->x, targetNode->y);
		}
		car->position = xi_lerp_v2(startV2, endV2, progress);
	}
}

static void ludum_mode_play_simulate(ldModePlay *play) {
    ldContext *ld = play->ld;
    xiContext *xi = ld->xi;

    xiInputKeyboard *kb = &xi->keyboard;
    xiInputMouse *mouse = &xi->mouse;

    xiAudioPlayer *audio = &xi->audio_player;

    play->music.timer += xi->time.delta.s;

    if (kb->keys['m'].pressed) {
        ludum_music_layer_random_play(play, audio);
    }

    if (kb->alt) {
        play->max_zoom = LD_DEBUG_MAX_ZOOM;
    }
    else {
        play->max_zoom = LD_TOTAL_MAX_ZOOM;
    }

    // handle camera movement
    //
    play->zoom += (0.85f * mouse->delta.wheel.y);
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
    xi_camera_transform_get_from_axes(&camera, aspect, x, y, z, p, 0);

    f32 dt = (f32) xi->time.delta.s;

    play->clouds.timer -= dt;
    if (play->clouds.timer <= 0.0f) {
        ludum_cloud_add(play);
    }

    // update clouds, removing any that have left the screen completely
    //
    for (u32 it = 0; it < LD_MAX_CLOUDS; ++it) {
        ldCloudState *cloud = &play->clouds.states[it];
        if (cloud->enabled) {
            cloud->p.x += (cloud->speed * dt);

            // increase bounds slightly to be sure its completely offscreen so the player can't see
            // any popping out behaviour
            //
            f32 min = 1.05f * (play->min_camera.x - play->clouds.max_dim.w);
            f32 max = 1.05f * (play->max_camera.x + play->clouds.max_dim.w);

            cloud->enabled = (cloud->p.x >= min) && (cloud->p.x <= max);

            // was enabled, no longer is
            //
            if (!cloud->enabled) {
                cloud->next = play->clouds.free;
                play->clouds.free = it;

                play->clouds.count -= 1;
            }
        }
    }

    // prevent the camera from leaving the map area
    //
    if (!kb->alt) {
        rect3 bounds = xi_camera_bounds_get(&camera);

        if (bounds.min.x < play->min_camera.x) { play->camera_p.x += (play->min_camera.x - bounds.min.x); }
        if (bounds.max.x > play->max_camera.x) { play->camera_p.x += (play->max_camera.x - bounds.max.x); }

        if (bounds.min.y < play->min_camera.y) { play->camera_p.y += (play->min_camera.y - bounds.min.y); }
        if (bounds.max.y > play->max_camera.y) { play->camera_p.y += (play->max_camera.y - bounds.max.y); }
    }

    play->angle += dt;

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
	// handle car updates
	ludum_car_update(play, dt);
}

static void ludum_mode_play_render(ldModePlay *play, xiRenderer *renderer) {
    ldContext *ld = play->ld;
    xiContext *xi = ld->xi;

    // basic camera transform
    //
    v3 x = xi_v3_create(1, 0, 0);
    v3 y = xi_v3_create(0, 1, 0);
    v3 z = xi_v3_create(0, 0, 1);

    v3 p = xi_v3_from_v2(play->camera_p, play->zoom);

    xi_camera_transform_set_from_axes(renderer, x, y, z, p, 0);

    xi_v2 map_center = xi_v2_mul_f32(xi_v2_add(play->min_camera, play->max_camera), 0.5f);
    xi_v2 map_bounds = xi_v2_sub(play->max_camera, play->min_camera);

    xi_f32 scale = XI_MAX(map_bounds.x, map_bounds.y);
    xi_sprite_draw_xy_scaled(renderer, play->map, map_center, scale, 0);

	// draw cars
	for(u32 i = 0; i < play->carCount; i++) {
		/* CAR ROUTE FOR DEBUG
		u32 lastStop = play->cars[i].route.stops[0];
		for (u32 it = 1; it < play->cars[i].route.stop_count; it++) {
			nav_mesh_node *node1 = &play->nodes[lastStop];
			v2 node_d1 = xi_v2_create(node1->x, node1->y);
			nav_mesh_node *node2 = &play->nodes[play->cars[i].route.stops[it]];
			v2 node_d2 = xi_v2_create(node2->x, node2->y);
			v4 b = xi_v4_create(0, 0, 1, 1);
			xi_line_draw_xy(renderer, b, node_d2, b, node_d1, 0.02);
			lastStop = play->cars[i].route.stops[it];
		}
		*/
		ldCar car = play->cars[i];
        xi_sprite_draw_xy_scaled(renderer, car.images[car.direction], car.position, 0.1, 0);
	}
    xi_quad_outline_draw_xy(renderer, xi_v4_create(1, 0, 0, 1), map_center, map_bounds, 0, 0.02f);

    for (u32 it = 0; it < play->node_count; ++it) {
        nav_mesh_node *node = &play->nodes[it];

        v4 r = xi_v4_create(1, 0, 0, 1);

        v2 node_p = xi_v2_create(node->x, node->y);
        xi_quad_draw_xy(renderer, r, node_p, xi_v2_create(0.02, 0.02), 0);
    }

    f32 cloud_scale = 2.8f;

    // draw cloud shadows
    //
    for (u32 it = 0; it < LD_MAX_CLOUDS; ++it) {
        ldCloudState *cloud = &play->clouds.states[it];
        if (cloud->enabled) {
            xiImageHandle image = play->clouds.shadows[cloud->index];

            v2 scale = xi_v2_create(cloud->scale * cloud_scale, cloud_scale);
            xi_sprite_draw_xy(renderer, image, cloud->p, scale, 0);
        }
    }

    // push new layer for clouds above the rest of the world
    //
    renderer->layer_offset = 0.7f;
    xi_renderer_layer_push(renderer);

    renderer->layer_offset = 0.5f; // :engine_work probably just want a set call for this

    for (u32 it = 0; it < LD_MAX_CLOUDS; ++it) {
        ldCloudState *cloud = &play->clouds.states[it];

        xi_renderer_layer_push(renderer);

        f32 alpha = XI_CLAMP((play->zoom - renderer->layer) / LD_TOTAL_MAX_ZOOM, 0, 1);
        v4 a = xi_v4_create(1, 1, 1, alpha);

        if (cloud->enabled) {
            xiImageHandle image = play->clouds.images[cloud->index];

            v2 scale = xi_v2_create(cloud->scale * cloud_scale, cloud_scale);
            xi_coloured_sprite_draw_xy(renderer, image, a, cloud->p, scale, 0);
        }

        renderer->layer_offset *= 0.85f;
    }

    xi_logger_flush(&play->log);
}
