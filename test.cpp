#include <stdlib.h>
#include "glib2d.h"


int main(void)
{
	g2dInit();
	g2dTexture* tex;
	tex= g2dTexLoad("jtex.jpg",G2D_SWIZZLE);
	while(1)
	{
		g2dClear(BLACK);
		g2dBeginRects(tex); 
	//	g2dSetCoordMode(G2D_DOWN_RIGHT);
	//	g2dSetColor(WHITE);
		g2dSetCoordXY(100,100);
		g2dSetScaleWH(227,149);
		g2dAdd();

		g2dEnd();
		g2dFlip(G2D_VSYNC);		
	}
	g2dTerm();
	return 0;

}

