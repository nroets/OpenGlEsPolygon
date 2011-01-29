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
#include <jni.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
#include <assert.h>
#include <string.h>

using namespace std;

struct GdkPoint {
	short x, y;
};
typedef int calcType; // Used to prevent overflows when
                      // multiplying two GdkPoint fields

struct PolygonEdge {
  int isLeft : 1;
  int delta : 2; // Either -1 or 1
  int continues : 1; // It's a polygon stored in an array and this edge wraps
                     // over. We should continue at this+1 when cnt runs out.
  int cnt : 20;
  GdkPoint *pt, prev;
  PolygonEdge *opp;
/* I tried to make PolygonEdge a node in a tree by using a set<>. It failed
   and I'm not sure how to fix it (add an iterator here ?).
  set<PolygonEdge *, edgeCmp>::iterator itr;
  
   So instead I copied and pasted my own AA tree code here:
*/
  PolygonEdge *parent, *left, *right;
  int level;
};

bool edgeCmp /*::operator()*/(const PolygonEdge *a, const PolygonEdge *b)
{
  const GdkPoint *ap = a->pt, *bp = b->pt,
    *st = a->prev.y < b->prev.y ? &a->prev : &b->prev;
  int cross;
  if (a->pt == &a->prev) return a->prev.x < 0; // left and right borders
  if (b->pt == &b->prev) return b->prev.x > 0;
  while ((cross = (ap->x - st->x) * (int)(bp->y - st->y) -
                  (ap->y - st->y) * (int)(bp->x - st->x)) == 0) {
    if (ap->y < bp->y) {
      st = ap;
      ap += a->delta;
      if (ap - a->pt >= a->cnt || ap - a->pt + a->cnt <= 0) {
        if (!a->continues) return 0; // Edges are the same
        a++;
        ap = a->pt;
      }
    }
    else {
      st = bp;
      bp += a->delta;
      if (bp - b->pt >= b->cnt || bp - b->pt + b->cnt <= 0) {
        if (!b->continues) return 0; // Edges are the same
        b++;
        bp = b->pt;
      }
    }
  }
  return cross < 0;
}

//-------------------[ Start of AA tree code ]---------------------------
struct PolygonEdge *First (struct PolygonEdge *root)
{
  if (!root->left) return NULL;
  while (root->left) root = root->left;
  return root;
}

struct PolygonEdge *Next (struct PolygonEdge *n)
{
  if (n->right) {
    n = n->right;
    while (n->left) n = n->left;
  }
  else {
    while (n->parent && n->parent->right == n) n = n->parent;
    // Follow the parent link while it's pointing to the left.
    n = n->parent; // Follow one parent link pointing to the right

    if (!n->parent) return NULL;
    // The last PolygonEdge is the root, because it has nothing to it's right
  }
  return n;
}

struct PolygonEdge *Prev (struct PolygonEdge *n)
{
  if (n->left) {
    n = n->left;
    while (n->right) n = n->right;
  }
  else {
    while (n->parent && n->parent->left == n) n = n->parent;
    // Follow the parent link while it's pointing to the left.
    n = n->parent; // Follow one parent link pointing to the right

    if (!n->parent) return NULL;
    // The last PolygonEdge is the root, because it has nothing to it's right
  }
  return n;
}

void Skew (struct PolygonEdge *oldparent)
{
  struct PolygonEdge *newp = oldparent->left;
  
  if (oldparent->parent->left == oldparent) oldparent->parent->left = newp;
  else oldparent->parent->right = newp;
  newp->parent = oldparent->parent;
  oldparent->parent = newp;
  
  oldparent->left = newp->right;
  if (oldparent->left) oldparent->left->parent = oldparent;
  newp->right = oldparent;
  
  oldparent->level = oldparent->left ? oldparent->left->level + 1 : 1;
}

int Split (struct PolygonEdge *oldparent)
{
  struct PolygonEdge *newp = oldparent->right;
  
  if (newp && newp->right && newp->right->level == oldparent->level) { 
    if (oldparent->parent->left == oldparent) oldparent->parent->left = newp;
    else oldparent->parent->right = newp;
    newp->parent = oldparent->parent;
    oldparent->parent = newp;
    
    oldparent->right = newp->left;
    if (oldparent->right) oldparent->right->parent = oldparent;
    newp->left = oldparent;
    newp->level = oldparent->level + 1;
    return 1;
  }
  return 0;
}

