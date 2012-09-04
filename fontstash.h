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

#ifndef FONTSTASH_H
#define FONTSTASH_H

struct g2dStash* g2dStashCreate(int cachew, int cacheh);


int g2dAddFont(struct g2dStash* stash, const char* path);
int g2dAddFontFromMemory(struct g2dStash* stash, unsigned char* buffer);

int g2dAddBitmapFont(struct g2dStash* stash, int ascent, int descent, int line_gap);
int g2dAddGlyph(struct g2dStash* stash, int idx, unsigned int id, const char* s,
                  short size, short base, int x, int y, int w, int h,
                  float xoffset, float yoffset, float xadvance);


void g2dDrawText(struct g2dStash* stash,
				   int idx, float size,
				   float x, float y, const char* string);

void g2dDimText(struct g2dStash* stash, int idx, float size, const char* string,
				  float* minx, float* miny, float* maxx, float* maxy);

void g2dVmetrics(struct g2dStash* stash,
				  int idx, float size,
				  float* ascender, float* descender, float * lineh);

void g2dStashDelete(struct g2dStash* stash);

#endif // FONTSTASH_H
