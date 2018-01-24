// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "lifealgo.h"
#include "util.h"       // for lifestatus
#include "string.h"
using namespace std ;
lifealgo::~lifealgo() {
   poller = 0 ;
   maxCellStates = 2 ;
}
int lifealgo::verbose ;
/*
 *   Right now, the base/expo should match the current increment.
 *   We do not check this.
 */
int lifealgo::startrecording(int basearg, int expoarg) {
  if (timeline.framecount) {
    // already have a timeline; skip to its end
    gotoframe(timeline.framecount-1) ;
  } else {
    // use the current frame and increment to start a new timeline
    void *now = getcurrentstate() ;
    if (now == 0)
      return 0 ;
    timeline.base = basearg ;
    timeline.expo = expoarg ;
    timeline.frames.push_back(now) ;
    timeline.framecount = 1 ;
    timeline.end = timeline.start = generation ;
    timeline.inc = increment ;
  }
  timeline.next = timeline.end ;
  timeline.next += timeline.inc ;
  timeline.recording = 1 ;
  return timeline.framecount ;
}
pair<int, int> lifealgo::stoprecording() {
  timeline.recording = 0 ;
  timeline.next = 0 ;
  return make_pair(timeline.base, timeline.expo) ;
}
void lifealgo::extendtimeline() {
  if (timeline.recording && generation == timeline.next) {
    void *now = getcurrentstate() ;
    if (now && timeline.framecount < MAX_FRAME_COUNT) {
      timeline.frames.push_back(now) ;
      timeline.framecount++ ;
      timeline.end = timeline.next ;
      timeline.next += timeline.inc ;
    }
  }
}
/*
 *   Note that this *also* changes inc, so don't call unless this is
 *   what you want to do.  It does not update or change the base or
 *   expo if the base != 2, so they can get out of sync.
 *
 *   Currently this is only used by bgolly, and it will only work
 *   properly if the increment argument is a power of two.
 */
void lifealgo::pruneframes() {
   if (timeline.framecount > 1) {
      for (int i=2; i<timeline.framecount; i += 2)
         timeline.frames[i >> 1]  = timeline.frames[i] ;
      timeline.framecount = (timeline.framecount + 1) >> 1 ;
      timeline.frames.resize(timeline.framecount) ;
      timeline.inc += timeline.inc ;
      timeline.end = timeline.inc ;
      timeline.end.mul_smallint(timeline.framecount-1) ;
      timeline.end += timeline.start ;
      timeline.next = timeline.end ;
      timeline.next += timeline.inc ;
      if (timeline.base == 2)
         timeline.expo++ ;
   }
}
int lifealgo::gotoframe(int i) {
  if (i < 0 || i >= timeline.framecount)
    return 0 ;
  setcurrentstate(timeline.frames[i]) ;
  // AKT: avoid mul_smallint(i) crashing with divide-by-zero if i is 0
  if (i > 0) {
    generation = timeline.inc ;
    generation.mul_smallint(i) ;
  } else {
    generation = 0;
  }
  generation += timeline.start ;
  return timeline.framecount ;
}
void lifealgo::destroytimeline() {
  timeline.frames.clear() ;
  timeline.recording = 0 ;
  timeline.framecount = 0 ;
  timeline.end = 0 ;
  timeline.start = 0 ;
  timeline.inc = 0 ;
  timeline.next = 0 ;
}

// -----------------------------------------------------------------------------

// AKT: the following routines provide support for a bounded universe

