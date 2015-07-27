/****************************************************************************
 * FILE: seq.c
 * AUTHOR: William Stafford Noble
 * CREATE DATE: 06/24/2002
 * PROJECT: MHMM
 * DESCRIPTION: Biological sequences.
 * COPYRIGHT: 1998-2008, UCSD, UCSC, UW
 ****************************************************************************/
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include "utils.h"
#include "seq.h"

#define MAX_SEQ_NAME 100     // Longest sequence ID.
#define MAX_SEQ_COMMENT 128 // Longest comment.

// maximum count to keep track of when calculating a background
const static long BG_CALC_CHUNK = LONG_MAX;

// Instantiate the SEQ_T type.
struct seq {
  char  name[MAX_SEQ_NAME + 1];     // Sequence ID.
  char  desc[MAX_SEQ_COMMENT + 1];  // Sequence description.
  int   length;                 // Sequence length.
  int   offset;                 // Offset from the start of complete sequence.
  float weight;                 // Sequence weight.
  char* sequence;               // The actual sequence.
  int*  intseq;                 // The sequence in integer format.
  int*  gc;                     // Cumulative GC counts; note: 2Gb size limit.
  double total_gc;              // Total frequency of G and C in sequence; DNA only
  BOOLEAN_T is_complete;        // Is this the end of the sequence?
};

// For calculating the background
// of a truly huge dataset keep
// track of the result up to size
// long max and then store weights
// depending on how many chunks
// have been normalized into the
// final result
struct bgcalc {
  // the alphabet
  ALPH_T alph;
  // the number of bases seen so far
  // in this chunk
  long chunk_seen;
  // the counts of the individual
  // bases seen so far
  long *chunk_counts;
  // the background seen so far
  double *bg;
  // the weight of the background
  // seen so far. Note that 1 chunk
  // has the weight of 1.
  long weight;
};


/****************************************************************************
 * Allocate one sequence object.
 ****************************************************************************/
SEQ_T* allocate_seq
  (char* name,
   char* description,
   int   offset,
   char* sequence)
{
  SEQ_T* new_sequence;

  // Allocate the sequence object.
  new_sequence = (SEQ_T*)mm_malloc(sizeof(SEQ_T));

  // Store the name, truncating if necessary.
  strncpy(new_sequence->name, name, MAX_SEQ_NAME);
  new_sequence->name[MAX_SEQ_NAME] = '\0';
  if (strlen(new_sequence->name) != strlen(name)) {
    fprintf(stderr, "Warning: truncating sequence ID %s to %s.\n",
        name, new_sequence->name);
  }

  // Store the description, truncating if necessary.
  strncpy(new_sequence->desc, description, MAX_SEQ_COMMENT);
  new_sequence->desc[MAX_SEQ_COMMENT] = '\0';

  // Store the sequence.
  copy_string(&(new_sequence->sequence), sequence);

  // Fill in the remaining fields.
  new_sequence->length = strlen(sequence);
  new_sequence->offset = offset;
  new_sequence->weight = 1.0;
  new_sequence->intseq = NULL;
  new_sequence->gc = NULL;
  new_sequence->is_complete = TRUE;

  return(new_sequence);
}

/****************************************************************************
 * Get and set various fields.
 ****************************************************************************/
char* get_seq_name
  (SEQ_T* a_sequence)
{
  assert(a_sequence != NULL);
  return(a_sequence->name);
}

char* get_seq_description
  (SEQ_T* a_sequence)
{
  assert(a_sequence != NULL);
  return(a_sequence->desc);
}

int get_seq_length
  (SEQ_T* a_sequence)
{
  assert(a_sequence != NULL);
  return(a_sequence->length);
}

int get_seq_offset
  (SEQ_T* a_sequence)
{
  assert(a_sequence != NULL);
  return(a_sequence->offset);
}

float get_seq_weight
  (SEQ_T* a_sequence)
{
  assert(a_sequence != NULL);
  return(a_sequence->weight);
}

