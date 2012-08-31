#include <stdlib.h>
#include "glib2d.h"
#include "fontstash.h"

int main(void)
{
	g2dInit();
	g2dTexture* tex= g2dTexLoad("pic.png",G2D_SWIZZLE);
	sth_stash* stash = sth_create(512, 512);

	/* load truetype font */
	int droid = sth_add_font(stash, "simfang.ttf");
	/* position of the text */
	float x = 10, y = 10;

	/* draw text during your OpenGL render loop */


	while(1)
	{


	g2dClear(BLACK);

	//texture reandering
	g2dBeginRects(tex);

	g2dSetCoordXY(0,100);
	g2dSetScaleWH(25,38);
	g2dSetCropXY(0,0);
	g2dSetCropWH(25,38);
	g2dAdd();
	g2dEnd();


	//font rendering
	/* position: (x, y); font size: 24 */
	sth_draw_text(stash, droid, 24.0f, 10, 0, "ACDEFGHIJKLMN");

			
	g2dFlip(G2D_VSYNC);		
	}

	g2dTerm();
	return 0;

}
