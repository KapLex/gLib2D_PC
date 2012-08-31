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
}sth_quad;

typedef struct
{
	short x,y,h;
}sth_row;

typedef struct sth_texture
{
	// TODO: replace rows with pointer
	sth_row rows[MAX_ROWS];
	int nrows;
	g2dTexture * tex;
	sth_texture* next;
}sth_texture;

typedef struct sth_glyph
{
	unsigned int codepoint;
	short size;
	sth_texture* texture;
	int x0,y0,x1,y1;
	float xadv,xoff,yoff;
	int next;
}sth_glyph;

typedef struct sth_font
{
	int idx;
	int type;
	stbtt_fontinfo font;
	unsigned char* data;
	sth_glyph* glyphs;
	int lut[HASH_LUT_SIZE];
	int nglyphs;
	float ascender;
	float descender;
	float lineh;
	sth_font* next;
}sth_font;



typedef struct
{
	int tw,th;
	float itw,ith;
	sth_texture* textures;
	sth_font* fonts;
	int drawing;
}sth_stash;



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



sth_stash* sth_create(int cachew, int cacheh)
{
	sth_stash* stash = NULL;
	sth_texture* texture = NULL;

	// Allocate memory for the font stash.
	stash = (sth_stash*)malloc(sizeof(sth_stash));
	if (stash == NULL) goto error;
	memset(stash,0,sizeof(sth_stash));

	// Allocate memory for the first texture
	texture = (sth_texture*)malloc(sizeof(sth_texture));

	if (texture == NULL) goto error;
	memset(texture,0,sizeof(sth_texture));

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

int sth_add_font_from_memory(sth_stash* stash, unsigned char* buffer)
{
	int i, ascent, descent, fh, lineGap;
	sth_font* fnt = NULL;

	fnt = (sth_font*)malloc(sizeof(sth_font));
	if (fnt == NULL) goto error;
	memset(fnt,0,sizeof(sth_font));

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

int sth_add_font(sth_stash* stash, const char* path)
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
	
	idx = sth_add_font_from_memory(stash, data);
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

int sth_add_bitmap_font(sth_stash* stash, int ascent, int descent, int line_gap)
{
	int i, fh;
	sth_font* fnt = NULL;

	fnt = (sth_font*)malloc(sizeof(sth_font));
	if (fnt == NULL) goto error;
	memset(fnt,0,sizeof(sth_font));

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

void sth_add_glyph(sth_stash* stash,
                  int idx,
                  GLuint id,
                  const char* s,
                  short size, short base,
                  int x, int y, int w, int h,
                  float xoffset, float yoffset, float xadvance)
{
	sth_texture* texture = NULL;
	sth_font* fnt = NULL;
	sth_glyph* glyph = NULL;
	unsigned int codepoint;
	unsigned int state = 0;

	if (stash == NULL) return;
	texture = stash->textures;
	while (texture != NULL && texture->tex->id != id) texture = texture->next;
	if (texture == NULL)
	{
		// Create new texture
		texture = (sth_texture*)malloc(sizeof(sth_texture));
		if (texture == NULL) return;
		memset(texture, 0, sizeof(sth_texture));
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
	fnt->glyphs = (sth_glyph *)realloc(fnt->glyphs, fnt->nglyphs*sizeof(sth_glyph));
	if (!fnt->glyphs) return;

	// Init glyph.
	glyph = &fnt->glyphs[fnt->nglyphs-1];
	memset(glyph, 0, sizeof(sth_glyph));
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

static sth_glyph* get_glyph(sth_stash* stash, sth_font* fnt, unsigned int codepoint, short isize)
{
	int i,g,advance,lsb,x0,y0,x1,y1,gw,gh;
	float scale;
	sth_texture* texture = NULL;
	sth_glyph* glyph = NULL;
	unsigned char* bmp = NULL;
	unsigned int h;
	float size = isize/10.0f;
	int rh;
	sth_row* br = NULL;

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
						texture->next = (sth_texture*)malloc(sizeof(sth_texture));
						texture = texture->next;
						texture->tex = g2dTexCreate(stash->tw,stash->th);
						if (texture == NULL) goto error;
						memset(texture,0,sizeof(sth_texture));
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
	fnt->glyphs = (sth_glyph *)realloc(fnt->glyphs, fnt->nglyphs*sizeof(sth_glyph));
	if (!fnt->glyphs) return 0;

	// Init glyph.
	glyph = &fnt->glyphs[fnt->nglyphs-1];
	memset(glyph, 0, sizeof(sth_glyph));
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

static int get_quad(sth_stash* stash, sth_font* fnt, sth_glyph* glyph, short isize, float* x, float* y, sth_quad* q)
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




void sth_draw_text(sth_stash* stash,
				   int idx, float size,
				   float x, float y,
				   const char* s)
{
	unsigned int codepoint;
	sth_glyph* glyph = NULL;
	sth_texture* texture = NULL;
	unsigned int state = 0;
	short isize = (short)(size*10.0f);
	sth_font* fnt = NULL;
	
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

void sth_dim_text(sth_stash* stash,
				  int idx, float size,
				  const char* s,
				  float* minx, float* miny, float* maxx, float* maxy)
{
	unsigned int codepoint;
	sth_glyph* glyph = NULL;
	unsigned int state = 0;
	sth_quad q;
	short isize = (short)(size*10.0f);
	sth_font* fnt = NULL;
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

void sth_vmetrics(sth_stash* stash,
				  int idx, float size,
				  float* ascender, float* descender, float* lineh)
{
	sth_font* fnt = NULL;

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

void sth_delete(sth_stash* stash)
{
	sth_texture* texture = NULL;
	sth_texture* curtex = NULL;
	sth_font* fnt = NULL;
	sth_font* curfnt = NULL;

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