void set_seq_weight
  (float  weight,
   SEQ_T* a_sequence)
{
  assert(a_sequence != NULL);
  a_sequence->weight = weight;
}

unsigned char get_seq_char
  (int index,
   SEQ_T* a_sequence)
{
  assert(a_sequence != NULL);
  assert(a_sequence->sequence != NULL);
  assert(index >= 0);
  assert(index <= a_sequence->length);
  return(a_sequence->sequence[index]);
}

void set_seq_char
  (int    index,
   char   a_char,
   SEQ_T* a_sequence)
{
  assert(a_sequence != NULL);
  assert(a_sequence->sequence != NULL);
  assert(index >= 0);
  assert(index <= a_sequence->length);
  a_sequence->sequence[index] = a_char;
}

int get_seq_int
  (int index,
   SEQ_T* a_sequence)
{
  assert(a_sequence != NULL);
  assert(a_sequence->sequence != NULL);
  assert(index >= 0);
  assert(index < a_sequence->length);
  return(a_sequence->intseq[index]);
}

int get_seq_gc
  (int index,
   SEQ_T* a_sequence)
{
  assert(a_sequence != NULL);
  assert(a_sequence->sequence != NULL);
  assert(index >= 0);
  assert(index < a_sequence->length);
  return(a_sequence->gc[index]);
}

char* get_raw_sequence
  (SEQ_T* a_sequence)
{
  assert(a_sequence != NULL);
  return(a_sequence->sequence);
}

char* get_raw_subsequence
  (int start, int stop, SEQ_T* a_sequence)
{
  assert(a_sequence != NULL);
  assert((stop - start) >= 0);
  char *sequence = get_raw_sequence(a_sequence);
  char *subsequence = mm_malloc((stop - start + 2) * sizeof(char));
  strncpy(subsequence, sequence + start, stop - start + 1);
  subsequence[stop - start + 1] = 0;
  return(subsequence);
}

int* get_int_sequence
  (SEQ_T* a_sequence)
{
  assert(a_sequence != NULL);
  return(a_sequence->intseq);
}

int* get_gc_sequence
  (SEQ_T* a_sequence)
{
  assert(a_sequence != NULL);
  return(a_sequence->gc);
}

double get_total_gc_sequence
  (SEQ_T* a_sequence)
{
  assert(a_sequence != NULL);
  return(a_sequence->total_gc);
}

void set_total_gc_sequence
  (SEQ_T* a_sequence, double gc)
{
  assert(a_sequence != NULL);
  a_sequence->total_gc = gc;
}


/**************************************************************************
 * Copy a sequence object.  Memory must be freed by caller.
 **************************************************************************/
SEQ_T* copy_sequence
  (SEQ_T* source_sequence)
{
  // Allocate the sequence object.
  SEQ_T* new_sequence = allocate_seq(source_sequence->name,
				     source_sequence->desc,
				     source_sequence->offset,
				     source_sequence->sequence);

  // Copy additional fields.
  new_sequence->weight = source_sequence->weight;
  new_sequence->is_complete = source_sequence->is_complete;
  if (source_sequence->intseq != NULL) {
    new_sequence->intseq = (int*)mm_malloc(sizeof(int) * source_sequence->length);
    int i;
    for (i = 0; i < source_sequence->length; i++) {
      new_sequence->intseq[i] = source_sequence->intseq[i];
    }
  }
  if (source_sequence->gc != NULL) {
    new_sequence->gc = (int*)mm_malloc(sizeof(int) * source_sequence->length);
    int i;
    for (i = 0; i < source_sequence->length; i++) {
      new_sequence->gc[i] = source_sequence->gc[i];
    }
  }

  return(new_sequence);
}

/**************************************************************************
 * Sometimes a sequence object contains only a portion of the actual
 * sequence.  This function tells whether or not the end of this
 * sequence object corresponds to the end of the actual sequence.
 **************************************************************************/