static struct PolygonEdge root;
// A static variable to make things easy.

void RebalanceAfterLeafAdd (struct PolygonEdge *n)
{ // n is a PolygonEdge that has just been inserted and is now a leaf PolygonEdge.
  n->level = 1;
  n->left = NULL;
  n->right = NULL;
  for (n = n->parent; n != &root; n = n->parent) {
    // At this point n->parent->level == n->level
    if (n->level != (n->left ? n->left->level + 1 : 1)) {
      // At this point the tree is correct, except (AA2) for n->parent
      Skew (n);
      // We handle it (a left add) by changing it into a right add using Skew
      // If the original add was to the left side of a PolygonEdge that is on the
      // right side of a horisontal link, n now points to the rights side
      // of the second horisontal link, which is correct.
      
      // However if the original add was to the left of PolygonEdge with a horisontal
      // link, we must get to the right side of the second link.
      if (!n->right || n->level != n->right->level) n = n->parent;
    }
    if (!Split (n->parent)) break;
  }
}

void Delete (struct PolygonEdge *n)
{ // If n is not a leaf, we first swap it out with the leaf PolygonEdge that just
  // precedes it.
  struct PolygonEdge *leaf = n, *tmp;
  
  if (n->left) {
    for (leaf = n->left; leaf->right; leaf = leaf->right) {}
    // When we stop, left has no 'right' child so it cannot have a left one
  }
  else if (n->right) leaf = n->right;

  tmp = leaf->parent == n ? leaf : leaf->parent;
  if (leaf->parent->left == leaf) leaf->parent->left = NULL;
  else leaf->parent->right = NULL;
  
  if (n != leaf) {
    if (n->parent->left == n) n->parent->left = leaf;
    else n->parent->right = leaf;
    leaf->parent = n->parent;
    if (n->left) n->left->parent = leaf;
    leaf->left = n->left;
    if (n->right) n->right->parent = leaf;
    leaf->right = n->right;
    leaf->level = n->level;
  }
  // free (n);

  while (tmp != &root) {
    // One of tmp's childern had it's level reduced
    if (tmp->level > (tmp->left ? tmp->left->level + 1 : 1)) { // AA2 failed
      tmp->level--;
      if (Split (tmp)) {
        if (Split (tmp)) Skew (tmp->parent->parent);
        break;
      }
      tmp = tmp->parent;
    }
    else if (tmp->level <= (tmp->right ? tmp->right->level + 1 : 1)) break;
    else { // AA3 failed
      Skew (tmp);
      //if (tmp->right) tmp->right->level = tmp->right->left ? tmp->right->left->level + 1 : 1;
      if (tmp->level > tmp->parent->level) {
        Skew (tmp);
        Split (tmp->parent->parent);
        break;
      }
      tmp = tmp->parent->parent;
    }
  }
}

void Check (struct PolygonEdge *n)
{
  assert (!n->left || n->left->parent == n);
  assert (!n->right || n->right->parent == n);
//  assert (!Next (n) || n->data <= Next (n)->data);
  assert (!n->parent || n->parent->level >= n->level);
  assert (n->level == (n->left == NULL ? 1 : n->left->level + 1));
  assert (n->level <= 1 || n->right && n->level - n->right->level <= 1);
  assert (!n->parent || !n->parent->parent ||
          n->parent->parent->level > n->level);
}

void Add (struct PolygonEdge *n)
{
  struct PolygonEdge *s = &root;
  int lessThan = 1;
  while (lessThan ? s->left : s->right) {
    s = lessThan ? s->left : s->right;
    lessThan = edgeCmp(n, s); //c.operator<(n, s);
  }
  if (lessThan) s->left = n;
  else s->right = n;
  n->parent = s;
  RebalanceAfterLeafAdd (n);
}
//----------------------------[ End of AA tree ]----------------------------

struct PolygonEdges { // Emulate STL vector on Android.
	PolygonEdge *e;
	int size, alloced;
	PolygonEdge &operator [](int i) { return e[i]; }
	void resize (int s) {
		if (alloced < s) { // Only grow.
			alloced = s + s / 10;
			e = (PolygonEdge*) realloc (e, alloced * sizeof (*e));
		}
		size = s;
	}
	~PolygonEdges () { free (e); }
};

