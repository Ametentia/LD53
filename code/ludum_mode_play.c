//#define LD_DEFAULT_MIN_ZOOM 12.0f
#define LD_DEFAULT_MIN_ZOOM 0.70f
#define LD_DEFAULT_MAX_ZOOM 4.5f // @incomplete: this will break when we limit zoom
#define LD_TOTAL_MAX_ZOOM   4.0f
#define LD_DEBUG_MAX_ZOOM   12.0f

#define LD_AMBIENT_MUSIC_TAG 0x3322

#define LD_SHORT_RADIO_TAG   0x4821
#define LD_LONG_RADIO_TAG    0x4822

#define LD_CLOUD_MIN_MOVE_SPEED 0.05f
#define LD_CLOUD_MAX_MOVE_SPEED 0.18f

#define LD_CLOUD_MIN_SPAWN_TIME 4.8f
#define LD_CLOUD_MAX_SPAWN_TIME 10.2f
#define LD_CLOUD_SCALE 2.8f

#define LD_CLOUD_VARIATION_COUNT 6
#define LD_MAX_CLOUDS 6

#define LD_STATIC_LONG_TIMER_THRESHOLD 5.5f

#define LD_NUM_RADIO_STATIC 7

#define LD_MAX_STORE_ORDERS 12
#define LD_CAR_HIT_BOX_SIZE 0.1
#define LD_MAX_CAR_ORDERS 3
#define LD_ORDER_MARKER_HIT_BOX_X 0.1
#define LD_ORDER_MARKER_HIT_BOX_Y 0.2

typedef enum ldCarDir {
    LD_CAR_DIR_FRONT = 0,
    LD_CAR_DIR_SIDE,
    LD_CAR_DIR_BACK,
    LD_CAR_DIR_FRONT_SELECTED,
    LD_CAR_DIR_SIDE_SELECTED,
    LD_CAR_DIR_BACK_SELECTED,

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

typedef struct ldStore ldStore;

typedef struct ldOrder {
	u32 index;
	v2 position;
	f32 timer;
	u32 assignedDriver;
	b32 collected;
	ldStore *store;
} ldOrder;

typedef struct ldCar {
	v2 position;
	nav_route route;
	u32 currentRouteStopTarget;
	u32 currentNodeIndex;
	f32 speed;
	f32 nodeProgress;
	f32 onBreak;
    ldCarDir direction;
	ldOrder *assignedOrders[LD_MAX_CAR_ORDERS];
	u32 orderCount;
	u32 currentOrder;
	u32 orderOffset;
} ldCar;

typedef struct ldStore {
	v2 position;
	u32 index;
	u32 orderCount;
	ldOrder orders[LD_MAX_STORE_ORDERS];
	f32 orderDelayMax;
	f32 orderDelayMin;
	f32 nextOrderIn;
} ldStore;

typedef union ldMusicTag {
    struct {
        u16 type; // ldMusicType
        b8  radio;
        u8  index;
    };

    u32 value;
} ldMusicTag;

typedef struct ldCloudState ldCloudState;

typedef struct ldCloudState {
    b32 enabled;
    u32 index;

    v2 p;
    f32 layer;
    f32 speed;
    f32 scale;
    f32 darkness;

    ldCloudState *next;
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

	// Cars
	u32 selectedCar;
	u32 carCount;
	ldCar cars[25];
    xiImageHandle carImages[6];
	xiImageHandle licenseHandle;
	xiImageHandle order_empty_handle;
	xiImageHandle order_filled_handle;

	// Stores
	u32 storeCount;
	ldStore stores[10];
	xiImageHandle storeMarker;
	xiImageHandle orderMarker;
	xiImageHandle orderMarkerAssigned;
	xiImageHandle orderBubble;

    struct {
        xiImageHandle  images[LD_CLOUD_VARIATION_COUNT];
        xiImageHandle shadows[LD_CLOUD_VARIATION_COUNT];

        f32 timer;

        f32 min_y;
        f32 step_y;

        v2 max_dim;

        u32 num_pushed;

        u32 index;
        u32 count;
        ldCloudState *free;
        ldCloudState *clouds;
    } clouds;

    xiSoundHandle static_long;
    xiSoundHandle static_short;

    xiImageHandle map;

    v2 p;
    f32 angle;

    xiLogger log;
};

static b32 pointInRect(v2 point, rect2 rect) {
	return point.x > rect.min.x &&
		point.x < rect.max.x &&
		point.y > rect.min.y &&
		point.y < rect.max.y;
}

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

    play->clouds.free   = 0;
    play->clouds.clouds = 0;

    play->clouds.timer = xi_rng_range_f32(&play->rng, LD_CLOUD_MIN_SPAWN_TIME, LD_CLOUD_MAX_SPAWN_TIME);
    play->clouds.free = 0;
}

