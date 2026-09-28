#include "kernel/PX_LiveFramework.h"

#include <math.h>
#include <stdio.h>

static px_bool PX_LiveFrameworkPancTestNear(px_float actual,px_float expected)
{
	return fabsf(actual-expected)<0.001f;
}

static px_void PX_LiveFrameworkPancTestSetTracking(PX_LiveLayer *pLayer)
{
	pLayer->panc_x=0;
	pLayer->panc_y=0;
	pLayer->panc_width=100;
	pLayer->panc_height=100;
	pLayer->panc_sx=50;
	pLayer->panc_sy=50;
	pLayer->panc_beginx=50;
	pLayer->panc_beginy=50;
	pLayer->panc_currentx=60;
	pLayer->panc_currenty=50;
	pLayer->panc_endx=60;
	pLayer->panc_endy=50;
}

int main(px_void)
{
	px_byte memory[64*1024];
	px_memorypool mp=PX_MemoryPoolCreate(memory,sizeof(memory));
	PX_LiveFramework live;
	PX_LiveLayer *pRoot,*pChild;
	PX_LiveVertex rootVertex,childVertex;

	if (!PX_LiveFrameworkInitialize(&mp,&live,100,100))
	{
		return 1;
	}
	if (!PX_LiveFrameworkCreateLayer(&live,"root")||!PX_LiveFrameworkCreateLayer(&live,"child"))
	{
		return 1;
	}

	pRoot=PX_LiveFrameworkGetLayer(&live,0);
	pChild=PX_LiveFrameworkGetLayer(&live,1);
	pRoot->keyPoint=PX_POINT(20,50,1);
	pChild->keyPoint=PX_POINT(40,50,1);
	pChild->rel_beginStretch=0.5f;
	pChild->rel_currentStretch=0.5f;
	pChild->rel_endStretch=0.5f;
	PX_LiveFrameworkPancTestSetTracking(pRoot);
	PX_LiveFrameworkPancTestSetTracking(pChild);
	PX_LiveFrameworkLinkLayer(&live,pRoot,pChild);

	PX_memset(&rootVertex,0,sizeof(rootVertex));
	rootVertex.sourcePosition=PX_POINT(80,50,1);
	rootVertex.currentPosition=rootVertex.sourcePosition;
	PX_VectorPushback(&pRoot->vertices,&rootVertex);

	PX_memset(&childVertex,0,sizeof(childVertex));
	childVertex.sourcePosition=pChild->keyPoint;
	childVertex.currentPosition=childVertex.sourcePosition;
	PX_VectorPushback(&pChild->vertices,&childVertex);

	PX_LiveFrameworkRender(PX_NULL,&live,0,0,PX_ALIGN_LEFTTOP,0);

	rootVertex=*PX_VECTORAT(PX_LiveVertex,&pRoot->vertices,0);
	childVertex=*PX_VECTORAT(PX_LiveVertex,&pChild->vertices,0);
	if (!PX_LiveFrameworkPancTestNear(pRoot->currentKeyPoint.x,24)||
		!PX_LiveFrameworkPancTestNear(pChild->currentKeyPoint.x,36)||
		!PX_LiveFrameworkPancTestNear(rootVertex.currentPosition.x,54)||
		!PX_LiveFrameworkPancTestNear(childVertex.currentPosition.x,pChild->currentKeyPoint.x))
	{
		printf("unexpected tracking result: root_key=%f child_key=%f root_vertex=%f child_vertex=%f\n",
			pRoot->currentKeyPoint.x,pChild->currentKeyPoint.x,
			rootVertex.currentPosition.x,childVertex.currentPosition.x);
		return 1;
	}

	pRoot->panc_endx=pRoot->panc_sx;
	pChild->panc_endx=pChild->panc_sx;
	PX_LiveFrameworkRender(PX_NULL,&live,0,0,PX_ALIGN_LEFTTOP,0);
	rootVertex=*PX_VECTORAT(PX_LiveVertex,&pRoot->vertices,0);
	childVertex=*PX_VECTORAT(PX_LiveVertex,&pChild->vertices,0);
	if (!PX_LiveFrameworkPancTestNear(pRoot->currentKeyPoint.x,20)||
		!PX_LiveFrameworkPancTestNear(pChild->currentKeyPoint.x,30)||
		!PX_LiveFrameworkPancTestNear(rootVertex.currentPosition.x,50)||
		!PX_LiveFrameworkPancTestNear(childVertex.currentPosition.x,30))
	{
		printf("unexpected untracked result: root_key=%f child_key=%f root_vertex=%f child_vertex=%f\n",
			pRoot->currentKeyPoint.x,pChild->currentKeyPoint.x,
			rootVertex.currentPosition.x,childVertex.currentPosition.x);
		return 1;
	}

	PX_LiveFrameworkFree(&live);
	return 0;
}
