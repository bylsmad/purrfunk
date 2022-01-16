/* :overtones: for PD - takes output of sigmund tracks and adds overtone number as first element of list /*/

#include <m_pd.h>
#include <stdlib.h>
#include <math.h>

#define NTONE_DEF 20
#define CLOSE_ENOUGH_SEMITONES 0.25
#define MIN_FUNDAMENTAL_AMP 0.5

static t_class *overtones_class;

// structure for holding a transformation on selected overtones

#define WHAT_FREQ 1
#define WHAT_AMP 2

#define OP_SCALE 1
#define OP_REPLACE 2

#define SEL_EQ 1
#define SEL_ODD 2
#define SEL_EVEN 3
#define SEL_GT 4
#define SEL_LT 5
#define SEL_ALL 6
#define SEL_OUTLIER 7

typedef struct _op
{
  int what;
  int op;
  t_float op_arg;
  int selector;
  t_float selector_arg;
  struct _op *next;
} t_op;

typedef struct _tone
{
  t_float t_freq;
  t_float t_amp;
  int t_newflag;
  int t_harmonic;
} t_tone;

typedef struct _overtones
{
  t_object x_obj;
  int x_ntone;
  t_tone *x_tonev;
  t_op *x_op_head;
  t_float x_fundamental_freq;
  t_float x_fundamental_amp;
  int x_print_it;
  // left outlet gives output in same format as input - 4 element lists
  t_outlet *x_out_one;
  // right outlet gives output in 5 element lists, first element is harmonic number
  t_outlet *x_out_two;
  
} t_overtones;


static t_float overtones_pitch_diff(t_float f1, t_float f2)
{
    return (f2 > 0.0 ? 17.3123405046 * log(f1 / f2) : -1500);
}

int comp_harmonic(const void *x, const void *y) {
  t_tone t1 = *((t_tone *) x);
  t_tone t2 = *((t_tone *) y);

//  if ( t1.t_harmonic > t2.t_harmonic ) { return 1; }
//  if ( t1.t_harmonic < t2.t_harmonic ) { return -1; }

  if ( t1.t_freq > t2.t_freq ) { return 1; }
  if ( t1.t_freq < t2.t_freq ) { return -1; }
  return 0;
}

static void overtones_out(t_overtones* x)
{
  int i;
  int match;
  t_float freq;
  t_float amp;
  t_float amp_percent;
  t_op *op;
  
  for ( i = 0; i < x->x_ntone; i++) {

    freq = x->x_tonev[i].t_freq;
    amp = x->x_tonev[i].t_amp;

    op = x->x_op_head;
    while ( op ) {
      match = 0;
      if ( op->selector == SEL_ALL ) {
        match = 1;
      } else if ( x->x_tonev[i].t_harmonic == -1 ) {
        if ( op->selector == SEL_OUTLIER ) {
          match = 1;
        }
      } else if (   ( op->selector == SEL_EQ && x->x_tonev[i].t_harmonic == op->selector_arg )
          || ( op->selector == SEL_ODD && x->x_tonev[i].t_harmonic % 2 == 1 )
          || ( op->selector == SEL_EVEN && x->x_tonev[i].t_harmonic % 2 == 0 )
          || ( op->selector == SEL_GT && x->x_tonev[i].t_harmonic > op->selector_arg )
          || ( op->selector == SEL_LT && x->x_tonev[i].t_harmonic < op->selector_arg )
                ) {
          match = 1;
      }

      if ( match ) {
        switch ( op->what ) {
          case WHAT_FREQ:
            switch ( op->op ) {
              case OP_SCALE:
                freq *= op->op_arg;
              break;
              case OP_REPLACE:
                freq = op->op_arg;
              break;
            }
          break;
          case WHAT_AMP:
            switch ( op->op ) {
              case OP_SCALE:
                amp *= op->op_arg;
              break;
              case OP_REPLACE:
                amp = op->op_arg;
              break;
            }
          break;
        }
      }

      op = op->next;
    } 
/*    if ( x->x_print_it && x->x_tonev[i].t_newflag != -1 ) {
      amp_percent = rint(10000.0 * amp / x->x_fundamental_amp) / 100.0; 
      if ( x->x_tonev[i].t_harmonic == -1 ) {
        post("OUT: outlier %f: f = %f a = %f raw_a = %f",x->x_tonev[i].t_freq / x->x_fundamental_freq, freq, amp_percent,amp); 
      } else {
        post("OUT: harmonic %i: f = %f a = %f, raw_a = %f",x->x_tonev[i].t_harmonic, freq, amp_percent, amp);
      }
    }
*/

    // left output provides same data format as sigmund~ tracks
    t_atom at_one[4];
    SETFLOAT(at_one, (t_float)i);
    SETFLOAT(at_one+1, freq);
    SETFLOAT(at_one+2, amp);
    SETFLOAT(at_one+3, x->x_tonev[i].t_newflag);
    outlet_list(x->x_out_one, &s_list, 4, at_one); 

    // right output sends track plus two floats for overtone number analysis
    t_atom at[6];
    t_float ratio = 0.0;
    // ratio of freq to fundamental
    if ( x->x_fundamental_freq > 0.0 ) {
      ratio = x->x_tonev[i].t_freq / x->x_fundamental_freq;
    }
    SETFLOAT(at, ratio);
    // harmonic number or -1 if not an integral mult of fundamental
    SETFLOAT(at+1, x->x_tonev[i].t_harmonic);
    SETFLOAT(at+2, (t_float)i);
    SETFLOAT(at+3, freq);
    SETFLOAT(at+4, amp);
    SETFLOAT(at+5, x->x_tonev[i].t_newflag);
    outlet_list(x->x_out_two, &s_list, 6, at); 
  }
}