static void ludum_cloud_add(ldModePlay *play) {
    xiRandomState *rng = &play->rng;

    if (play->clouds.count < LD_MAX_CLOUDS) {
        ldCloudState *cloud = play->clouds.free;

        if (cloud) {
            play->clouds.free = cloud->next;
        }
        else {
            cloud = xi_arena_push_type(play->arena, ldCloudState);

            XI_ASSERT(play->clouds.num_pushed < LD_MAX_CLOUDS);
            play->clouds.num_pushed += 1;
        }

        u32 index = play->clouds.index;
        play->clouds.index = (index + 1) % LD_MAX_CLOUDS;

        b32 left = (index & 1);

        f32 h = play->clouds.max_dim.h;

        cloud->p.y = play->clouds.min_y + (index * play->clouds.step_y); // + xi_rng_range_f32(rng, -h, h);
        cloud->p.x = left ? (play->min_camera.x - play->clouds.max_dim.w) :
                            (play->max_camera.x + play->clouds.max_dim.w);

        f32 dir = (left ? 1 : -1);

        cloud->layer   = xi_rng_range_f32(rng, 1.2f, 2.95f);
        cloud->scale   = (xi_rng_choice_u32(rng, 2) ? -1 : 1) * xi_rng_range_f32(rng, 0.88f, 1.11f);
        cloud->speed   = dir * xi_rng_range_f32(rng, LD_CLOUD_MIN_MOVE_SPEED, LD_CLOUD_MAX_MOVE_SPEED);
        cloud->index   = 3; // xi_rng_choice_u32(&play->rng, LD_CLOUD_VARIATION_COUNT);
        cloud->enabled = true;

        if (!play->clouds.clouds) {
            // no clouds in list so just push onto head
            //
            play->clouds.clouds = cloud;
            xi_log(&play->log, "clouds", "no cloud, setting to head");
        }
        else {
            ldCloudState *prev = 0;
            ldCloudState *cur  = play->clouds.clouds;
            while (cur != 0) {
                if (cur->layer > cloud->layer) {
                    if (prev) {
                        XI_ASSERT(prev->layer <= cloud->layer);
                        XI_ASSERT(prev->next == cur);

                        prev->next  = cloud;
                    }
                    else {
                        play->clouds.clouds = cloud;
                    }

                    cloud->next = cur;

                    // find the right layer index to insert to
                    //
                    break;
                }

                prev = cur;
                cur  = cur->next;
            }

            // we walked the list and didn't find a layer greater than ours so append to the end
            // (which should be in 'prev')
            //
            if (cur == 0) {
                XI_ASSERT(prev != 0);

                prev->next  = cloud;
                cloud->next = 0;
            }
        }

        play->clouds.count += 1;
    }

    play->clouds.timer = xi_rng_range_f32(rng, LD_CLOUD_MIN_SPAWN_TIME, LD_CLOUD_MAX_SPAWN_TIME);
}

static u32 ludum_find_street_node(ldModePlay *play, u32 start_node) {
	u32 nodeIndex;
	do {
		nodeIndex = xi_rng_choice_u32(&play->rng, play->node_count);
	} while(play->nodes[nodeIndex].motorway || nodeIndex == start_node);
	return nodeIndex;
}

