/* :overtones: for PD - takes output of sigmund tracks and adds overtone number as first element of list /*/

#include <m_pd.h>
#include <stdlib.h>
#include <math.h>

#define NTONE_DEF 20
#define CLOSE_ENOUGH_SEMITONES 0.5
#define MIN_FUNDAMENTAL_AMP 0.15
#define TOP_SCORE_HISTORY_LENGTH 3
#define LOGTEN 2.302585092994

static t_class *overtones_class;

// structure for holding a transformation on selected overtones

#define WHAT_FREQ 1
#define WHAT_AMP 2
#define WHAT_DB 3

#define OP_SCALE 1
#define OP_REPLACE 2

#define SEL_EQ 1
#define SEL_ODD 2
#define SEL_EVEN 3
#define SEL_GT 4
#define SEL_GE 5
#define SEL_LT 6
#define SEL_LE 7
#define SEL_ALL 8
#define SEL_OUTLIER 9

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
  t_float t_score;
} t_tone;

typedef struct _overtones
{
  t_object x_obj;
  int x_ntone;
  t_tone *x_tonev;
  t_op *x_op_head;
  t_float x_fundamental_freq;
  int x_fundamental_index;
  int *x_top_score_history;
  t_float x_fundamental_amp;
  int x_print_it;
  // left outlet gives output in same format as input - 4 element lists
  t_outlet *x_out_one;
  // right outlet gives output in 5 element lists, first element is harmonic number
  t_outlet *x_out_two;

} t_overtones;

// returns how many semitones f1 is above f2
// +-infinity are represented as +-1500
static t_float overtones_pitch_diff(t_float f1, t_float f2)
{
    return (f1 > 0.0 ? (f2 > 0.0 ? 17.3123405046 * log(f1 / f2) : 1500) : -1500);
}

static t_float getDistanceToHarmonic( t_float freq, t_float fundamental, int harmonic) {
  if ( harmonic > 0 ) {
    return fabs( overtones_pitch_diff( freq, ((t_float) harmonic ) * fundamental)); 
  } 
  if ( harmonic < 0 ) {
    return fabs( overtones_pitch_diff( ((t_float) ( -1 * harmonic) ) * freq, fundamental));
  }
  return 1500;
}

static int getNearestMult ( t_float f1, t_float f2 ) {
  t_float round_up_cutoff;
  int result;
  
  result = floor( f1 / f2 );
  if ( result > 0 ) {
    round_up_cutoff = result * sqrt ( ( result + 1.0 ) / result ) * f2;
  } else {
    round_up_cutoff =  sqrt ( 0.5 ) * f2;
  }
  if ( f1 >= round_up_cutoff ) {
    result += 1;
  }
  return result;
}

static int getHarmonic( t_float freq, t_float fundamental, int includeNegative ) {

  int result;

  result = getNearestMult( freq, fundamental );
  if ( result == 0 && includeNegative ) {
    result = -1 * getNearestMult( fundamental, freq);
  }
  return result;
}

