#include <time.h>

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

// :note this header is generated automatically with tools/mapGenerator
//
#include "nav_mesh_data.h"

int *generateRoute(nav_mesh *mesh, xi_u32 currentIndex, xi_u32 targetIndex) {
	clock_t t;
	t = clock();

    XI_ASSERT(targetIndex < mesh->nodeCount);

	xi_u32 startNode = currentIndex;

    xiArena *temp = xi_temp_get();

    // :note msvc doesn't allow arrays to be created with non-constant expressions so i have changed
    // this over to allocate from the temporary arena instead
    //
	xi_u32 *nodeVisited   = xi_arena_push_array(temp, xi_u32, mesh->nodeCount);
	xi_f32 *nodeDistance  = xi_arena_push_array(temp, xi_f32, mesh->nodeCount);
	xi_u32 *backTrackNode = xi_arena_push_array(temp, xi_u32, mesh->nodeCount);
	xi_u32 *checkedNodes  = xi_arena_push_array(temp, xi_u32, mesh->nodeCount);
	xi_u32 *nodeChecked   = xi_arena_push_array(temp, xi_u32, mesh->nodeCount);

	xi_u32 checkedNodeCount = 0;

	for (xi_u32 i = 0; i < mesh->nodeCount; i++) {
		nodeDistance[i] = (currentIndex == i) ? 0 : XI_U32_MAX;
	}

	xi_u32 checkNode;
	xi_u32 calDistance;
   	xi_u32 smallestClosest;
	xi_u32 nextNode;
	xi_f32 xDiff;
	xi_f32 yDiff;
	xi_u32 node;
	xi_u32 checkSnip = 0;
	xi_b32 noMisses  = true;

	while(!nodeVisited[targetIndex]) {
		for(xi_u32 i = 0; i < mesh->nodes[currentIndex].connectionCount; i++) {
			checkNode = mesh->nodes[currentIndex].connections[i];

			if(nodeVisited[checkNode])
				continue;

			if(!nodeChecked[checkNode]){
				checkedNodes[checkedNodeCount] = checkNode;
				checkedNodeCount++;
				nodeChecked[checkNode] = 1;
			}

			xDiff = ABS(mesh->nodes[targetIndex].x - mesh->nodes[checkNode].x);
			yDiff = ABS(mesh->nodes[targetIndex].y - mesh->nodes[checkNode].y);

			calDistance = xDiff+yDiff+1+nodeDistance[currentIndex];
			if(nodeDistance[checkNode] > calDistance) {
				nodeDistance[checkNode] = calDistance;
				backTrackNode[checkNode] = currentIndex;
			}
		}

		nodeVisited[currentIndex] = 1;
		smallestClosest = XI_U32_MAX;
		nextNode = 0;
		for(xi_u32 i = checkSnip; i < checkedNodeCount; i++) {
			node = checkedNodes[i];
			if(nodeVisited[node]){
				if(noMisses)
					checkSnip++;

				continue;
			}

			noMisses = false;

			if(nodeDistance[node] < smallestClosest) {
				smallestClosest = nodeDistance[node];
				nextNode = node;
				currentIndex = nextNode;
			}
		}
	}

	while(currentIndex!=startNode)
		currentIndex = backTrackNode[currentIndex];

	t = clock() - t;
	double timeTaken = ((double)t/(CLOCKS_PER_SEC/1000.0));

	return 0;
}