static void ludum_car_init(ldModePlay *play, xiAssetManager *assets) {
	for(u32 i = 0; i < play->carCount; i++) {
		ldCar *car = &play->cars[i];
		u32 start = xi_rng_choice_u32(&play->rng, play->node_count);
		u32 storeIndex = xi_rng_choice_u32(&play->rng, play->storeCount);
		car->route = generateRoute(play->arena, &NAV_MESH, play->stores[storeIndex].index, start);
		car->speed = 10;
		car->orderCount = 0;
		car->orderOffset = 0;
		car->currentRouteStopTarget = 1;
		car->nodeProgress = 0;
        nav_mesh_node *node = &play->nodes[car->route.stops[car->currentRouteStopTarget-1]];
		car->position = xi_v2_create(node->x, node->y);
	}
}

static void ludum_store_init(ldModePlay *play, xiAssetManager *assets) {
	for(u32 i = 0; i < play->storeCount; i++) {
		ldStore *store = &play->stores[i];
		u32 location = ludum_find_street_node(play, 0);
        nav_mesh_node *node = &play->nodes[location];
		store->index = location;
		store->orderCount = 0;
		store->orderDelayMax = 20;
		store->orderDelayMin = 18;
		// Give the player 7 seconds before adding an order?
		store->nextOrderIn = 7;
		store->position = xi_v2_create(node->x, node->y);
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
		play->carCount = 1;
		play->storeMarker = xi_image_get_by_name(&xi->assets, "restaurantMarker");
		play->orderMarker = xi_image_get_by_name(&xi->assets, "orderMarker");
		play->orderMarkerAssigned = xi_image_get_by_name(&xi->assets, "orderMarkerAssigned");
		play->orderBubble = xi_image_get_by_name(&xi->assets, "pulse");
		play->storeCount = 1;
		ludum_store_init(play, &xi->assets);
		ludum_car_init(play, &xi->assets);
		play->selectedCar = -1;
        play->carImages[LD_CAR_DIR_FRONT] = xi_image_get_by_name_str(&xi->assets, xi_str_wrap_cstr("car_front"));
        play->carImages[LD_CAR_DIR_SIDE] = xi_image_get_by_name_str(&xi->assets, xi_str_wrap_cstr("car_side"));
        play->carImages[LD_CAR_DIR_BACK] = xi_image_get_by_name_str(&xi->assets, xi_str_wrap_cstr("car_back"));
        play->carImages[LD_CAR_DIR_FRONT_SELECTED] = xi_image_get_by_name_str(&xi->assets, xi_str_wrap_cstr("car_front_selected"));
        play->carImages[LD_CAR_DIR_SIDE_SELECTED] = xi_image_get_by_name_str(&xi->assets, xi_str_wrap_cstr("car_side_selected"));
        play->carImages[LD_CAR_DIR_BACK_SELECTED] = xi_image_get_by_name_str(&xi->assets, xi_str_wrap_cstr("car_back_selected"));
        play->licenseHandle = xi_image_get_by_name_str(&xi->assets, xi_str_wrap_cstr("license_01"));
        play->order_empty_handle = xi_image_get_by_name_str(&xi->assets, xi_str_wrap_cstr("empty_order"));
        play->order_filled_handle = xi_image_get_by_name_str(&xi->assets, xi_str_wrap_cstr("order_filled"));

        ld->mode = LD_MODE_PLAY;
    }

    ld->play = play;
}

