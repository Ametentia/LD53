#if !defined(LUDUM_H_)
#define LUDUM_H_

#include <xi/xi.h>

// typedefs for simpler typenames
//
typedef xi_u8  u8;
typedef xi_u16 u16;
typedef xi_u32 u32;
typedef xi_u64 u64;

typedef xi_s8  s8;
typedef xi_s16 s16;
typedef xi_s32 s32;
typedef xi_s64 s64;

typedef xi_s8  b8;
typedef xi_s16 b16;
typedef xi_s32 b32;
typedef xi_s64 b64;

typedef xi_sptr sptr;
typedef xi_uptr uptr;

typedef xi_f32 f32;
typedef xi_f64 f64;

typedef xi_v2u v2u;
typedef xi_v2s v2s;
typedef xi_v2  v2;
typedef xi_v3  v3;
typedef xi_v4  v4;

typedef xi_m2x2 m2x2;
typedef xi_m4x4 m4x4;

typedef xi_rect2 rect2;
typedef xi_rect3 rect3;

// mode typedefs so we don't have to have unneeded headers
//
typedef struct ldModePlay ldModePlay;

enum ldModeType {
    LD_MODE_NONE = 0,
    LD_MODE_MENU, // @incomplete: no menu yet
    LD_MODE_PLAY
};

typedef struct ldContext {
    xiContext *xi;

    xiArena perm_arena;
    xiArena mode_arena;

    u32 mode;
    union {
        ldModePlay *play;
    };
} ldContext;

// :engine_work this really should be defined along with XI_MIN, XI_MAX and XI_CLAMP
//
#define ABS(x) ((x) < 0 ? -(x) : (x))

#endif  // LUDUM_H_