const char* lifealgo::setgridsize(const char* suffix) {
   // parse a rule suffix like ":T100,200" and set the various grid parameters;
   // note that we allow any legal partial suffix -- this lets people type a
   // suffix into the Set Rule dialog without the algorithm changing to UNKNOWN
   const char *p = suffix;
   char topology = 0;
   gridwd = gridht = 0;
   hshift = vshift = 0;
   htwist = vtwist = false;
   boundedplane = false;
   sphere = false;
   
   p++;
   if (*p == 0) return 0;                 // treat ":" like ":T0,0"
   if (*p == 't' || *p == 'T') {
      // torus or infinite tube
      topology = 'T';
   } else if (*p == 'p' || *p == 'P') {
      boundedplane = true;
      topology = 'P';
   } else if (*p == 's' || *p == 'S') {
      sphere = true;
      topology = 'S';
   } else if (*p == 'k' || *p == 'K') {
      // Klein bottle (either htwist or vtwist should become true)
      topology = 'K';
   } else if (*p == 'c' || *p == 'C') {
      // cross-surface
      htwist = vtwist = true;
      topology = 'C';
   } else {
      return "Unknown grid topology.";
   }
   
   p++;
   if (*p == 0) return 0;                 // treat ":<char>" like ":T0,0"
   
   while ('0' <= *p && *p <= '9') {
      if (gridwd >= 200000000) {
         gridwd =   2000000000;           // keep width within editable limits
      } else {
         gridwd = 10 * gridwd + *p - '0';
      }
      p++;
   }
   if (*p == '*') {
      if (topology != 'K') return "Only specify a twist for a Klein bottle.";
      htwist = true;
      p++;
   }
   if (*p == '+' || *p == '-') {
      if (topology == 'P') return "Plane can't have a shift.";
      if (topology == 'S') return "Sphere can't have a shift.";
      if (topology == 'C') return "Cross-surface can't have a shift.";
      if (topology == 'K' && !htwist) return "Shift must be on twisted edges.";
      if (gridwd == 0) return "Can't shift infinite width.";
      int sign = *p == '+' ? 1 : -1;
      p++;
      while ('0' <= *p && *p <= '9') {
         hshift = 10 * hshift + *p - '0';
         p++;
      }
      if (hshift >= (int)gridwd) hshift = hshift % (int)gridwd;
      hshift *= sign;
   }
   if (*p == ',' && topology != 'S') {
      p++;
   } else if (*p) {
      return "Unexpected stuff after grid width.";
   }

   // gridwd has been set
   if ((topology == 'K' || topology == 'C' || topology == 'S') && gridwd == 0) {
      return "Given topology can't have an infinite width.";
   }
   
   if (*p == 0) {
      // grid height is not specified so set it to grid width;
      // ie. treat ":T100" like ":T100,100";
      // this also allows us to have ":S100" rather than ":S100,100"
      gridht = gridwd;
   } else {
      while ('0' <= *p && *p <= '9') {
         if (gridht >= 200000000) {
            gridht =   2000000000;     // keep height within editable limits
         } else {
            gridht = 10 * gridht + *p - '0';
         }
         p++;
      }
      if (*p == '*') {
         if (topology != 'K') return "Only specify a twist for a Klein bottle.";
         if (htwist) return "Klein bottle can't have both horizontal and vertical twists.";
         vtwist = true;
         p++;
      }
      if (*p == '+' || *p == '-') {
         if (topology == 'P') return "Plane can't have a shift.";
         if (topology == 'C') return "Cross-surface can't have a shift.";
         if (topology == 'K' && !vtwist) return "Shift must be on twisted edges.";
         if (hshift != 0) return "Can't have both horizontal and vertical shifts.";
         if (gridht == 0) return "Can't shift infinite height.";
         int sign = *p == '+' ? 1 : -1;
         p++;
         while ('0' <= *p && *p <= '9') {
            vshift = 10 * vshift + *p - '0';
            p++;
         }
         if (vshift >= (int)gridht) vshift = vshift % (int)gridht;
         vshift *= sign;
      }
      if (*p) return "Unexpected stuff after grid height.";
   }

   // gridht has been set
   if ((topology == 'K' || topology == 'C') && gridht == 0) {
      return "Klein bottle or cross-surface can't have an infinite height.";
   }
   
   if (topology == 'K' && !(htwist || vtwist)) {
      // treat ":K10,20" like ":K10,20*"
      vtwist = true;
   }
   
   if ((hshift != 0 || vshift != 0) && (gridwd == 0 || gridht == 0)) {
      return "Shifting is not allowed if either grid dimension is unbounded.";
   }
   
   // now ok to set grid edges
   if (gridwd > 0) {
      gridleft = -int(gridwd) / 2;
      gridright = int(gridwd) - 1;
      gridright += gridleft;
   } else {
      // play safe and set these to something
      gridleft = bigint::zero;
      gridright = bigint::zero;
   }
   if (gridht > 0) {
      gridtop = -int(gridht) / 2;
      gridbottom = int(gridht) - 1;
      gridbottom += gridtop;
   } else {
      // play safe and set these to something
      gridtop = bigint::zero;
      gridbottom = bigint::zero;
   }
   return 0;
}

