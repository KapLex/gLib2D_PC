//
// Copyright (c) 2011 Andreas Krinke andreas.krinke@gmx.de
// Copyright (c) 2009 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include <GL/glfw.h>
#include "glib2d.h"

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_malloc(x,u)    malloc(x)
#define STBTT_free(x,u)      free(x)
#include "stb_truetype.h"

#define HASH_LUT_SIZE 256
#define MAX_ROWS 128


#define TTFONT_FILE 1
#define TTFONT_MEM  2
#define BMFONT      3

static int idx = 1;

static unsigned int hashint(unsigned int a)
{
	a += ~(a<<15);
	a ^=  (a>>10);
	a +=  (a<<3);
	a ^=  (a>>6);
	a += ~(a<<11);
	a ^=  (a>>16);
	return a;
}


typedef struct
{
	float x0,y0,s0,t0;
	float x1,y1,s1,t1;
}Quad;

typedef struct
{
	short x,y,h;
}Row;

typedef struct TextureAtlas
{
	// TODO: replace rows with pointer
	Row rows[MAX_ROWS];
	int nrows;
	g2dTexture * tex;
	TextureAtlas* next;
}TextureAtlas;

typedef struct Glyph
{
	unsigned int codepoint;
	short size;
	TextureAtlas* texture;
	int x0,y0,x1,y1;
	float xadv,xoff,yoff;
	int next;
}Glyph;

typedef struct g2dFont
{
	int idx;
	int type;
	stbtt_fontinfo font;
	unsigned char* data;
	Glyph* glyphs;
	int lut[HASH_LUT_SIZE];
	int nglyphs;
	float ascender;
	float descender;
	float lineh;
	g2dFont* next;
}g2dFont;



typedef struct
{
	int tw,th;
	float itw,ith;
	TextureAtlas* textures;
	g2dFont* fonts;
	int drawing;
}g2dStash;



// Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.

#define UTF8_ACCEPT 0
#define UTF8_REJECT 1

static const unsigned char utf8d[] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 00..1f
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 20..3f
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 40..5f
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 60..7f
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, // 80..9f
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, // a0..bf
	8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, // c0..df
	0xa,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x4,0x3,0x3, // e0..ef
	0xb,0x6,0x6,0x6,0x5,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8, // f0..ff
	0x0,0x1,0x2,0x3,0x5,0x8,0x7,0x1,0x1,0x1,0x4,0x6,0x1,0x1,0x1,0x1, // s0..s0
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,0,1,0,1,1,1,1,1,1, // s1..s2
	1,2,1,1,1,1,1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1, // s3..s4
	1,2,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,3,1,1,1,1,1,1, // s5..s6
	1,3,1,1,1,1,1,3,1,3,1,1,1,1,1,1,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // s7..s8
};

static unsigned int decutf8(unsigned int* state, unsigned int* codep, unsigned int byte)
{
	unsigned int type = utf8d[byte];
	*codep = (*state != UTF8_ACCEPT) ?
		(byte & 0x3fu) | (*codep << 6) :
		(0xff >> type) & (byte);
	*state = utf8d[256 + *state*16 + type];
	return *state;
}