void overtones_list(t_overtones *x, t_symbol *s, int argc, t_atom *argv) {
  if ( argc == 4 && s == &s_list ) {
    int tracknum = atom_getintarg(0, argc, argv);
    if ( tracknum < x->x_ntone ) {
      x->x_tonev[tracknum].t_freq = atom_getfloatarg(1, argc, argv);
      x->x_tonev[tracknum].t_amp = atom_getfloatarg(2, argc, argv);
      x->x_tonev[tracknum].t_newflag = (int) atom_getfloatarg(3, argc, argv);
    }
    // silently skip if sigmund is sending more tracks than I'm configured to accept
  } else {
    post("overtones expects lists of 4 numbers from tracks output of sigmund~");
  }
}

void overtones_transform_add(t_overtones *x, t_symbol *what, t_symbol *op_op, t_floatarg op_arg,
  t_symbol *selector, t_floatarg selector_arg) {
  t_op *op = getbytes(sizeof(t_op));
  t_op *prev_op;
  op->next = (t_op *) 0;
  if (what == gensym("freq")) {
    op->what = WHAT_FREQ;
  } else if ( what == gensym("amp")) {
    op->what = WHAT_AMP;
  } else {
    post("ERROR overtones transform first argument not recognized, valid args are freq and amp");
    return;
  }

  if ( op_op == gensym("*")) {
    op->op = OP_SCALE;
  } else if ( op_op == gensym("=")) {
    op->op = OP_REPLACE;
  } else {
    post("ERROR overtones transform second argument not recognized, valid args are = and *");
    return;
  }

  op->op_arg = op_arg;

  if ( selector == gensym("=") ) {
    op->selector = SEL_EQ;
  } else if ( selector == gensym("odd") ) {
    op->selector = SEL_ODD;
  } else if ( selector == gensym("even") ) {
    op->selector = SEL_EVEN;
  } else if ( selector == gensym(">") ) {
    op->selector = SEL_GT;
  } else if ( selector == gensym("<") ) {
    op->selector = SEL_LT;
  } else if ( selector == gensym("all") ) {
    op->selector = SEL_ALL;
  } else if ( selector == gensym("outlier") ) {
    op->selector = SEL_OUTLIER;
  } else {
    post("ERROR fourth parameter of overtones transform not recognized known values are = odd even > < all outlier");
    return;
  }

  op->selector_arg = selector_arg;

  if ( x->x_op_head ) {
    prev_op = x->x_op_head;
    while ( prev_op->next ) {
      prev_op = prev_op->next;
    }
    prev_op->next = op;
  } else {
    x->x_op_head = op;
  }
} 

void overtones_enable_print(t_overtones *x) {
  x->x_print_it = 1;
}

// debug prints
void overtones_print(t_overtones *x) {
  int i;
  t_float amp_percent;
  t_tone* sorted_tones = (t_tone *)copybytes(x->x_tonev, x->x_ntone * sizeof(*x->x_tonev));
  qsort(sorted_tones, x->x_ntone, sizeof(*sorted_tones), comp_harmonic);

  post("fundamental_freq %f fundamental_amp %f",x->x_fundamental_freq,x->x_fundamental_amp);

  for ( i = 0; i < x->x_ntone; i++) {
    // skip inactive tracks
    if ( sorted_tones[i].t_newflag == -1 ) {
      continue;
    }
    amp_percent = rint(10000.0 * sorted_tones[i].t_amp / x->x_fundamental_amp) / 100.0; 
    if ( sorted_tones[i].t_harmonic == -1 ) {
      post("outlier %f: f = %f a = %f raw_a = %f",sorted_tones[i].t_freq / x->x_fundamental_freq, sorted_tones[i].t_freq, amp_percent,sorted_tones[i].t_amp); 
    } else {
      post("harmonic %i: f = %f a = %f, raw_a = %f",sorted_tones[i].t_harmonic, sorted_tones[i].t_freq, amp_percent, sorted_tones[i].t_amp);
    }
  }

  freebytes(sorted_tones, x->x_ntone * sizeof(*x->x_tonev));
  
}