const char* lifealgo::canonicalsuffix() {
   if (gridwd > 0 || gridht > 0) {
      static char bounds[64];
      if (boundedplane) {
         sprintf(bounds, ":P%u,%u", gridwd, gridht);
      } else if (sphere) {
         // sphere requires a square grid (gridwd == gridht)
         sprintf(bounds, ":S%u", gridwd);
      } else if (htwist && vtwist) {
         // cross-surface if both horizontal and vertical edges are twisted
         sprintf(bounds, ":C%u,%u", gridwd, gridht);
      } else if (htwist) {
         // Klein bottle if only horizontal edges are twisted
         if (hshift != 0 && (gridwd & 1) == 0) {
            // twist and shift is only possible if gridwd is even and hshift is 1
            sprintf(bounds, ":K%u*+1,%u", gridwd, gridht);
         } else {
            sprintf(bounds, ":K%u*,%u", gridwd, gridht);
         }
      } else if (vtwist) {
         // Klein bottle if only vertical edges are twisted
         if (vshift != 0 && (gridht & 1) == 0) {
            // twist and shift is only possible if gridht is even and vshift is 1
            sprintf(bounds, ":K%u,%u*+1", gridwd, gridht);
         } else {
            sprintf(bounds, ":K%u,%u*", gridwd, gridht);
         }
      } else if (hshift < 0) {
         // torus with -ve horizontal shift
         sprintf(bounds, ":T%u%d,%u", gridwd, hshift, gridht);
      } else if (hshift > 0) {
         // torus with +ve horizontal shift
         sprintf(bounds, ":T%u+%d,%u", gridwd, hshift, gridht);
      } else if (vshift < 0) {
         // torus with -ve vertical shift
         sprintf(bounds, ":T%u,%u%d", gridwd, gridht, vshift);
      } else if (vshift > 0) {
         // torus with +ve vertical shift
         sprintf(bounds, ":T%u,%u+%d", gridwd, gridht, vshift);
      } else {
         // unshifted torus, or an infinite tube
         sprintf(bounds, ":T%u,%u", gridwd, gridht);
      }
      return bounds;
   } else {
      // unbounded universe
      return 0;
   }
}