g2dStash* g2dStashCreate(int cachew, int cacheh)
{
	g2dStash* stash = NULL;
	TextureAtlas* texture = NULL;

	// Allocate memory for the font stash.
	stash = (g2dStash*)malloc(sizeof(g2dStash));
	if (stash == NULL) goto error;
	memset(stash,0,sizeof(g2dStash));

	// Allocate memory for the first texture
	texture = (TextureAtlas*)malloc(sizeof(TextureAtlas));

	if (texture == NULL) goto error;
	memset(texture,0,sizeof(TextureAtlas));

	texture->tex = g2dTexCreate(cachew, cachew);

	// Create first texture for the cache.
	stash->tw = cachew;
	stash->th = cacheh;
	stash->itw = 1.0f/cachew;
	stash->ith = 1.0f/cacheh;
	stash->textures = texture;
	glGenTextures(1, &texture->tex->id);
	if (!texture->tex->id) goto error;
	glBindTexture(GL_TEXTURE_2D, texture->tex->id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, stash->tw,stash->th, 0, GL_ALPHA, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	return stash;
	
error:
	if (stash != NULL)
		free(stash);
	if (texture != NULL)
		free(texture);
	return NULL;
}

int g2dAddFontFromMemory(g2dStash* stash, unsigned char* buffer)
{
	int i, ascent, descent, fh, lineGap;
	g2dFont* fnt = NULL;

	fnt = (g2dFont*)malloc(sizeof(g2dFont));
	if (fnt == NULL) goto error;
	memset(fnt,0,sizeof(g2dFont));

	// Init hash lookup.
	for (i = 0; i < HASH_LUT_SIZE; ++i) fnt->lut[i] = -1;

	fnt->data = buffer;

	// Init stb_truetype
	if (!stbtt_InitFont(&fnt->font, fnt->data, 0)) goto error;
	
	// Store normalized line height. The real line height is got
	// by multiplying the lineh by font size.
	stbtt_GetFontVMetrics(&fnt->font, &ascent, &descent, &lineGap);
	fh = ascent - descent;
	fnt->ascender = (float)ascent / (float)fh;
	fnt->descender = (float)descent / (float)fh;
	fnt->lineh = (float)(fh + lineGap) / (float)fh;

	fnt->idx = idx;
	fnt->type = TTFONT_MEM;
	fnt->next = stash->fonts;
	stash->fonts = fnt;
	
	return idx++;

error:
	if (fnt) {
		if (fnt->glyphs) free(fnt->glyphs);
		free(fnt);
	}
	return 0;
}

int g2dAddFont(g2dStash* stash, const char* path)
{
	FILE* fp = 0;
	int datasize;
	unsigned char* data = NULL;
	int idx;
	
	// Read in the font data.
	fp = fopen(path, "rb");
	if (!fp) goto error;
	fseek(fp,0,SEEK_END);
	datasize = (int)ftell(fp);
	fseek(fp,0,SEEK_SET);
	data = (unsigned char*)malloc(datasize);
	if (data == NULL) goto error;
	fread(data, 1, datasize, fp);
	fclose(fp);
	fp = 0;
	
	idx = g2dAddFontFromMemory(stash, data);
	// Modify type of the loaded font.
	if (idx)
		stash->fonts->type = TTFONT_FILE;
	else
		free(data);

	return idx;
	
error:
	if (data) free(data);
	if (fp) fclose(fp);
	return 0;
}

int g2dAddBitmapFont(g2dStash* stash, int ascent, int descent, int line_gap)
{
	int i, fh;
	g2dFont* fnt = NULL;

	fnt = (g2dFont*)malloc(sizeof(g2dFont));
	if (fnt == NULL) goto error;
	memset(fnt,0,sizeof(g2dFont));

	// Init hash lookup.
	for (i = 0; i < HASH_LUT_SIZE; ++i) fnt->lut[i] = -1;

	// Store normalized line height. The real line height is got
	// by multiplying the lineh by font size.
	fh = ascent - descent;
	fnt->ascender = (float)ascent / (float)fh;
	fnt->descender = (float)descent / (float)fh;
	fnt->lineh = (float)(fh + line_gap) / (float)fh;

	fnt->idx = idx;
	fnt->type = BMFONT;
	fnt->next = stash->fonts;
	stash->fonts = fnt;
	
	return idx++;

error:
	if (fnt) free(fnt);
	return 0;
}

void g2dAddGlyph(g2dStash* stash,
                  int idx,
                  GLuint id,
                  const char* s,
                  short size, short base,
                  int x, int y, int w, int h,
                  float xoffset, float yoffset, float xadvance)
{
	TextureAtlas* texture = NULL;
	g2dFont* fnt = NULL;
	Glyph* glyph = NULL;
	unsigned int codepoint;
	unsigned int state = 0;

	if (stash == NULL) return;
	texture = stash->textures;
	while (texture != NULL && texture->tex->id != id) texture = texture->next;
	if (texture == NULL)
	{
		// Create new texture
		texture = (TextureAtlas*)malloc(sizeof(TextureAtlas));
		if (texture == NULL) return;
		memset(texture, 0, sizeof(TextureAtlas));
		texture->tex = g2dTexCreate(stash->tw,stash->th);
		texture->tex->id = id;
		texture->next = stash->textures;
		stash->textures = texture;
	}

	fnt = stash->fonts;
	while (fnt != NULL && fnt->idx != idx) fnt = fnt->next;
	if (fnt == NULL) return;
	if (fnt->type != BMFONT) return;
	
	for (; *s; ++s)
	{
		if (!decutf8(&state, &codepoint, *(unsigned char*)s)) break;
	}
	if (state != UTF8_ACCEPT) return;

	// Alloc space for new glyph.
	fnt->nglyphs++;
	fnt->glyphs = (Glyph *)realloc(fnt->glyphs, fnt->nglyphs*sizeof(Glyph));
	if (!fnt->glyphs) return;

	// Init glyph.
	glyph = &fnt->glyphs[fnt->nglyphs-1];
	memset(glyph, 0, sizeof(Glyph));
	glyph->codepoint = codepoint;
	glyph->size = size;
	glyph->texture = texture;
	glyph->x0 = x;
	glyph->y0 = y;
	glyph->x1 = glyph->x0+w;
	glyph->y1 = glyph->y0+h;
	glyph->xoff = xoffset;
	glyph->yoff = yoffset - base;
	glyph->xadv = xadvance;
	
	// Find code point and size.
	h = hashint(codepoint) & (HASH_LUT_SIZE-1);
	// Insert char to hash lookup.
	glyph->next = fnt->lut[h];
	fnt->lut[h] = fnt->nglyphs-1;
}

static Glyph* get_glyph(g2dStash* stash, g2dFont* fnt, unsigned int codepoint, short isize)
{
	int i,g,advance,lsb,x0,y0,x1,y1,gw,gh;
	float scale;
	TextureAtlas* texture = NULL;
	Glyph* glyph = NULL;
	unsigned char* bmp = NULL;
	unsigned int h;
	float size = isize/10.0f;
	int rh;
	Row* br = NULL;

	// Find code point and size.
	h = hashint(codepoint) & (HASH_LUT_SIZE-1);
	i = fnt->lut[h];
	while (i != -1)
	{
		if (fnt->glyphs[i].codepoint == codepoint && (fnt->type == BMFONT || fnt->glyphs[i].size == isize))
			return &fnt->glyphs[i];
		i = fnt->glyphs[i].next;
	}
	// Could not find glyph.
	
	// For bitmap fonts: ignore this glyph.
	if (fnt->type == BMFONT) return 0;
	
	// For truetype fonts: create this glyph.
	scale = stbtt_ScaleForPixelHeight(&fnt->font, size);
	g = stbtt_FindGlyphIndex(&fnt->font, codepoint);
	stbtt_GetGlyphHMetrics(&fnt->font, g, &advance, &lsb);
	stbtt_GetGlyphBitmapBox(&fnt->font, g, scale,scale, &x0,&y0,&x1,&y1);
	gw = x1-x0;
	gh = y1-y0;
	
    // Check if glyph is larger than maximum texture size
	if (gw >= stash->tw || gh >= stash->th)
		return 0;

	// Find texture and row where the glyph can be fit.
	br = NULL;
	rh = (gh+7) & ~7;
	texture = stash->textures;
	while(br == NULL)
	{
		for (i = 0; i < texture->nrows; ++i)
		{
			if (texture->rows[i].h == rh && texture->rows[i].x+gw+1 <= stash->tw)
				br = &texture->rows[i];
		}
	
		// If no row is found, there are 3 possibilities:
		//   - add new row
		//   - try next texture
		//   - create new texture
		if (br == NULL)
		{
			short py = 0;
			// Check that there is enough space.
			if (texture->nrows)
			{
				py = texture->rows[texture->nrows-1].y + texture->rows[texture->nrows-1].h+1;
				if (py+rh > stash->th)
				{
					if (texture->next != NULL)
					{
						texture = texture->next;
					}
					else
					{
						// Create new texture
						texture->next = (TextureAtlas*)malloc(sizeof(TextureAtlas));
						texture = texture->next;
						texture->tex = g2dTexCreate(stash->tw,stash->th);
						if (texture == NULL) goto error;
						memset(texture,0,sizeof(TextureAtlas));
						glGenTextures(1, &texture->tex->id);
						if (!texture->tex->id) goto error;
						glBindTexture(GL_TEXTURE_2D, texture->tex->id);
						glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, stash->tw,stash->th, 0, GL_ALPHA, GL_UNSIGNED_BYTE, 0);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					}
					continue;
				}
			}
			// Init and add row
			br = &texture->rows[texture->nrows];
			br->x = 0;
			br->y = py;
			br->h = rh;
			texture->nrows++;	
		}
	}
	
	// Alloc space for new glyph.
	fnt->nglyphs++;
	fnt->glyphs = (Glyph *)realloc(fnt->glyphs, fnt->nglyphs*sizeof(Glyph));
	if (!fnt->glyphs) return 0;

	// Init glyph.
	glyph = &fnt->glyphs[fnt->nglyphs-1];
	memset(glyph, 0, sizeof(Glyph));
	glyph->codepoint = codepoint;
	glyph->size = isize;
	glyph->texture = texture;
	glyph->x0 = br->x;
	glyph->y0 = br->y;
	glyph->x1 = glyph->x0+gw;
	glyph->y1 = glyph->y0+gh;
	glyph->xadv = scale * advance;
	glyph->xoff = (float)x0;
	glyph->yoff = (float)y0;
	glyph->next = 0;

	// Advance row location.
	br->x += gw+1;
	
	// Insert char to hash lookup.
	glyph->next = fnt->lut[h];
	fnt->lut[h] = fnt->nglyphs-1;

	// Rasterize
	bmp = (unsigned char*)malloc(gw*gh);
	if (bmp)
	{
		stbtt_MakeGlyphBitmap(&fnt->font, bmp, gw,gh,gw, scale,scale, g);
		// Update texture
		glBindTexture(GL_TEXTURE_2D, texture->tex->id);
		glPixelStorei(GL_UNPACK_ALIGNMENT,1);
		glTexSubImage2D(GL_TEXTURE_2D, 0, glyph->x0,glyph->y0, gw,gh, GL_ALPHA,GL_UNSIGNED_BYTE,bmp); 
		free(bmp);
	}
	
	return glyph;

error:
	if (texture)
		free(texture);
	return 0;
}

