#include <xi/xi.h>
#include "nav_mesh.h"
#include <time.h>
#define ABS(x) ((x) < 0 ? -(x) : (x))

int *generateRoute(nav_mesh *mesh, xi_u32 currentIndex, xi_u32 targetIndex) {
	clock_t t;
	t = clock();
	xi_u32 startNode = currentIndex;
	xi_u32 nodeVisited[mesh->nodeCount];
	xi_f32 nodeDistance[mesh->nodeCount];
	xi_u32 backTrackNode[mesh->nodeCount];
	xi_u32 checkedNodes[mesh->nodeCount];
	xi_u32 nodeChecked[mesh->nodeCount];
	xi_u32 checkedNodeCount = 0;
	xi_u32 distance;
	for(xi_u32 i = 0; i < mesh->nodeCount; i++) {
		nodeVisited[i] = 0;
		distance = 4294967295;
		if(currentIndex == i) {
			distance = 0;
		}
		nodeDistance[i] = distance;
		backTrackNode[i] = 0;
		checkedNodes[i] = 0;
		nodeChecked[i] = 0;
	}
	xi_u32 checkNode;
	xi_u32 calDistance;
   	xi_u32 smallestClosest;
	xi_u32 nextNode;
	xi_f32 xDiff;
	xi_f32 yDiff;
	xi_u32 node;
	xi_u32 checkSnip = 0;
	xi_u32 noMisses = 0;
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
		smallestClosest = 4294967295;
		nextNode = 0;
		noMisses = 1;
		for(xi_u32 i = checkSnip; i < checkedNodeCount; i++) {
			node = checkedNodes[i];
			if(nodeVisited[node]){
				if(noMisses)
					checkSnip++;
				continue;
			}
			noMisses = 0;
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
