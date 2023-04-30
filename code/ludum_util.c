// this really should be part of the engine, here for now :engine_work
//
static void ludum_camera_transform_get(xiCameraTransform *camera, f32 aspect_ratio,
        v3 x_axis, v3 y_axis, v3 z_axis, v3 p, u32 flags, f32 near_plane, f32 far_plane, f32 focal_length)
{
    camera->x_axis   = x_axis;
    camera->y_axis   = y_axis;
    camera->z_axis   = z_axis;

    camera->position = p;

    xi_m4x4_inv projection;

    if (flags & XI_CAMERA_TRANSFORM_FLAG_ORTHOGRAPHIC) {
        projection = xi_m4x4_orthographic_projection(aspect_ratio, near_plane, far_plane);
    }
    else {
        projection = xi_m4x4_perspective_projection(focal_length, aspect_ratio, near_plane, far_plane);
    }

    xi_m4x4_inv tx = xi_m4x4_from_camera_transform(x_axis, y_axis, z_axis, p);

    camera->transform.fwd = xi_m4x4_mul(projection.fwd, tx.fwd);
    camera->transform.inv = xi_m4x4_mul(tx.inv, projection.inv);
}

static void ludum_camera_transform_get_from_axes(xiCameraTransform *camera, f32 aspect_ratio,
        v3 x_axis, v3 y_axis, v3 z_axis, v3 p, u32 flags)
{
    xi_f32 focal_length = 1.5696855771174903493f; // ~65 degree fov
    ludum_camera_transform_get(camera, aspect_ratio, x_axis, y_axis, z_axis,
            p, flags, 0.001f, 10000.0f, focal_length);
}