void lifealgo::JoinTwistedEdges()
{
    // set grid edges
    int gl = gridleft.toint();
    int gt = gridtop.toint();
    int gr = gridright.toint();
    int gb = gridbottom.toint();
    
    // border edges are 1 cell outside grid edges
    int bl = gl - 1;
    int bt = gt - 1;
    int br = gr + 1;
    int bb = gb + 1;
    
    if (htwist && vtwist) {
        // cross-surface
        //  eg. :C4,3
        //  a l k j i d
        //  l A B C D i
        //  h E F G H e
        //  d I J K L a
        //  i d c b a l
        
        for (int x = gl; x <= gr; x++) {
            int twistedx = gr - x + gl;
            int state = getcell(twistedx, gt);
            if (state > 0) setcell(x, bb, state);
            state = getcell(twistedx, gb);
            if (state > 0) setcell(x, bt, state);
        }
        
        for (int y = gt; y <= gb; y++) {
            int twistedy = gb - y + gt;
            int state = getcell(gl, twistedy);
            if (state > 0) setcell(br, y, state);
            state = getcell(gr, twistedy);
            if (state > 0) setcell(bl, y, state);
        }
        
        // copy grid's corner cells to SAME corners in border
        // (these cells are topologically different to non-corner cells)
        setcell(bl, bt, getcell(gl, gt));
        setcell(br, bt, getcell(gr, gt));
        setcell(br, bb, getcell(gr, gb));
        setcell(bl, bb, getcell(gl, gb));
        
    } else if (htwist) {
        // Klein bottle with top and bottom edges twisted 180 degrees
        //  eg. :K4*,3
        //  i l k j i l
        //  d A B C D a
        //  h E F G H e
        //  l I J K L i
        //  a d c b a d
        
        for (int x = gl; x <= gr; x++) {
            int twistedx = gr - x + gl;
            int state = getcell(twistedx, gt);
            if (state > 0) setcell(x, bb, state);
            state = getcell(twistedx, gb);
            if (state > 0) setcell(x, bt, state);
        }
        
        for (int y = gt; y <= gb; y++) {
            // join left and right edges with no twist
            int state = getcell(gl, y);
            if (state > 0) setcell(br, y, state);
            state = getcell(gr, y);
            if (state > 0) setcell(bl, y, state);
        }
        
        // do corner cells
        setcell(bl, bt, getcell(gl, gb));
        setcell(br, bt, getcell(gr, gb));
        setcell(bl, bb, getcell(gl, gt));
        setcell(br, bb, getcell(gr, gt));
        
    } else { // vtwist
        // Klein bottle with left and right edges twisted 180 degrees
        //  eg. :K4,3*
        //  d i j k l a
        //  l A B C D i
        //  h E F G H e
        //  d I J K L a
        //  l a b c d i
        
        for (int x = gl; x <= gr; x++) {
            // join top and bottom edges with no twist
            int state = getcell(x, gt);
            if (state > 0) setcell(x, bb, state);
            state = getcell(x, gb);
            if (state > 0) setcell(x, bt, state);
        }
        
        for (int y = gt; y <= gb; y++) {
            int twistedy = gb - y + gt;
            int state = getcell(gl, twistedy);
            if (state > 0) setcell(br, y, state);
            state = getcell(gr, twistedy);
            if (state > 0) setcell(bl, y, state);
        }
        
        // do corner cells
        setcell(bl, bt, getcell(gr, gt));
        setcell(br, bt, getcell(gl, gt));
        setcell(bl, bb, getcell(gr, gb));
        setcell(br, bb, getcell(gl, gb));
    }
}

