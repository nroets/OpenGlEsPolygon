#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <assert.h>

#include <algorithm>
#include <vector>
#include <set>
#include <stdio.h>
#include <string.h>

using namespace std;

struct edgeCmp
{
  bool operator()(const struct PolygonEdge *a, const struct PolygonEdge *b);
};

struct PolygonEdge {
//  PolygonEdge () {}
  int isLeft : 1;
  int delta : 2; // Either -1 or 1
  int continues : 1; // It's a polygon stored in an array and this edge wraps
                     // over. We should continue at this+1 when cnt runs out.
  int cnt : 20;
  //PolygonEdge **heap;
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
//    printf ("%3d,%3d   %3d,%3d   %3d,%3d\n", ap->x, ap->y, st->x, st->y,
//      bp->x, bp->y);
//  printf ("cross = %d\n", cross < 0);
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

void AddPolygon (vector<PolygonEdge> &d, GdkPoint *p, int cnt)
{
  int i, j, k, firstd = d.size ();
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
    d.push_back (PolygonEdge ());
    d.back().pt = p + (p[i].y < p[k].y ? i : k);
    d.back().delta = p[i].y < p[k].y ? 1 : -1;
    d.back().continues = 0;
    d.back().cnt = k - i + 1;
  }
  GdkPoint *ll = &p[(lowest + cnt - 1) % cnt], *lr = &p[(lowest + 1) % cnt];
  for (i = firstd; i < d.size (); i++) {
    // This ll/lr inequality is a cross product that is true if the polygon
    // was clockwise.
    d[i].isLeft = ((ll->x - p[lowest].x) * int (lr->y - p[lowest].y) >
      int (ll->y - p[lowest].y) * (lr->x - p[lowest].x)) == (d[i].delta == 1);
  }
}

void Fill (GdkWindow *w, GdkGC *gc, vector<PolygonEdge> &d)
{
  vector<PolygonEdge*> heap;
  heap.resize (d.size () + 1); // leave heap[0] open to make indexing easier
  int i, h = 1, j;
  //set<PolygonEdge *, edgeCmp> active;
  //typedef set<PolygonEdge *, edgeCmp>::iterator itrType;
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
  Add (&left); //active.insert (&left);
  right.prev.x = 30000;
  right.prev.y = 0;
  right.opp = &dummy;
  right.pt = &right.prev;
  right.isLeft = 0;
/*  rightTop.x = 30000;
  rightTop.y = 0;
  right.pt = &rightTop;*/
  Add (&right); //active.insert (&right);
  for (i = 0; i < d.size(); i++) {
    for (j = 0; j + 1 < d[i].cnt; j++) assert (d[i].pt[j*d[i].delta].y <= d[i].pt[(j+1)*d[i].delta].y);
    if (d[i].continues)                assert (d[i].pt[j*d[i].delta].y <= d[i+1].pt->y);
  }
  for (i = 0; i < d.size(); i++) {
    for (j = h++; j > 1 && heap[j / 2]->pt->y > d[i].pt->y; j /= 2) {
      //heap[j/2]->heap = &heap[j];
      heap[j] = heap[j/2];
    }
    //d[i].heap = &heap[j];
    heap[j] = &d[i];
    d[i].opp = NULL;
    if (d[i].continues) ++i; // Don't add the second part to the heap
  }
  for (i = 2; i < h; i++) assert (heap[i]->pt->y >= heap[i/2]->pt->y);
  while (h > 1) {
    PolygonEdge *head = heap[1];
    printf ("%p %3d\n", head->opp, head->pt->y);
    if (!head->opp) { // Insert it
      memcpy (&head->prev, head->pt, sizeof (head->prev)); // This is only
      // to make the compare work.
      /*itrType itr = active.insert (head).first;
      head->itr = itr;
      if (head->isLeft) itr++;
      else itr--;
      head->opp = itr == active.end () || //itr == active.rend () ||
        (*itr)->isLeft == head->isLeft ? &dummy : *itr; */
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
      GdkPoint q[4];
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
      gdk_draw_polygon (w, gc, TRUE, q, 4);
      memcpy (&o->prev, &q[0], sizeof (o->prev));
      memcpy (&o->opp->prev, &q[1], sizeof (o->opp->prev));
    }
    //else if (o != &dummy) memcpy (&o->prev, o->pt, sizeof (o->prev));
    if (o->opp != head) {
      o->opp->opp = &dummy;
      o->opp = head;
    }

    if (--head->cnt) head->pt += head->delta;
    else if (head->continues) {
      head->continues = head[1].continues;
      head->cnt = head[1].cnt;
      head->pt = head[1].pt;
    }
    else { // Remove it
      #if 0
      itrType n = head->itr;
      //#if 0
      if (head->isLeft) n--;
      else n++;
      if (n != active.end() /*&& n != active.rend()*/ && (*n)->opp == &dummy) {
        n->opp = head->opp;
        head->opp->opp = n;
      }
      active.erase (head->itr);
      #endif
      head->opp->opp = &dummy;
      PolygonEdge *n = head->isLeft ? Prev (head) : Next (head);
      if (n && n->opp == &dummy) {
        n->opp = head->opp;
        head->opp->opp = n;
      }
      Delete (head);
      //#endif
      //assert (itr != active.end());
      head = heap[--h];
    }
    
    for (j = 2; j < h; j *= 2) {
      if (j + 1 < h && heap[j + 1]->pt->y < heap[j]->pt->y) j++;
      if (heap[j]->pt->y >= head->pt->y) break;
      //heap[j]->heap = &heap[j / 2];
      heap[j / 2] = heap[j];
    }
    heap[j / 2] = head;
    for (i = 2; i < h; i++) assert (heap[i]->pt->y >= heap[i/2]->pt->y);
    //head->heap = &heap[j / 2];
  } // While going through the edges
}

