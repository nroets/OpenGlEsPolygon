/* Copyright (C) 2011 Nic Roets. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are
permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this list of
      conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice, this list
      of conditions and the following disclaimer in the documentation and/or other materials
      provided with the distribution.

THIS SOFTWARE IS PROVIDED BY Nic Roets ``AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those of the
authors and should not be interpreted as representing official policies, either expressed
or implied, of Nic Roets.
*/

/* This is a polygon triangulation engine for OpenGL ES. In order to keep the
datastructures and algorithms simple, it uses 2 triangles per node instead
of the optimal 1. Because of the simplicity, I was able to add a "sea" mode,
where the coastline is supplied as linestrings.

This is however not a mature product.
1. Non-simple (self intersecting) polygons
may trigger the "sea" mode, so "sea" mode is currently a parameter of Fill()
2. If the same segment appears more than once, it may occasionally get
confused between "land" and "sea". To reduce the chance of this happening,
coordinates are stored as 16 + 16 bit fixed numbers.
3. Only a function to add outer polygons is supplied. The function to add
inner polygons are left to the reader.
4. The tree structure is not being rebalanced for fear that the code is buggy.
This will cause slightly degraded performance.
5. An extra GL_LINE was added to work around a bug on the Vodaphone 845.
6. The Android test application may not work. The engine is however running
quite well as part of Gosmore.
*/
#ifndef OPENGLESPOLYGON_H
#define OPENGLESPOLYGON_H

#include <vector>

struct FixedPoint {
  int x, y;
};

struct PolygonEdge {
  int isLeft : 1;
  int delta : 2; // Either -1 or 1
  int continues : 1; // It's a polygon stored in an array and this edge wraps
                     // over. We should continue at this+1 when cnt runs out.
  int cnt : 20;
  FixedPoint *pt, prev;
  PolygonEdge *opp;
/* I tried to make PolygonEdge a node in a tree by using a set<>. It failed
   and I'm not sure how to fix it (add an iterator here ?).
  set<PolygonEdge *, edgeCmp>::iterator itr;

   So instead I copied and pasted my own AA tree code here:
*/
  PolygonEdge *parent, *left, *right;
  int level;
};

#ifdef ANDROID_NDK
struct GdkPoint {
  short x, y;
};

void Fill (std::vector<PolygonEdge> &d,int hasSea);
#else
void Fill (std::vector<PolygonEdge> &d,int hasSea, GdkWindow *w, GdkGC *gc);
#endif
void AddPolygon (std::vector<PolygonEdge> &d, FixedPoint *p, int cnt);
void AddClockwise (std::vector<PolygonEdge> &d, FixedPoint *p, int cnt);

#endif
