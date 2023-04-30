#if !defined(NAV_MESH_H_)
#define NAV_MESH_H_
#include <xi/xi.h>

typedef struct nav_mesh_node {
	xi_f32 x;
	xi_f32 y;
	xi_u32 connectionCount;
	xi_u32 connections[32];
} nav_mesh_node;

typedef struct nav_mesh {
	xi_u32 nodeCount;
	nav_mesh_node *nodes;
} nav_mesh;

int *generateRoute(nav_mesh *mesh, xi_u32 currentIndex, xi_u32  targetIndex);

#endif