//int l[2][4] = { 10, 50, 50, 10, 70, 30, 30, 70 };


/*struct polyPtr {
  int *p;
};

bool operator < (const polyPtr &a, const polyPtr &b)
{
  return a.p[0] < b.p[0];
}*/

GtkWidget *draw;
GdkColor c;

GdkPoint pt[1000];
int ptCnt = 0;

gint DrawExpose (void)
{
  GdkGC *mygc = gdk_gc_new (draw->window);
  gdk_gc_set_fill (mygc, GDK_SOLID);
  
//  seidel *root = NULL;
//  for (i = 0; i < sizeof (l) / sizeof (l[0]); i++) Add (&root, l[i]);
/*  int p[] = { 10, 10, 50, 20, 10, 50 };
  polyPtr ptr[sizeof (p) / sizeof (p[0]) / 2];
  for (int i = 0; i < sizeof (p) / sizeof (p[0]) / 2; i++) ptr[i].ptr = p + i * 2;
  sort (ptr, ptr + sizeof (p) / sizeof (p[0]) / 2);
  set<
  lower_bound 
*/
  
  c.blue = 0;
  c.red = 0;
/*  c.green = 0xffff - c.blue / 2 - c.red / 2;
  if (c.blue <= 0xF000) c.blue += 0xfff;
  else {
    c.blue = 0;
    c.red = (c.red + (int) 0xfff) % (0xffff + 0xfff);
  }*/
  if (ptCnt > 2) {
    c.green = 0;
    gdk_colormap_alloc_color (gdk_window_get_colormap (draw->window),
        &c, FALSE, TRUE);
    gdk_gc_set_foreground (mygc, &c);
    gdk_draw_polygon (draw->window, mygc, TRUE, &pt[0], ptCnt);
    
    vector<PolygonEdge> d;
    AddPolygon (d, pt, ptCnt);
    
    c.blue = 0xffff;
    gdk_colormap_alloc_color (gdk_window_get_colormap (draw->window),
      &c, FALSE, TRUE);
    gdk_gc_set_foreground (mygc, &c);
    Fill (draw->window, mygc, d);

    c.green = 0xffff;
    gdk_colormap_alloc_color (gdk_window_get_colormap (draw->window),
        &c, FALSE, TRUE);
    gdk_gc_set_foreground (mygc, &c);
    gdk_draw_polygon (draw->window, mygc, FALSE, &pt[0], ptCnt);
  }
}

int Click (GtkWidget * /*widget*/, GdkEventButton *event, void * /*para*/)
{
  pt[ptCnt].x = event->x;
  pt[ptCnt++].y = event->y;
  gtk_widget_queue_clear (draw); 
  return FALSE;
}

int main (int argc, char *argv[])
{
/*  pt[ptCnt].x = 10;
  pt[ptCnt++].y = 50;
  pt[ptCnt].x = 20;
  pt[ptCnt++].y = 90;
  pt[ptCnt].x = 50;
  pt[ptCnt++].y = 50;
  pt[ptCnt].x = 30;
  pt[ptCnt++].y = 60;*/

  gtk_init (&argc, &argv);
  draw = gtk_drawing_area_new ();
  gtk_signal_connect (GTK_OBJECT (draw), "expose_event",
    (GtkSignalFunc) DrawExpose, NULL);
  gtk_signal_connect (GTK_OBJECT (draw), "button-release-event",
    (GtkSignalFunc) Click, NULL);
  gtk_widget_set_events (draw, GDK_EXPOSURE_MASK | GDK_BUTTON_RELEASE_MASK |
    GDK_BUTTON_PRESS_MASK |  GDK_POINTER_MOTION_MASK);
  GtkWidget *hbox = gtk_hbox_new (FALSE, 3), *vbox = gtk_vbox_new (FALSE, 0);
  GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_signal_connect (GTK_OBJECT (window), "delete_event",
    GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
  
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_box_pack_start (GTK_BOX (vbox), draw, TRUE, TRUE, 0);
  gtk_widget_show_all (window);
  gtk_main ();
}