static int get_quad(g2dStash* stash, g2dFont* fnt, Glyph* glyph, short isize, float* x, float* y, Quad* q)
{
	int rx,ry;
	float scale = 1.0f;

	if (fnt->type == BMFONT) scale = isize/(glyph->size*10.0f);

	rx = floorf(*x + scale * glyph->xoff);
	ry = floorf(*y - scale * glyph->yoff);
	
	q->x0 = rx;
	q->y0 = ry;
	q->x1 = rx + scale * (glyph->x1 - glyph->x0);
	q->y1 = ry - scale * (glyph->y1 - glyph->y0);
	
	q->s0 = (glyph->x0) * stash->itw;
	q->t0 = (glyph->y0) * stash->ith;
	q->s1 = (glyph->x1) * stash->itw;
	q->t1 = (glyph->y1) * stash->ith;
	
	*x += scale * glyph->xadv;
	
	return 1;
}




void g2dDrawText(g2dStash* stash,
				   int idx, float size,
				   float x, float y,
				   const char* s)
{
	unsigned int codepoint;
	Glyph* glyph = NULL;
	TextureAtlas* texture = NULL;
	unsigned int state = 0;
	short isize = (short)(size*10.0f);
	g2dFont* fnt = NULL;
	
	float scale = 1.0;

	if (stash == NULL) return;

	if (!stash->textures) return;
	fnt = stash->fonts;
	while(fnt != NULL && fnt->idx != idx) fnt = fnt->next;
	if (fnt == NULL) return;
	if (fnt->type != BMFONT && !fnt->data) return;
	

	for (; *s; ++s)
	{
		if (decutf8(&state, &codepoint, *(unsigned char*)s)) continue;
		glyph = get_glyph(stash, fnt, codepoint, isize);
		if (!glyph) continue;
		texture = glyph->texture;


		g2dBeginRects(glyph->texture->tex);
		g2dSetCoordXY(x,y);
		g2dSetCropXY(glyph->x0,glyph->y0);
		g2dSetCropWH(glyph->x1-glyph->x0,glyph->y1-glyph->y0);
		g2dSetScaleWH(size,size);
		g2dAdd();
		g2dEnd();

		if (fnt->type == BMFONT) scale = isize/(glyph->size*10.0f);
		x += scale * glyph->xadv;
	}

}