void set_complete
  (BOOLEAN_T is_complete,
   SEQ_T* a_sequence)
{
  assert(a_sequence != NULL);
  a_sequence->is_complete = is_complete;
}

BOOLEAN_T is_complete
  (SEQ_T* a_sequence)
{
  assert(a_sequence != NULL);
  return(a_sequence->is_complete);
}


/***************************************************************************
 * Add or remove Xs from either side of the sequence.
 ***************************************************************************/
static void add_flanking_xs
  (SEQ_T* sequence, ALPH_T alph)
{
  char*  new_seq = NULL;         // Pointer to copy of the sequence.

  new_seq = (char*)mm_calloc(sequence->length + 3, sizeof(char));
  strcpy(&(new_seq[1]), sequence->sequence);

  new_seq[0] = alph_wildcard(alph);
  new_seq[sequence->length + 1] = alph_wildcard(alph);
  new_seq[sequence->length + 2] = '\0';

  myfree(sequence->sequence);
  sequence->sequence = new_seq;
  sequence->length += 2;
}

void remove_flanking_xs
  (SEQ_T* sequence)
{
  char*  new_seq;         // Copy of the sequence.

  new_seq = (char*)mm_calloc(sequence->length - 1, sizeof(char));
  strncpy(new_seq, &(sequence->sequence[1]), sequence->length - 2);
  new_seq[sequence->length - 2] = '\0';

  myfree(sequence->sequence);
  sequence->sequence = new_seq;
  sequence->length -= 2;
}

void allocate_int_sequence
  (SEQ_T* sequence)
{
  sequence->intseq = (int *)mm_malloc(sizeof(int) * sequence->length);
}

/**************************************************************************
 * Prepare a sequence for recognition by
 *  - making sure it is uppercase,
 *  - making sure it doesn't contain illegal characters,
 *  - adding flanking Xs to match START/END states, and
 *  - converting it to an integer format
 *  - computing cumulative GC counts
 *
 * In the integer form, each character in the sequence is replaced by
 * the index of that character in the alphabet array.  Thus, if the
 * alphabet is 'ACGT', every occurence of the letter 'G' in the
 * sequence will be represented by the index 2.
 **************************************************************************/
void prepare_sequence
  (SEQ_T* sequence, ALPH_T alph)
{
  int i_seq;        // Index in the sequence.
  int badchar;      // Number of characters converted.
  char wildcard;    // Wildcard character

  wildcard = alph_wildcard(alph);
  badchar = 0;

  for (i_seq = 0; i_seq < get_seq_length(sequence); i_seq++) {
    // Make sure the sequence is uppercase.
    if (islower((int)(sequence->sequence)[i_seq])) {
      (sequence->sequence)[i_seq] = toupper((int)(sequence->sequence)[i_seq]);
    }

    // Convert non-alphabetic characters to ambiguous.
    if (alph_index(alph, (sequence->sequence)[i_seq]) == -1) {
      fprintf(stderr, "%c -> %c\n", (sequence->sequence)[i_seq], wildcard);
      (sequence->sequence)[i_seq] = wildcard;
      badchar++;
    }
  }

  // Tell the user about the conversions.
  if (badchar) {
    fprintf(stderr, "Warning: converted %d non-alphabetic ", badchar);
    fprintf(stderr, "characters to %c in sequence %s.\n", wildcard, 
        get_seq_name(sequence));
  }

  // Add flanking X's.
  add_flanking_xs(sequence, alph);

  // Make the integer sequence.
  sequence->intseq = (int *)mm_malloc(sizeof(int) * get_seq_length(sequence));
  for (i_seq = 0; i_seq < get_seq_length(sequence); i_seq++) {
    (sequence->intseq)[i_seq]
      = alph_index(alph, (sequence->sequence)[i_seq]);
  }

  //
  // Get cumulative GC counts.
  //
  if (alph == DNA_ALPH) {
    int len = get_seq_length(sequence);
    char c = (sequence->sequence)[0];		// first character

    sequence->gc = (int *)mm_malloc(sizeof(int) * get_seq_length(sequence));

    // set count at first position
    (sequence->gc)[0] = (c == 'G' || c == 'C') ? 1 : 0;
    // set cumulative counts at rest of postitions
    for (i_seq = 1; i_seq < len; i_seq++) {
      c = (sequence->sequence)[i_seq];
      (sequence->gc)[i_seq] = (c == 'G' || c == 'C') ?
        (sequence->gc)[i_seq-1] + 1 : (sequence->gc)[i_seq-1];
    }
  }
}