static int comp_harmonic(const void *x, const void *y) {
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
  t_float freq, ratio;
  t_float amp,  amp_ratio;
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
          || ( op->selector == SEL_ODD && x->x_tonev[i].t_harmonic != 1 && x->x_tonev[i].t_harmonic % 2 == 1 )
          || ( op->selector == SEL_EVEN && x->x_tonev[i].t_harmonic % 2 == 0 )
          || ( op->selector == SEL_GT && x->x_tonev[i].t_harmonic > op->selector_arg )
          || ( op->selector == SEL_LT && x->x_tonev[i].t_harmonic < op->selector_arg )
                ) {
          match = 1;
      }
      ratio = 0.0;
      if ( x->x_fundamental_freq > 0.0 ) {
        ratio = x->x_tonev[i].t_freq / x->x_fundamental_freq;
      }
      if ( op->selector == SEL_GE && ratio >= op->selector_arg ) {
        match = 1;
      }
      if ( op->selector == SEL_LE && ratio <= op->selector_arg ) {
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
                amp = op->op_arg * x->x_fundamental_amp;
              break;
            }
          case WHAT_DB:
            switch ( op->op ) {
              case OP_SCALE:
                amp *= exp( ( LOGTEN * 0.05) * op->op_arg);
              break;
              case OP_REPLACE:
                amp = exp( ( LOGTEN * 0.05) * op->op_arg) * x->x_fundamental_amp;
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
    t_atom at[8];
    ratio = 0.0;
    // ratio of freq to fundamental
    if ( x->x_fundamental_freq > 0.0 ) {
      ratio = x->x_tonev[i].t_freq / x->x_fundamental_freq;
    }
    amp_ratio = 0.0;
    if ( x->x_fundamental_amp > 0.0 ) {
      amp_ratio = x->x_tonev[i].t_amp / x->x_fundamental_amp;
    }
    SETFLOAT(at, ratio);
    // harmonic number or -1 if not an integral mult of fundamental
    SETFLOAT(at+1, x->x_tonev[i].t_harmonic);
    SETFLOAT(at+2, amp_ratio) ;
    SETFLOAT(at+3, (t_float)i);
    SETFLOAT(at+4, freq);
    SETFLOAT(at+5, amp);
    SETFLOAT(at+6, x->x_tonev[i].t_newflag);
    SETFLOAT(at+7, x->x_tonev[i].t_score);
    outlet_list(x->x_out_two, &s_list, 7, at); 
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
  } else if ( what == gensym("db")) {
    op->what = WHAT_DB;
  } else {
    post("ERROR overtones transform first argument not recognized, valid args are freq and amp");
    return;
  }

  if ( ( op->what == WHAT_FREQ || op->what == WHAT_AMP ) && op_op == gensym("*")) {  
    op->op = OP_SCALE;
  } else if ( op->what == WHAT_DB && ( op_op == gensym("+") || op_op == gensym("-") ) ) {
    op->op = OP_SCALE;
  } else if ( op_op == gensym("=")) {
    op->op = OP_REPLACE;
  } else {
    post("ERROR overtones transform second argument not recognized, valid args are = and * for amp and freq; =, + and - for db");
    return;
  }

  op->op_arg = op_arg;
  if ( op_op == gensym("-") ) {
    op->op_arg *= -1.0;
  }

  if ( selector == gensym("=") ) {
    op->selector = SEL_EQ;
  } else if ( selector == gensym("odd") ) {
    op->selector = SEL_ODD;
  } else if ( selector == gensym("even") ) {
    op->selector = SEL_EVEN;
  } else if ( selector == gensym(">") ) {
    op->selector = SEL_GT;
  } else if ( selector == gensym(">=") ) {
    op->selector = SEL_GE;
  } else if ( selector == gensym("<") ) {
    op->selector = SEL_LT;
  } else if ( selector == gensym("<=") ) {
    op->selector = SEL_LE;
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
  int best_score_index = -1, change_fund_index;
  t_float max_amp = 0.0, min_fund_amp;
  t_float max_fund_score;
  t_float distance, score;
  int i, j;
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
  min_fund_amp = MIN_FUNDAMENTAL_AMP * max_amp;

  // find the peak with highest score with at least min_fund_amp
  //  use it as fundamental freq, label it 1
  // set default harmonic number -1 for all others

  for ( i = 0; i < x->x_ntone; i++) {
    // default all to outlier
    x->x_tonev[i].t_harmonic = -1;
    x->x_tonev[i].t_score = 0.0;
    // skip inactive peaks
    if ( x->x_tonev[i].t_newflag == -1 ) {
      continue;
    }
    // skip if amplitude is too low
    if ( x->x_tonev[i].t_amp < min_fund_amp ) {
      continue;
    }
    // calculate score - sum of amplitudes of positive non-outliers
    score = 0.0;
    for ( j = 0; j < x->x_ntone; j++) {
      // favor fundamental with high amp by counting my own amp double in score
      if ( i == j ) {
        score += 2.0 * x->x_tonev[j].t_amp;
        continue;
      }
      // skip inactive peaks
      if ( x->x_tonev[j].t_newflag == -1 ) {
        continue;
      }

      // skip frequencies less than octave + semitone below
      if ( x->x_tonev[j].t_freq < 0.471937156 * x->x_tonev[i].t_freq ) {
        continue;
      }

      harmonic = getHarmonic(x->x_tonev[j].t_freq,x->x_tonev[i].t_freq,0);
      distance = getDistanceToHarmonic( x->x_tonev[j].t_freq,x->x_tonev[i].t_freq,harmonic);
      //post("DEBUG freq-i = %f freq-j = %f harmonic = %i distance = %f",x->x_tonev[i].t_freq,x->x_tonev[j].t_freq,harmonic,distance);
      if ( distance < CLOSE_ENOUGH_SEMITONES ) {
        score += x->x_tonev[j].t_amp;
      }
    }
    //post("DEBUG i = %i score = %f freq = %f",i,score,x->x_tonev[i].t_freq);
    if ( best_score_index == -1
        || score > max_fund_score
        || ( score == max_fund_score && x->x_tonev[i].t_freq < x->x_tonev[best_score_index].t_freq )) {
      max_fund_score = score;
      best_score_index = i;
    }
    x->x_tonev[i].t_score = score;
  }
  // only change fundamental index if this one has had best score TOP_SCORE_HISTORY_LENGTH times in a row
  if ( best_score_index != x->x_fundamental_index ) {
    change_fund_index = 1;
    for ( i = 0; i < TOP_SCORE_HISTORY_LENGTH; i++ ) {
      if ( x->x_top_score_history[i] != -1 && x->x_top_score_history[i] != best_score_index ) {
        change_fund_index = 0;
      }
    }
    if ( change_fund_index ) {
      x->x_fundamental_index = best_score_index;
    }
  }
  for ( i = 0; i < TOP_SCORE_HISTORY_LENGTH; i++ ) {
    // rotate history
    if ( i < TOP_SCORE_HISTORY_LENGTH - 1 ) {
      x->x_top_score_history[i] = x->x_top_score_history[i + 1];
    } else {
      x->x_top_score_history[i] = best_score_index;
    }
  }

  if ( x->x_fundamental_index != -1 ) {
    x->x_tonev[x->x_fundamental_index].t_harmonic = 1;
    x->x_fundamental_freq = x->x_tonev[x->x_fundamental_index].t_freq;
    x->x_fundamental_amp = x->x_tonev[x->x_fundamental_index].t_amp;

    // ideantify each peak with near multiple of fundamental freq and label
    for ( i = 0; i < x->x_ntone; i++) {
      // skip fundamental
      if ( i == x->x_fundamental_index ) {
        continue;
      }
      // skip inactive tracks
      if ( x->x_tonev[i].t_newflag == -1 ) {
        continue;
      }
      harmonic = getHarmonic(x->x_tonev[i].t_freq, x->x_fundamental_freq,1);
      if ( getDistanceToHarmonic(x->x_tonev[i].t_freq, x->x_fundamental_freq, harmonic) < CLOSE_ENOUGH_SEMITONES ) {
        x->x_tonev[i].t_harmonic = harmonic;
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
    x->x_top_score_history = (int *)getbytes(TOP_SCORE_HISTORY_LENGTH * sizeof(int));
    for ( i = 0; i < TOP_SCORE_HISTORY_LENGTH; i++ ) {
      x->x_top_score_history[i] = -1;
    }
    x->x_fundamental_index = -1;
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