void g2dDimText(g2dStash* stash,
				  int idx, float size,
				  const char* s,
				  float* minx, float* miny, float* maxx, float* maxy)
{
	unsigned int codepoint;
	Glyph* glyph = NULL;
	unsigned int state = 0;
	Quad q;
	short isize = (short)(size*10.0f);
	g2dFont* fnt = NULL;
	float x = 0, y = 0;
	
	if (stash == NULL) return;
	if (!stash->textures || !stash->textures->tex->id) return;
	fnt = stash->fonts;
	while(fnt != NULL && fnt->idx != idx) fnt = fnt->next;
	if (fnt == NULL) return;
	if (fnt->type != BMFONT && !fnt->data) return;
	
	*minx = *maxx = x;
	*miny = *maxy = y;

	for (; *s; ++s)
	{
		if (decutf8(&state, &codepoint, *(unsigned char*)s)) continue;
		glyph = get_glyph(stash, fnt, codepoint, isize);
		if (!glyph) continue;
		if (!get_quad(stash, fnt, glyph, isize, &x, &y, &q)) continue;
		if (q.x0 < *minx) *minx = q.x0;
		if (q.x1 > *maxx) *maxx = q.x1;
		if (q.y1 < *miny) *miny = q.y1;
		if (q.y0 > *maxy) *maxy = q.y0;
	}
}