void lifealgo::JoinTwistedAndShiftedEdges()
{
    // set grid edges
    int gl = gridleft.toint();
    int gt = gridtop.toint();
    int gr = gridright.toint();
    int gb = gridbottom.toint();
    
    // border edges are 1 cell outside grid edges
    int bl = gl - 1;
    int bt = gt - 1;
    int br = gr + 1;
    int bb = gb + 1;
    
    if (hshift != 0) {
        // Klein bottle with shift by 1 on twisted horizontal edge (with even number of cells)
        //  eg. :K4*+1,3
        //  j i l k j i
        //  d A B C D a
        //  h E F G H e
        //  l I J K L i
        //  b a d c b a
        
        int state, twistedx, shiftedx;
        
        for (int x = gl; x <= gr; x++) {
            // join top and bottom edges with a twist and then shift by 1
            twistedx = gr - x + gl;
            shiftedx = twistedx - 1; if (shiftedx < gl) shiftedx = gr;
            state = getcell(shiftedx, gb);
            if (state > 0) setcell(x, bt, state);
            
            state = getcell(shiftedx, gt);
            if (state > 0) setcell(x, bb, state);
        }
        
        for (int y = gt; y <= gb; y++) {
            // join left and right edges with no twist or shift
            state = getcell(gl, y);
            if (state > 0) setcell(br, y, state);
            state = getcell(gr, y);
            if (state > 0) setcell(bl, y, state);
        }
        
        // do corner cells
        shiftedx = gl - 1; if (shiftedx < gl) shiftedx = gr;
        setcell(bl, bt, getcell(shiftedx, gb));
        setcell(bl, bb, getcell(shiftedx, gt));
        shiftedx = gr - 1; if (shiftedx < gl) shiftedx = gr;
        setcell(br, bt, getcell(shiftedx, gb));
        setcell(br, bb, getcell(shiftedx, gt));
        
    } else { // vshift != 0
        // Klein bottle with shift by 1 on twisted vertical edge (with even number of cells)
        //  eg. :K3,4*+1
        //  f j k l d
        //  c A B C a
        //  l D E F j
        //  i G H I g
        //  f J K L d
        //  c a b c a
        
        int state, twistedy, shiftedy;
        
        for (int x = gl; x <= gr; x++) {
            // join top and bottom edges with no twist or shift
            state = getcell(x, gt);
            if (state > 0) setcell(x, bb, state);
            state = getcell(x, gb);
            if (state > 0) setcell(x, bt, state);
        }
        
        for (int y = gt; y <= gb; y++) {
            // join left and right edges with a twist and then shift by 1
            twistedy = gb - y + gt;
            shiftedy = twistedy - 1; if (shiftedy < gt) shiftedy = gb;
            state = getcell(gr, shiftedy);
            if (state > 0) setcell(bl, y, state);
            
            state = getcell(gl, shiftedy);
            if (state > 0) setcell(br, y, state);
        }
        
        // do corner cells
        shiftedy = gt - 1; if (shiftedy < gt) shiftedy = gb;
        setcell(bl, bt, getcell(gr, shiftedy));
        setcell(br, bt, getcell(gl, shiftedy));
        shiftedy = gb - 1; if (shiftedy < gt) shiftedy = gb;
        setcell(bl, bb, getcell(gr, shiftedy));
        setcell(br, bb, getcell(gl, shiftedy));
    }
}