/***************************************************************************
 * Remove the first N bases of a given sequence.
 ***************************************************************************/
void shift_sequence
  (int    offset,
   SEQ_T* sequence)
{
  char* smaller_sequence;

  // Make a smaller copy of the raw sequence.
  assert(offset > 0);
  assert(offset <= sequence->length);
  smaller_sequence = (char*)mm_malloc((sequence->length - offset + 1) * sizeof(char));
  strcpy(smaller_sequence, &(sequence->sequence[offset]));
  assert((int)strlen(smaller_sequence) == (sequence->length - offset));

  // Put the smaller sequence into the sequence object.
  myfree(sequence->sequence);
  sequence->sequence = smaller_sequence;
  sequence->offset += offset;
  sequence->length -= offset;

  // Free the integer version.
  myfree(sequence->intseq);
  sequence->intseq = NULL;

  // Free the GC counts.
  myfree(sequence->gc);
  sequence->gc = NULL;
}

/***************************************************************************
 * Get the maximum sequence length from a set of sequences.
 ***************************************************************************/
int get_max_seq_length
  (int     num_seqs,
   SEQ_T** sequences)
{
  int max_length;
  int this_length;
  int i_seq;

  max_length = 0;
  for (i_seq = 0; i_seq < num_seqs; i_seq++) {
    this_length = get_seq_length(sequences[i_seq]);
    if (this_length > max_length) {
      max_length = this_length;
    }
  }
  return(max_length);
}

/***************************************************************************
 * Get the maximum sequence ID length from a set of sequences.
 ***************************************************************************/
int get_max_seq_name
  (int     num_seqs,
   SEQ_T** sequences)
{
  int max_length;
  int this_length;
  int i_seq;

  max_length = 0;
  for (i_seq = 0; i_seq < num_seqs; i_seq++) {
    this_length = strlen(get_seq_name(sequences[i_seq]));
    if (this_length > max_length) {
      max_length = this_length;
    }
  }
  return(max_length);
}

/***************************************************************************
 * Set the sequence weights according to an external file.
 *
 * If the filename is "none," "internal," or NULL, then the weights are
 * set uniformly.
 ***************************************************************************/
void set_sequence_weights
  (char*    weight_filename, // Name of file containing sequence weights.
   int      num_seqs,        // Number of sequences.
   SEQ_T**  sequences)       // The sequences.
{
  ARRAY_T* weights;
  FILE *   weight_file;
  int      i_seq;

  /* Allocate the weights. */
  weights = allocate_array(num_seqs);

  /* Set uniform weights if no file was supplied. */
  if ((weight_filename == NULL) || (strcmp(weight_filename, "none") == 0) ||
      (strcmp(weight_filename, "internal") == 0)) {
    init_array(1.0, weights);
  }

  /* Read the weights from a file. */
  else {
    if (open_file(weight_filename, "r", FALSE, "weight", "weights",
		  &weight_file) == 0)
      exit(1);
    read_array(weight_file, weights);
    fclose(weight_file);

    /* Normalize the weights so they add to the total number of sequences. */
    normalize(0.0, weights);
    scalar_mult(num_seqs, weights);
  }

  /* Assign the weights to the sequences. */
  for (i_seq = 0; i_seq < num_seqs; i_seq++) {
    (sequences[i_seq])->weight = get_array_item(i_seq, weights);
  }

  /* Free the weights. */
  free_array(weights);
}


