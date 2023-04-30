#include <xi/xi.h>
#include "nav_mesh.c"
#include "nav_mesh_data.h"

extern XI_EXPORT XI_GAME_INIT(xiContext *xi, xi_u32 type) {
    switch (type) {
        case XI_ENGINE_CONFIGURE: {
            // @todo: configure engine parameters here
            //
        }
        break;
        case XI_GAME_INIT: {
            // @todo: init game here
            // TODO: @todo remove this debug path finding call
			generateRoute(&NAV_MESH, 0, 5991);
        }
        break;
        case XI_GAME_RELOADED: {
        }
        break;
    }
}

extern XI_EXPORT XI_GAME_SIMULATE(xiContext *xi) {
}

extern XI_EXPORT XI_GAME_RENDER(xiContext *xi, xiRenderer *renderer) {
    // basic camera transform
    //
    xi_v3 x = xi_v3_create(1, 0, 0);
    xi_v3 y = xi_v3_create(0, 1, 0);
    xi_v3 z = xi_v3_create(0, 0, 1);
    xi_v3 p = xi_v3_create(0, 0, 3.5);

    xi_camera_transform_set_axes(renderer, x, y, z, p, 0);

    // draw red square
    //
    xi_quad_draw_xy(renderer, xi_v4_create(1, 1, 0, 1), xi_v2_create(0, 0), xi_v2_create(1, 1), 0);

}