void lifealgo::JoinShiftedEdges()
{
    // set grid edges
    int gl = gridleft.toint();
    int gt = gridtop.toint();
    int gr = gridright.toint();
    int gb = gridbottom.toint();
    
    // border edges are 1 cell outside grid edges
    int bl = gl - 1;
    int bt = gt - 1;
    int br = gr + 1;
    int bb = gb + 1;
    
    if (hshift != 0) {
        // torus with horizontal shift
        //  eg. :T4+1,3
        //  k l i j k l
        //  d A B C D a
        //  h E F G H e
        //  l I J K L i
        //  a b c d a b
        
        int state, shiftedx;
        
        for (int x = gl; x <= gr; x++) {
            // join top and bottom edges with a horizontal shift
            shiftedx = x - hshift;
            if (shiftedx < gl) shiftedx += gridwd; else if (shiftedx > gr) shiftedx -= gridwd;
            state = getcell(shiftedx, gb);
            if (state > 0) setcell(x, bt, state);
            
            shiftedx = x + hshift;
            if (shiftedx < gl) shiftedx += gridwd; else if (shiftedx > gr) shiftedx -= gridwd;
            state = getcell(shiftedx, gt);
            if (state > 0) setcell(x, bb, state);
        }
        
        for (int y = gt; y <= gb; y++) {
            // join left and right edges with no shift
            state = getcell(gl, y);
            if (state > 0) setcell(br, y, state);
            
            state = getcell(gr, y);
            if (state > 0) setcell(bl, y, state);
        }
        
        // do corner cells
        shiftedx = gr - hshift;
        if (shiftedx < gl) shiftedx += gridwd; else if (shiftedx > gr) shiftedx -= gridwd;
        setcell(bl, bt, getcell(shiftedx, gb));
        shiftedx = gl - hshift;
        if (shiftedx < gl) shiftedx += gridwd; else if (shiftedx > gr) shiftedx -= gridwd;
        setcell(br, bt, getcell(shiftedx, gb));
        shiftedx = gr + hshift;
        if (shiftedx < gl) shiftedx += gridwd; else if (shiftedx > gr) shiftedx -= gridwd;
        setcell(bl, bb, getcell(shiftedx, gt));
        shiftedx = gl + hshift;
        if (shiftedx < gl) shiftedx += gridwd; else if (shiftedx > gr) shiftedx -= gridwd;
        setcell(br, bb, getcell(shiftedx, gt));
        
    } else { // vshift != 0
        // torus with vertical shift
        //  eg. :T4,3+1
        //  h i j k l a
        //  l A B C D e
        //  d E F G H i
        //  h I J K L a
        //  l a b c d e
        
        int state, shiftedy;
        
        for (int x = gl; x <= gr; x++) {
            // join top and bottom edges with no shift
            state = getcell(x, gt);
            if (state > 0) setcell(x, bb, state);
            
            state = getcell(x, gb);
            if (state > 0) setcell(x, bt, state);
        }
        
        for (int y = gt; y <= gb; y++) {
            // join left and right edges with a vertical shift
            shiftedy = y - vshift;
            if (shiftedy < gt) shiftedy += gridht; else if (shiftedy > gb) shiftedy -= gridht;
            state = getcell(gr, shiftedy);
            if (state > 0) setcell(bl, y, state);
            
            shiftedy = y + vshift;
            if (shiftedy < gt) shiftedy += gridht; else if (shiftedy > gb) shiftedy -= gridht;
            state = getcell(gl, shiftedy);
            if (state > 0) setcell(br, y, state);
        }
        
        // do corner cells
        shiftedy = gb - vshift;
        if (shiftedy < gt) shiftedy += gridht; else if (shiftedy > gb) shiftedy -= gridht;
        setcell(bl, bt, getcell(gr, shiftedy));
        shiftedy = gb + vshift;
        if (shiftedy < gt) shiftedy += gridht; else if (shiftedy > gb) shiftedy -= gridht;
        setcell(br, bt, getcell(gl, shiftedy));
        shiftedy = gt - vshift;
        if (shiftedy < gt) shiftedy += gridht; else if (shiftedy > gb) shiftedy -= gridht;
        setcell(bl, bb, getcell(gr, shiftedy));
        shiftedy = gt + vshift;
        if (shiftedy < gt) shiftedy += gridht; else if (shiftedy > gb) shiftedy -= gridht;
        setcell(br, bb, getcell(gl, shiftedy));
    }
}

void lifealgo::JoinAdjacentEdges(int pt, int pl, int pb, int pr)    // pattern edges
{
    // set grid edges
    int gl = gridleft.toint();
    int gt = gridtop.toint();
    int gr = gridright.toint();
    int gb = gridbottom.toint();
    
    // border edges are 1 cell outside grid edges
    int bl = gl - 1;
    int bt = gt - 1;
    int br = gr + 1;
    int bb = gb + 1;
    
    // sphere
    //  eg. :S3
    //  a a d g c
    //  a A B C g
    //  b D E F h
    //  c G H I i
    //  g c f i i
    
    // copy live cells in top edge to left border
    for (int x = pl; x <= pr; x++) {
        int state;
        int skip = nextcell(x, gt, state);
        if (skip < 0) break;
        x += skip;
        if (state > 0) setcell(bl, gt + (x - gl), state);
    }
    
    // copy live cells in left edge to top border
    for (int y = pt; y <= pb; y++) {
        // no point using nextcell() here -- edge is only 1 cell wide
        int state = getcell(gl, y);
        if (state > 0) setcell(gl + (y - gt), bt, state);
    }
    
    // copy live cells in bottom edge to right border
    for (int x = pl; x <= pr; x++) {
        int state;
        int skip = nextcell(x, gb, state);
        if (skip < 0) break;
        x += skip;
        if (state > 0) setcell(br, gt + (x - gl), state);
    }
    
    // copy live cells in right edge to bottom border
    for (int y = pt; y <= pb; y++) {
        // no point using nextcell() here -- edge is only 1 cell wide
        int state = getcell(gr, y);
        if (state > 0) setcell(gl + (y - gt), bb, state);
    }
    
    // copy grid's corner cells to SAME corners in border
    setcell(bl, bt, getcell(gl, gt));
    setcell(br, bt, getcell(gr, gt));
    setcell(br, bb, getcell(gr, gb));
    setcell(bl, bb, getcell(gl, gb));
}