void AddPolygon (PolygonEdges &d, GdkPoint *p, int cnt)
{
  int i, j, k, firstd = d.size;
  for (j = cnt - 1; j > 0 && (p[j - 1].y == p[j].y || // p[j..cnt-1] becomes
    (p[j - 1].y < p[j].y) == (p[j].y < p[0].y)); j--) {} // monotone
  for (i = 0; i < j && (p[i].y == p[i + 1].y ||
    (p[i].y < p[i + 1].y) == (p[j].y < p[0].y)); i++) {}
  // Now p[cn-1],p[0..i] is the longest monotone sequence
  d.resize (firstd + 2);
  d[firstd].delta = p[j].y < p[0].y ? 1 : -1;
  d[firstd].continues = 1;
  d[firstd + 1].delta = p[j].y < p[0].y ? 1 : -1;
  d[firstd + 1].continues = 0;
  d[firstd + !(p[j].y < p[0].y)].cnt = cnt - j;
  d[firstd + (p[j].y < p[0].y)].cnt = i + 1;
  d[firstd + !(p[j].y < p[0].y)].pt = p + (p[j].y < p[0].y ? j : cnt - 1);
  d[firstd + (p[j].y < p[0].y)].pt = p + (p[j].y < p[0].y ? 0 : i);
  int lowest = j;
  for (; i < j ; i = k) {
    if (p[lowest].y > p[i].y) lowest = i;
    for (k = i + 1; k < j && (p[k].y == p[k + 1].y || // p[i..k] becomes
        (p[k].y < p[k + 1].y) == (p[i].y < p[k].y)); k++) {} // monotone
    d.resize (d.size + 1);
    d[d.size - 1].pt = p + (p[i].y < p[k].y ? i : k);
    d[d.size - 1].delta = p[i].y < p[k].y ? 1 : -1;
    d[d.size - 1].continues = 0;
    d[d.size - 1].cnt = k - i + 1;
  }
  GdkPoint *ll = &p[(lowest + cnt - 1) % cnt], *lr = &p[(lowest + 1) % cnt];
  for (i = firstd; i < d.size; i++) {
    // This ll/lr inequality is a cross product that is true if the polygon
    // was clockwise. AddInnerPoly() will just negate isLeft.
    d[i].isLeft = ((ll->x - p[lowest].x) * int (lr->y - p[lowest].y) >
      int (ll->y - p[lowest].y) * (lr->x - p[lowest].x)) == (d[i].delta == 1);
  }
}