/****************************************************************************
 *  Return an array containing the frequencies in the sequence for each
 *  character of the alphabet. Characters not in the alphabet are not
 *  counted.
 ****************************************************************************/
ARRAY_T* get_sequence_freqs
  (SEQ_T* seq, ALPH_T alph)
{
  char c = 0;
  int a_index = 0;
  int a_size = 0;
  int i = 0;
  int total_bases = 0;
  int* num_bases = NULL;
  ARRAY_T* freqs = NULL;

  // Initialize counts for each character in the alphabet
  a_size = alph_size(alph, ALPH_SIZE);
  num_bases = mm_malloc(a_size * sizeof(int));
  for (i = 0; i < a_size; i++) {
    num_bases[i] = 0;
  }

  for (i = 0; i < seq->length; i++) {
    c = get_seq_char(i, seq);
    if (c != '-' && c != '-') {
      a_index = alph_index(alph, c);
      if (a_index == -1 || a_index >= a_size) continue;
      num_bases[a_index]++;
      total_bases++;
    }
  }

  freqs = allocate_array(a_size);
  for (i = 0; i < a_size; i++) {
    set_array_item(i, (double) num_bases[i] / (double) total_bases, freqs);
  }

  // Clean up the count of characters
  myfree(num_bases);

  return freqs;
}

/****************************************************************************
 *  Return an array containing the frequencies in the sequences for each
 *  character of the alphabet. Characters not in the alphabet are not
 *  counted.
 *
 *  When seq is provided it returns null, otherwise it converts the accumulated 
 *  result in bgcalc into a background.
 *
 *
 *  Pseudocode example:
 *    ALPH_T alph = ...
 *    BGCALC_T *bgcalc = NULL;
 *    for each seq:
 *      calculate_background(alph, seq, &bgcalc);
 *    ARRAY_T *bg = calculate_background(NULL, &bgcalc);
 ****************************************************************************/