void lifealgo::JoinEdges(int pt, int pl, int pb, int pr)    // pattern edges
{
    // set grid edges
    int gl = gridleft.toint();
    int gt = gridtop.toint();
    int gr = gridright.toint();
    int gb = gridbottom.toint();
    
    // border edges are 1 cell outside grid edges
    int bl = gl - 1;
    int bt = gt - 1;
    int br = gr + 1;
    int bb = gb + 1;
    
    if (gridht > 0) {
        // copy live cells in top edge to bottom border
        for (int x = pl; x <= pr; x++) {
            int state;
            int skip = nextcell(x, gt, state);
            if (skip < 0) break;
            x += skip;
            if (state > 0) setcell(x, bb, state);
        }
        // copy live cells in bottom edge to top border
        for (int x = pl; x <= pr; x++) {
            int state;
            int skip = nextcell(x, gb, state);
            if (skip < 0) break;
            x += skip;
            if (state > 0) setcell(x, bt, state);
        }
    }
    
    if (gridwd > 0) {
        // copy live cells in left edge to right border
        for (int y = pt; y <= pb; y++) {
            // no point using nextcell() here -- edge is only 1 cell wide
            int state = getcell(gl, y);
            if (state > 0) setcell(br, y, state);
        }
        // copy live cells in right edge to left border
        for (int y = pt; y <= pb; y++) {
            // no point using nextcell() here -- edge is only 1 cell wide
            int state = getcell(gr, y);
            if (state > 0) setcell(bl, y, state);
        }
    }
    
    if (gridwd > 0 && gridht > 0) {
        // copy grid's corner cells to opposite corners in border
        setcell(bl, bt, getcell(gr, gb));
        setcell(br, bt, getcell(gl, gb));
        setcell(br, bb, getcell(gl, gt));
        setcell(bl, bb, getcell(gr, gt));
    }
}

bool lifealgo::CreateBorderCells()
{
    // no need to do anything if there is no pattern or if the grid is a bounded plane
    if (isEmpty() || boundedplane) return true;
    
    bigint top, left, bottom, right;
    findedges(&top, &left, &bottom, &right);
    
    // no need to do anything if pattern is completely inside grid edges
    if ( (gridwd == 0 || (gridleft < left && gridright > right)) &&
         (gridht == 0 || (gridtop < top && gridbottom > bottom)) ) {
        return true;
    }
    
    // if grid has infinite width or height then pattern might be too big to use setcell/getcell
    if ( (gridwd == 0 || gridht == 0) &&
         (top < bigint::min_coord || left < bigint::min_coord ||
          bottom > bigint::max_coord || right > bigint::max_coord) ) {
        lifestatus("Pattern is beyond editing limit!");
        // return false so caller can exit step() loop
        return false;
    }
    
    if (sphere) {
        // to get a sphere we join top edge with left edge, and right edge with bottom edge;
        // note that grid must be square (gridwd == gridht)
        int pl = left.toint();
        int pt = top.toint();
        int pr = right.toint();      
        int pb = bottom.toint();
        JoinAdjacentEdges(pt, pl, pb, pr);
        
    } else if (htwist || vtwist) {
        // Klein bottle or cross-surface
        if ( (htwist && hshift != 0 && (gridwd & 1) == 0) ||
             (vtwist && vshift != 0 && (gridht & 1) == 0) ) {
            // Klein bottle with shift is only possible if the shift is on the
            // twisted edge and that edge has an even number of cells
            JoinTwistedAndShiftedEdges();
        } else {
            JoinTwistedEdges();
        }
        
    } else if (hshift != 0 || vshift != 0) {
        // torus with horizontal or vertical shift
        JoinShiftedEdges();
        
    } else {
        // unshifted torus or infinite tube
        int pl = left.toint();
        int pt = top.toint();
        int pr = right.toint();      
        int pb = bottom.toint();
        JoinEdges(pt, pl, pb, pr);
    }
    
    endofpattern();
    return true;
}