static void ludum_store_update(ldModePlay *play, f32 delta) {
	for(int i = 0; i < play->storeCount; i++) {
		ldStore *store = &play->stores[i];

		store->nextOrderIn -= delta;
		if (store->nextOrderIn < 0 && store->orderCount < LD_MAX_STORE_ORDERS) {
            xi_log(&play->log, "order", "new order incoming!");

			ldOrder *order = &store->orders[store->orderCount];
			order->index = ludum_find_street_node(play, store->index);

			nav_mesh_node *node = &play->nodes[order->index];
			order->position = xi_v2_create(node->x, node->y);
			// TODO: change this to how far the shop is away?
			order->timer = 60;
			order->assignedDriver = -1;
			order->store= store;
			order->collected = false;
			store->orderCount++;
			store->nextOrderIn = store->orderDelayMin + xi_rng_choice_u32(&play->rng, store->orderDelayMax-store->orderDelayMin);
		}

		for(int it = 0; it < store->orderCount; it++) {
			ldOrder *order = &store->orders[it];
			order->timer -= delta;
		}
	}
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
			car->currentNodeIndex = endStop;
			if(car->currentRouteStopTarget == car->route.stop_count){
				if(car->orderCount > 0) {
					ldOrder *order = car->assignedOrders[car->currentOrder];
					u32 targetNode = order->index;
					if(order->collected) {
						targetNode = order->store->index;
					}
					car->route = generateRoute(play->arena, &NAV_MESH, targetNode, car->currentNodeIndex);
				} else {
					u32 storeIndex = xi_rng_choice_u32(&play->rng, play->storeCount);
					u32 targetIndex = play->stores[storeIndex].index;
					car->route = generateRoute(play->arena, &NAV_MESH, targetIndex, car->currentNodeIndex);
					car->currentRouteStopTarget = 1;
					car->onBreak = 1.5f;
				}
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

    v2u window_dim = xi->renderer.setup.window_dim;
    f32 aspect = (f32) window_dim.w / (f32) window_dim.h;
    v3 x = xi_v3_create(1, 0, 0);
    v3 y = xi_v3_create(0, 1, 0);
    v3 z = xi_v3_create(0, 0, 1);

    v3 p = xi_v3_from_v2(play->camera_p, play->zoom);
    xiCameraTransform camera;
    xi_camera_transform_get_from_axes(&camera, aspect, x, y, z, p, 0);

    if (kb->alt) {
        play->max_zoom = LD_DEBUG_MAX_ZOOM;
    }
    else {
        play->max_zoom = LD_TOTAL_MAX_ZOOM;
    }

    f32 dt = (f32) xi->time.delta.s;

    // handle camera movement
    //
    play->zoom += (1.88f * mouse->delta.wheel.y * dt);
    play->zoom  = XI_CLAMP(play->zoom, LD_DEFAULT_MIN_ZOOM, play->max_zoom);

	if (mouse->left.pressed && xi_v2_length(mouse->delta.clip) == 0) {
        xi_log(&play->log, "input", "pressed");
		b8 orderAssigned = false;
		if(play->selectedCar != -1) {
			// Check if an order wants to be assigned
			ldCar *car = &play->cars[play->selectedCar];
			ldOrder *order = 0;
			if(car->orderCount < LD_MAX_CAR_ORDERS) {
				for(u32 i = 0; i < play->storeCount; i++) {
					for(u32 it = 0; it < play->stores[i].orderCount; it++) {
						rect2 orderHitbox = {0};
						ldOrder *order = &play->stores[i].orders[it];
						orderHitbox.min = xi_v2_create(
							order->position.x - LD_ORDER_MARKER_HIT_BOX_X/2,
							order->position.y - LD_ORDER_MARKER_HIT_BOX_Y/2
						);
						orderHitbox.max = xi_v2_create(
							order->position.x + LD_ORDER_MARKER_HIT_BOX_X/2,
							order->position.y + LD_ORDER_MARKER_HIT_BOX_Y/2
						);
						v3 mouseUnprojectedV3 = xi_unproject_xy(&camera, mouse->position.clip);
						v2 mouseUnprojected = xi_v2_create(mouseUnprojectedV3.x, mouseUnprojectedV3.y);
						if(pointInRect(mouseUnprojected, orderHitbox), order->assignedDriver == -1) {
							if(car->orderCount == 0) {
								u32 storeIndex = order->store->index;
								car->route = generateRoute(play->arena, &NAV_MESH, storeIndex, car->currentNodeIndex);
							}
							car->assignedOrders[car->orderOffset] = order;
							car->orderCount++;
							car->orderOffset = (car->orderOffset + 1) % LD_MAX_CAR_ORDERS;
							order->assignedDriver = play->selectedCar;
							orderAssigned = true;
							i = play->storeCount+1;
							it = play->stores[i].orderCount+1;
						}

					}
				}
			}
		}
	   	if(!orderAssigned){
			u32 pastSelected = play->selectedCar;
			play->selectedCar = -1;
			for(u32 i = 0; i < play->carCount; i++) {
				ldCar *car = &play->cars[i];
				rect2 carHitbox = {0};
				carHitbox.min = xi_v2_create(
					car->position.x - LD_CAR_HIT_BOX_SIZE/2,
					car->position.y - LD_CAR_HIT_BOX_SIZE/2
				);
				carHitbox.max = xi_v2_create(
					car->position.x + LD_CAR_HIT_BOX_SIZE/2,
					car->position.y + LD_CAR_HIT_BOX_SIZE/2
				);
				v3 mouseUnprojectedV3 = xi_unproject_xy(&camera, mouse->position.clip);
				v2 mouseUnprojected = xi_v2_create(mouseUnprojectedV3.x, mouseUnprojectedV3.y);
				if(pointInRect(mouseUnprojected, carHitbox)) {
					play->selectedCar = i;
					xi_log(&play->log, "car", "selected a car %d", i);
					break;
				}
			}
			if(play->selectedCar != pastSelected && play->selectedCar!=-1) {
				ludum_music_layer_random_play(play, audio);
			} else if(play->selectedCar == -1) {
				xi_music_layer_disable_by_tag(audio, play->music.current.value, XI_MUSIC_LAYER_EFFECT_FADE, 0.5f);
				play->music.playing = false;
				play->music.plays   = 0;
			}
		}
	} else if (mouse->left.down) {
		v2 dist = xi_v2_mul_f32(xi_v2_neg(mouse->delta.clip), 0.88f * play->zoom);
		play->camera_p = xi_v2_add(play->camera_p, dist);
    }

    p = xi_v3_from_v2(play->camera_p, play->zoom);
    xi_camera_transform_get_from_axes(&camera, aspect, x, y, z, p, 0);

    play->clouds.timer -= dt;
    if (play->clouds.timer <= 0.0f) {
        ludum_cloud_add(play);
    }

    // update clouds, removing any that have left the screen completely
    //
    ldCloudState *prev  = 0;
    ldCloudState *cloud = play->clouds.clouds;
    while (cloud != 0) {
        cloud->p.x += (cloud->speed * dt);

        XI_ASSERT(!prev || (prev->next == cloud));

        f32 min = 1.05f * (play->min_camera.x - play->clouds.max_dim.w);
        f32 max = 1.05f * (play->max_camera.x + play->clouds.max_dim.w);

        if ((cloud->p.x < min) || (cloud->p.x > max)) {
            XI_ASSERT(play->clouds.count > 0);
            play->clouds.count -= 1;

            // remove from active list
            //
            if (prev) { prev->next = cloud->next; }
            else { play->clouds.clouds = cloud->next; }

            // put on freelist
            //
            cloud->next = play->clouds.free;
            play->clouds.free = cloud;

            cloud = prev ? prev->next : play->clouds.clouds;
        }
        else {
            // we didn't remove it so this cloud was our previous, otherwise
            // we don't update the prev because it was removed from the active list
            //
            prev  = cloud;
            cloud = cloud->next;
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

	ludum_store_update(play, dt);

	// handle car updates
    //
	ludum_car_update(play, dt);

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
    ldContext *ld = play->ld;
    xiContext *xi = ld->xi;

    // basic camera transform
    //
    v3 x = xi_v3_create(1, 0, 0);
    v3 y = xi_v3_create(0, 1, 0);
    v3 z = xi_v3_create(0, 0, 1);

    v3 p = xi_v3_from_v2(play->camera_p, play->zoom);

    xi_camera_transform_set_from_axes(renderer, x, y, z, p, 0);
	rect3 bounds = xi_camera_bounds_get(&renderer->camera);

    xi_v2 map_center = xi_v2_mul_f32(xi_v2_add(play->min_camera, play->max_camera), 0.5f);
    xi_v2 map_bounds = xi_v2_sub(play->max_camera, play->min_camera);

    xi_f32 scale = XI_MAX(map_bounds.x, map_bounds.y);
    xi_sprite_draw_xy_scaled(renderer, play->map, map_center, scale, 0);
	// draw cars
	for(u32 i = 0; i < play->carCount; i++) {
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
		ldCar car = play->cars[i];
		v4 b = xi_v4_create(0, 0, 1, 1);
		u32 selectedOffset = 0;
		if(play->selectedCar==i)
			selectedOffset = 3;
        xi_sprite_draw_xy_scaled(renderer, play->carImages[car.direction+selectedOffset], car.position, 0.1, 0);
	}

	// draw stores
	for(u32 i = 0; i < play->storeCount; i++) {
        xi_sprite_draw_xy_scaled(renderer, play->storeMarker, play->stores[i].position, 0.1, 0);

		for(u32 it = 0; it < play->stores[i].orderCount; it++) {
			ldOrder *order = &play->stores[i].orders[it];

			xiImageHandle marker = play->orderMarkerAssigned;

            v2 pos = order->position;
            pos.x = XI_CLAMP(order->position.x, bounds.min.x, bounds.max.x);
            pos.y = XI_CLAMP(order->position.y, bounds.min.y, bounds.max.y);

			if(play->stores[i].orders[it].assignedDriver == -1) {
				marker = play->orderMarker;
                v4 colour = xi_v4_create(0, 1, 0, 1);

                f32 time_remaining = order->timer / 60.0f; // @hardcode: from above when orders are spawned
                if (time_remaining < 0.15f) {
                    colour = xi_v4_create(1.0f, 0.0f, 0.0f, 1.0f);
                }
                else if (time_remaining < 0.5f) {
                    colour = xi_v4_create(1.0f, 0.65f, 0.0f, 1.0f);
                }


        		xi_coloured_sprite_draw_xy_scaled(renderer, play->orderBubble, colour, pos, 0.1+xi_sin(order->timer), 0);
			}

        	xi_sprite_draw_xy_scaled(renderer, marker, pos, 0.1, 0);
		}

	}

    xi_quad_outline_draw_xy(renderer, xi_v4_create(1, 0, 0, 1), map_center, map_bounds, 0, 0.02f);

    f32 cloud_scale = 2.8f;

    // draw cloud shadows
    //
    ldCloudState *cloud = play->clouds.clouds;
    while (cloud != 0) {
        xiImageHandle image = play->clouds.shadows[cloud->index];

        v2 scale = xi_v2_create(cloud->scale * cloud_scale, cloud_scale);
        xi_sprite_draw_xy(renderer, image, cloud->p, scale, 0);

        cloud = cloud->next;
    }

    cloud = play->clouds.clouds;
    while (cloud != 0) {
        // :note clouds are in layer sorted order so this is safe for transparency order.
        //
        // :engine_work it would be nice to have this auto-sorted or something but would require
        // a bit of restructuring
        //
        renderer->layer = cloud->layer;

        xiImageHandle image = play->clouds.images[cloud->index];

        f32 alpha = XI_CLAMP((play->zoom - renderer->layer) / LD_TOTAL_MAX_ZOOM, 0, 1);
        v4 a = xi_v4_create(1, 1, 1, alpha);

        v2 scale = xi_v2_create(cloud->scale * cloud_scale, cloud_scale);
        xi_coloured_sprite_draw_xy(renderer, image, a, cloud->p, scale, 0);

        renderer->layer_offset *= 0.85f;

        cloud = cloud->next;
    }

	renderer->layer = 0;
    x = xi_v3_create(3, 0, 0);
    y = xi_v3_create(0, 3, 0);
    z = xi_v3_create(0, 0, 3);
    p = xi_v3_create(0, 0, 3);

    xi_camera_transform_set_from_axes(renderer, x, y, z, p, XI_CAMERA_TRANSFORM_FLAG_ORTHOGRAPHIC);
	if(play->selectedCar != -1) {
    	xi_sprite_draw_xy(renderer, play->licenseHandle, xi_v2_create(0.25, -0.15), xi_v2_create(0.15, 0.15), 0);
		ldCar car = play->cars[play->selectedCar];
		for(u32 i = 0; i < LD_MAX_CAR_ORDERS; i++) {
			xiImageHandle handle = play->order_filled_handle;
			if(car.orderCount <= i) {
				handle = play->order_empty_handle;
			}
    		xi_sprite_draw_xy(renderer, handle, xi_v2_create(0.223+0.025*i, -0.15), xi_v2_create(0.025, 0.025), 0);
		}
	}

    xi_logger_flush(&play->log);
}