void overtones_bang(t_overtones *x) {
  x->x_fundamental_freq = 0.0;
  x->x_fundamental_amp = 0.0;
  int fundamental_index = -1;
  t_float max_amp = 0.0;
  t_float min_fund_amp = 0.0;
  int i;
  int harmonic;
  // find the maximum aplitude of the peaks
  for ( i = 0; i < x->x_ntone; i++) {
    // skip inactive peaks
    if ( x->x_tonev[i].t_newflag == -1 ) {
      continue;
    }
    if ( x->x_tonev[i].t_amp > max_amp ) {
      max_amp = x->x_tonev[i].t_amp;
    }
  }
  // find the peak with lowest frequency and at least MIN_FUNDAMENTAL_AMP * max_amp
  //  use it as fundamental freq, label it 1
  // set default harmonic number -1 for all others
  min_fund_amp = MIN_FUNDAMENTAL_AMP * max_amp;
  for ( i = 0; i < x->x_ntone; i++) {
    x->x_tonev[i].t_harmonic = -1;
    // skip inactive peaks
    if ( x->x_tonev[i].t_newflag == -1 ) {
      continue;
    }
    if ( x->x_tonev[i].t_amp >= min_fund_amp && ( x->x_fundamental_freq == 0.0 || x->x_tonev[i].t_freq < x->x_fundamental_freq ) ) {
      fundamental_index = i;
      x->x_fundamental_freq = x->x_tonev[i].t_freq;
      x->x_fundamental_amp = x->x_tonev[i].t_amp;
    }
  }
  if ( fundamental_index != -1 ) {
    x->x_tonev[fundamental_index].t_harmonic = 1;
  }

  // ideantify each peak with near multiple of fundamental freq and label
  for ( i = 0; i < x->x_ntone; i++) {
    // skip fundamental
    if ( x->x_tonev[i].t_harmonic != -1 ) {
      continue;
    }
    // skip inactive tracks
    if ( x->x_tonev[i].t_newflag == -1 ) {
      continue;
    }
    // get the multiple of fundamental_freq closest to t_freq
    harmonic = (int) rint( x->x_tonev[i].t_freq / x->x_fundamental_freq);

    if ( harmonic > 0 ) {
      if ( fabs( overtones_pitch_diff( x->x_tonev[i].t_freq, ( (t_float) harmonic ) * x->x_fundamental_freq)) < CLOSE_ENOUGH_SEMITONES ) {
        x->x_tonev[i].t_harmonic = harmonic;
        continue;
      }
    }

    // negative numbers for frequencies which are 1/N times fundamental for whole number N >= 2
    harmonic = (int) rint( x->x_fundamental_freq / x->x_tonev[i].t_freq );

    if ( harmonic > 0 ) {
      if ( fabs( overtones_pitch_diff( x->x_tonev[i].t_freq,  x->x_fundamental_freq / ( (t_float) harmonic ))) < CLOSE_ENOUGH_SEMITONES ) {
        x->x_tonev[i].t_harmonic = (-1) * harmonic;
      }
    }
  }
  overtones_out(x);
  if ( x->x_print_it ) {
    overtones_print(x);
    x->x_print_it = 0;
  }
}

void overtones_clear(t_overtones *x) {
  t_op *op;
  op = x->x_op_head;
  while ( op ) {
    op = op->next;
    freebytes(op, sizeof(t_op));
  }
  x->x_op_head = (t_op *) 0;

}


void overtones_free(t_overtones *x)
{
  if (x->x_tonev)
    freebytes(x->x_tonev, x->x_ntone * sizeof(*x->x_tonev));
  overtones_clear(x);
}


static void *overtones_new(t_floatarg ntone_param)
{
    int i;
    t_overtones *x = (t_overtones *)pd_new(overtones_class);
    x->x_out_one = outlet_new(&x->x_obj, &s_list);
    x->x_out_two = outlet_new(&x->x_obj, &s_list);
    x->x_ntone = (int) ntone_param;
    if ( x->x_ntone <= 0 ) {
      x->x_ntone = NTONE_DEF;
    }
    x->x_tonev = (t_tone *)getbytes(x->x_ntone * sizeof(*x->x_tonev));
    // initialize all tones to be empty tracks
    for ( i = 0; i < x->x_ntone; i++) {
      x->x_tonev[i].t_newflag = -1;
    }
    x->x_print_it = 0;
    x->x_op_head = (t_op *) 0;

    return (x);
}

void overtones_setup(void)
{
    overtones_class = class_new(gensym("overtones"), (t_newmethod)overtones_new, 
			    (t_method)overtones_free,sizeof(t_overtones), 
          CLASS_DEFAULT,  
          A_DEFFLOAT, 0);
    class_addlist(overtones_class,overtones_list);
  class_addbang(overtones_class,overtones_bang);
  class_addmethod(overtones_class,  
        (t_method)overtones_enable_print, gensym("print"), 0);  
  class_addmethod(overtones_class,  
        (t_method)overtones_transform_add, gensym("transform"), 
        A_DEFSYMBOL, A_DEFSYMBOL, A_DEFFLOAT, A_DEFSYMBOL, A_DEFFLOAT,0);  
  class_addmethod(overtones_class,  
        (t_method)overtones_clear, gensym("clear"), 0);  
}