void lifealgo::ClearRect(int top, int left, int bottom, int right)
{
    int cx, cy, v;
    for ( cy = top; cy <= bottom; cy++ ) {
        for ( cx = left; cx <= right; cx++ ) {
            int skip = nextcell(cx, cy, v);
            if (skip + cx > right)
                skip = -1;           // pretend we found no more live cells
            if (skip >= 0) {
                // found next live cell so delete it
                cx += skip;
                setcell(cx, cy, 0);
            } else {
                cx = right + 1;     // done this row
            }
        }
    }
}

bool lifealgo::DeleteBorderCells()
{
    // no need to do anything if there is no pattern
    if (isEmpty()) return true;
    
    // need to find pattern edges because pattern may have expanded beyond grid
    // (typically by 2 cells, but could be more if rule allows births in empty space)
    bigint top, left, bottom, right;
    findedges(&top, &left, &bottom, &right);
    
    // no need to do anything if grid encloses entire pattern
    if ( (gridwd == 0 || (gridleft <= left && gridright >= right)) &&
         (gridht == 0 || (gridtop <= top && gridbottom >= bottom)) ) {
        return true;
    }
    
    // set pattern edges
    int pl = left.toint();
    int pt = top.toint();
    int pr = right.toint();      
    int pb = bottom.toint();
    
    // set grid edges
    int gl = gridleft.toint();
    int gt = gridtop.toint();
    int gr = gridright.toint();
    int gb = gridbottom.toint();
    
    if (gridht > 0 && pt < gt) {
        // delete live cells above grid
        ClearRect(pt, pl, gt-1, pr);
        pt = gt; // reduce size of rect below
    }
    
    if (gridht > 0 && pb > gb) {
        // delete live cells below grid
        ClearRect(gb+1, pl, pb, pr);
        pb = gb; // reduce size of rect below
    }
    
    if (gridwd > 0 && pl < gl) {
        // delete live cells left of grid
        ClearRect(pt, pl, pb, gl-1);
    }
    
    if (gridwd > 0 && pr > gr) {
        // delete live cells right of grid
        ClearRect(pt, gr+1, pb, pr);
    }
    
    endofpattern();
    
    // do this test AFTER clearing border
    if ( top < bigint::min_coord || left < bigint::min_coord ||
         bottom > bigint::max_coord || right > bigint::max_coord ) {
        lifestatus("Pattern exceeded editing limit!");
        // return false so caller can exit step() loop
        return false;
    }

    return true;
}

void lifealgo::getcells(unsigned char *buf, int x, int y, int w, int h) {
   viewport vp(w, h) ;
   vp.setpositionmag(x+(w>>1), y+(h>>1), 0) ;
   staterender hsr(buf, w, h) ;
   memset(buf, 0, w*h) ;
   draw(vp, hsr) ;
}

// -----------------------------------------------------------------------------

int staticAlgoInfo::nextAlgoId = 0 ;
staticAlgoInfo *staticAlgoInfo::head = 0 ;
staticAlgoInfo::staticAlgoInfo() {
   id = nextAlgoId++ ;
   next = head ;
   head = this ;
   // init default icon data
   defxpm7x7 = NULL;
   defxpm15x15 = NULL;
   defxpm31x31 = NULL;
}
staticAlgoInfo *staticAlgoInfo::byName(const char *s) {
   for (staticAlgoInfo *i=head; i; i=i->next)
      if (strcmp(i->algoName, s) == 0)
         return i ;
   return 0 ;
}
int staticAlgoInfo::nameToIndex(const char *s) {
   staticAlgoInfo *r = byName(s) ;
   if (r == 0)
      return -1 ;
   return r->id ;
}
staticAlgoInfo &staticAlgoInfo::tick() {
   return *(new staticAlgoInfo()) ;
}