void Fill (PolygonEdges &d)
{
  PolygonEdge **heap = (PolygonEdge **) malloc ((d.size + 1) * sizeof (*heap));
  // leave heap[0] open to make indexing easier
  int i, h = 1, j;
  PolygonEdge dummy, left, right;
  
  root.left = NULL;
  root.right = NULL;
  root.level = 1000000;
  root.parent = NULL;
  
  left.prev.x = -30000;
  left.prev.y = 0;
  left.opp = &dummy;
  left.pt = &left.prev;
  left.isLeft = 1;
  Add (&left);
  right.prev.x = 30000;
  right.prev.y = 0;
  right.opp = &dummy;
  right.pt = &right.prev;
  right.isLeft = 0;
  Add (&right);
/*  for (i = 0; i < d.size(); i++) {
    for (j = 0; j + 1 < d[i].cnt; j++) assert (d[i].pt[j*d[i].delta].y <= d[i].pt[(j+1)*d[i].delta].y);
    if (d[i].continues)                assert (d[i].pt[j*d[i].delta].y <= d[i+1].pt->y);
  } // Make sure AddPolygon() and variants are correct */
  for (i = 0; i < d.size; i++) {
    for (j = h++; j > 1 && heap[j / 2]->pt->y > d[i].pt->y; j /= 2) {
      heap[j] = heap[j/2];
    }
    heap[j] = &d[i];
    d[i].opp = NULL;
    if (d[i].continues) ++i; // Don't add the second part to the heap
  }
  for (i = 2; i < h; i++) assert (heap[i]->pt->y >= heap[i/2]->pt->y);
  while (h > 1) {
    PolygonEdge *head = heap[1];
    //printf ("%p %3d\n", head->opp, head->pt->y);
    if (!head->opp) { // Insert it
      memcpy (&head->prev, head->pt, sizeof (head->prev)); // This is only
      // to make the compare work.
      Add (head);
      head->opp = head->isLeft ? Next (head) : Prev (head);
      if (head->opp == NULL || head->opp->isLeft == head->isLeft) {
        head->opp = &dummy;
      }
    }
    PolygonEdge *o = head->opp;
    /* Now we render the trapezium between head->opp and head->opp->opp up
       to head->pt->y */
    if (o != &dummy && o->opp != &dummy) {
      GdkPoint q[6];
      q[0].y = head->pt->y;
      q[0].x = o->pt->y <= o->prev.y ? o->pt->x
        : o->prev.x + (o->pt->x - o->prev.x) *
            int (q[0].y - o->prev.y) / (o->pt->y - o->prev.y);
      q[1].y = q[0].y;
      q[1].x = o->opp->pt->y <= o->opp->prev.y ? o->opp->pt->x
        : o->opp->prev.x + (o->opp->pt->x - o->opp->prev.x) *
            int (q[0].y - o->opp->prev.y) / (o->opp->pt->y - o->opp->prev.y);
      memcpy (&q[2], &o->opp->prev, sizeof (q[2]));
      memcpy (&q[3], &o->prev, sizeof (q[3]));
      memcpy (&q[4], &q[0], sizeof (q[4]));
      memcpy (&q[5], &q[2], sizeof (q[5]));
      // Frequently it happens that one of the triangles has 0 area because
      // two of the points are equal. TODO: Filter them out.
      glVertexPointer (2, GL_SHORT, 0, q);
      glDrawArrays (GL_TRIANGLES, 0, 6);
      memcpy (&o->prev, &q[0], sizeof (o->prev));
      memcpy (&o->opp->prev, &q[1], sizeof (o->opp->prev));
    }
    //else if (o != &dummy) memcpy (&o->prev, o->pt, sizeof (o->prev));
    if (o->opp != head) {
      o->opp->opp = &dummy; // Complete the 'Add'
      o->opp = head;
    }

    if (--head->cnt) head->pt += head->delta;
    else if (head->continues) {
      head->continues = head[1].continues;
      head->cnt = head[1].cnt;
      head->pt = head[1].pt;
    }
    else { // Remove it
      head->opp->opp = &dummy;
      PolygonEdge *n = head->isLeft ? Prev (head) : Next (head);
      if (n && n->opp == &dummy) {
        n->opp = head->opp;
        head->opp->opp = n;
      }
      Delete (head);
      head = heap[--h];
    }
    
    for (j = 2; j < h; j *= 2) {
      if (j + 1 < h && heap[j + 1]->pt->y < heap[j]->pt->y) j++;
      if (heap[j]->pt->y >= head->pt->y) break;
      heap[j / 2] = heap[j];
    }
    heap[j / 2] = head;
    for (i = 2; i < h; i++) assert (heap[i]->pt->y >= heap[i/2]->pt->y);
  } // While going through the edges
  free (heap);
}

extern "C" {
void
Java_za_co_rational_OpenGlEsPolygon_OpenGlEsPolygonRenderer_nativeRender( JNIEnv*  env,jobject thiz, jint width, jint height)
{
	glViewport(0,0,width,height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glOrthof(0.0f, width, 0.0f, height, 0.0f, 1.0f);

	glMatrixMode(GL_MODELVIEW);						// Really necessary ?
	glLoadIdentity();							// Really necessary ?
	glClear(GL_COLOR_BUFFER_BIT);

    glEnableClientState(GL_VERTEX_ARRAY);
	glColor4f (0.0, 0.0, 0.0, 1.0);

    GdkPoint pt[20];
    int ptCnt = 0;
	pt[ptCnt].x = 10;
	pt[ptCnt++].y = 50;
	pt[ptCnt].x = 20;
	pt[ptCnt++].y = 90;
	pt[ptCnt].x = 50;
	pt[ptCnt++].y = 50;
	pt[ptCnt].x = 30;
	pt[ptCnt++].y = 60;
	PolygonEdges d = { NULL, 0, 0 };
	AddPolygon (d, pt, ptCnt);
	Fill (d);
}
}