void g2dVmetrics(g2dStash* stash,
				  int idx, float size,
				  float* ascender, float* descender, float* lineh)
{
	g2dFont* fnt = NULL;

	if (stash == NULL) return;
	if (!stash->textures || !stash->textures->tex->id) return;
	fnt = stash->fonts;
	while(fnt != NULL && fnt->idx != idx) fnt = fnt->next;
	if (fnt == NULL) return;
	if (fnt->type != BMFONT && !fnt->data) return;
	if (ascender)
		*ascender = fnt->ascender*size;
	if (descender)
		*descender = fnt->descender*size;
	if (lineh)
		*lineh = fnt->lineh*size;
}

void g2dStashDelete(g2dStash* stash)
{
	TextureAtlas* texture = NULL;
	TextureAtlas* curtex = NULL;
	g2dFont* fnt = NULL;
	g2dFont* curfnt = NULL;

	if (!stash) return;

	texture = stash->textures;
	while(texture != NULL) {
		curtex = texture;
		texture = texture->next;
		if (curtex->tex->id)
			glDeleteTextures(1, &curtex->tex->id);
		free(curtex);
	}

	fnt = stash->fonts;
	while(fnt != NULL) {
		curfnt = fnt;
		fnt = fnt->next;
		if (curfnt->glyphs)
			free(curfnt->glyphs);
		if (curfnt->type == TTFONT_FILE && curfnt->data)
			free(curfnt->data);
		free(curfnt);
	}
	free(stash);
}