ARRAY_T* calculate_background(
  ALPH_T alph,
  SEQ_T* seq,
  BGCALC_T** bgcalc
){
  BGCALC_T *calc;
  int a_size, i, a_index;
  char c;
  double freq, chunk_part, chunk_freq;
  ARRAY_T *background;
  assert(bgcalc != NULL);
  assert(seq != NULL || *bgcalc != NULL);
  // get the alphabet
  // get the alphabet size
  a_size = alph_size(alph, ALPH_SIZE);
  if (*bgcalc == NULL) {
    //allocate and initialize calc
    calc = mm_malloc(sizeof(BGCALC_T));
    calc->alph = alph;
    calc->chunk_seen = 0;
    calc->weight = 0;
    calc->chunk_counts = mm_malloc(a_size * sizeof(long));
    calc->bg = mm_malloc(a_size * sizeof(double));
    for (i = 0; i < a_size; ++i) {
      calc->chunk_counts[i] = 0;
      calc->bg[i] = 0;
    }
    *bgcalc = calc;
  } else {
    calc = *bgcalc;
    assert(alph == calc->alph);
    if (calc->weight == LONG_MAX) return NULL;
  }
  if (seq == NULL) {
    // no sequence so calculate the final result
    background = allocate_array(alph_size(alph, ALL_SIZE));
    if (calc->weight == 0) {
      if (calc->chunk_seen > 0) {
        // when we haven't had to approximate yet
        // just do a normal background calculation
        for (i = 0; i < a_size; i++) {
          freq = (double) calc->chunk_counts[i] / (double) calc->chunk_seen;
          set_array_item(i, freq, background);
        }
      } else {
        fputs("Uniform\n", stdout);
        // when there are no counts then return uniform
        freq = (double) 1 / (double) a_size;
        for (i = 0; i < a_size; i++) {
          set_array_item(i, freq, background);
        }
      }
    } else {
      if (calc->chunk_seen > 0) {
        // combine the frequencies for the existing chunks with the counts
        // for the partially completed chunk
        chunk_part = (double) calc->chunk_seen / (double) BG_CALC_CHUNK;
        for (i = 0; i < a_size; i++) {
          chunk_freq = (double) calc->chunk_counts[i] / 
              (double) calc->chunk_seen;
          freq = ((calc->bg[i] * calc->weight) + (chunk_freq * chunk_part)) / 
              (calc->weight + chunk_part);
          set_array_item(i, freq, background);
        }
      } else {
        // in the odd case we get to an integer number of chunks
        for (i = 0; i < a_size; i++) {
          set_array_item(i, calc->bg[i], background);
        }
      }
    }
    calc_ambigs(alph, FALSE, background);
    // free bgcalc structure
    free(calc->bg);
    free(calc->chunk_counts);
    free(calc);
    *bgcalc = NULL;
    return background;
  }
  // we have a sequence to add to the background calculation
  for (i = 0; i < seq->length; i++) {
    c = get_seq_char(i, seq);
    a_index = alph_index(alph, c);
    if (a_index == -1 || a_index >= a_size) continue;
    calc->chunk_counts[a_index]++;
    calc->chunk_seen++;
    if (calc->chunk_seen == BG_CALC_CHUNK) {
      if (calc->weight == 0) {
        for (i = 0; i < a_size; i++) {
          calc->bg[i] = (double) calc->chunk_counts[i] / (double) BG_CALC_CHUNK;
        }
      } else {
        for (i = 0; i < a_size; i++) {
          chunk_freq = (double) calc->chunk_counts[i] / (double) BG_CALC_CHUNK;
          calc->bg[i] = (calc->bg[i] * calc->weight + chunk_freq) / 
              (calc->weight + 1);
        }
      }
      calc->weight++;
      // reset the counts for the next chunk
      for (i = 0; i < a_size; i++) {
        calc->chunk_counts[i] = 0;
      }
      calc->chunk_seen = 0;
      // I don't think it is feasible to reach this limit
      // but I guess I'd better check anyway
      if (calc->weight == LONG_MAX) {
        fprintf(stderr, "Sequence data set is so large that even the "
            "approximation designed for large datasets can't handle it!");
        return NULL;
      }
    }
  }
  return NULL;
}

/**********************************************************************
  shuffle_sequence()

  shuffle a given sequences based on their content
**********************************************************************/
void shuffle_sequence(
  SEQ_T* seq,		/* original sequence IN */
  unsigned int seed,	/* seed IN */
  SEQ_T** target	/* target sequence OUT */
){
	my_srand(seed);
	assert(*target==NULL);
	// reset target if not null
	if (*target != NULL){
		free_seq(*target);
	}

	*target = allocate_seq(get_seq_name(seq),"shuffled",get_seq_offset(seq),get_raw_sequence(seq));
	char *raw = get_raw_sequence(*target);

	/* copy original in temp string */
	char* tmp = (char*)mm_calloc(get_seq_length(seq)+1,sizeof(char));
	strcpy(tmp,get_raw_sequence(seq));
	tmp[get_seq_length(seq)]='\0';

	int i,j;
	char *ss;
	char *dd;
	for(j=0,i=get_seq_length(seq);i>0;i--){
		// Pick a random number in the range:
		int pick = rand() % i;
		raw[j++] = tmp[pick];
		// "shift" routine here eliminates the "picked" base from the _src string:
		// dd starts at the picked position: ss is one beyond that:
		for( dd = tmp+pick , ss = dd + 1 ; *dd ; *dd++=*ss++ );
	}
	myfree(tmp);
}

/****************************************************************************
 * Free one sequence object.
 ****************************************************************************/
void free_seq
  (SEQ_T* a_sequence)
{
  if (a_sequence == NULL) {
    return;
  }
  myfree(a_sequence->sequence);
  myfree(a_sequence->intseq);
  myfree(a_sequence->gc);
  myfree(a_sequence);
}

