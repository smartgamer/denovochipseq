/********************************************************************
 * FILE: ame.c
 * AUTHOR: Robert McLeay
 * CREATE DATE: 19/08/2008
 * PROJECT: MEME suite
 * COPYRIGHT: 2008, Robert McLeay
 *
 * ame is a yet unpublished algorithm that seeks to assist in
 * determining whether a given transcription factor regulates a set
 * of genes that have been found by an experimental method that
 * provides ranks, such as microarray or ChIp-chip.
 *
 *
 ********************************************************************/

// we need this plus hash_alph.h to get globals allocated memory when building a backgroun
#define DEFINE_GLOBALS

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <libgen.h>
#include "matrix.h"
#include "alphabet.h"
#include "cisml.h"
#include "fasta-io.h"
#include "io.h"
#include "motif-in.h"
#include "string-list.h"
#include "simple-getopt.h"
#include "ranksum_test.h"
#include "motif.h"
#include "array.h"
#include "macros.h"
#include "hash_alph.h"
#include "background.h"
#include "motif_regexp.h"
#include "regress.h"
#include "spearman-rank-correlation.h"
#include "projrel.h"
#include "pssm.h"
#include "html-monolith.h"
#include "ame.h"
#include "ramen_scan.h"

#include "fisher_exact.h"


VERBOSE_T verbosity = NORMAL_VERBOSE;

static const double range = 100; // used for creating PSSMs
static const int gcbins = 1; // used for creating PSSMs

/*
 * Global Variables
 */

static rsc_motifs_t motifs;
static MATRIX_T* pssms;

static rsc_arg_t default_args;
static rsc_arg_t args;

static time_t  t0, t1; /* measuring time */
static clock_t c0, c1; /* measuring cpu_time */

static double** results;
static STRING_LIST_T* seq_ids;
static ARRAY_T* seq_fscores;
static double* factorial_array;

// maximum line length for output (set buffer size bigger than needed)
// no real global use of this, global for convenience: keep it that way
#define MAXLINE 1000
static char buffer [MAXLINE];

#define DEFAULTDIR "ame_out"

static char *default_output_dirname = DEFAULTDIR;  // default output directory name
static const char *text_filename = "ame.txt";
static const char *html_filename = "ame.html";
static const char *template_filename = "ame_template.html";

const char const *pvalue_method_names[] = {"ranksum", "fisher", "multihg", 
  "long_multihg", "linreg", "spearman"};
const char const *scoring_method_names[] = {"avg_odds", "max_odds", "sum_odds", 
  "total_hits"};
const char const *bg_format_names[] = {"uniform", "motif", "file"};

static rsc_result_t * init_result (rsc_result_t *new_result, // NULL => create a new one and return it
    int db_idx,
    MOTIF_T *motif,           // motif to copy into result
    int split,                // where to split between foreground, background
    double pleft,             // p-values for left, right and both tails
    double pright,
    double pboth,
    double u);

static void rsc_print_results (char *format, ...); // send output to the text file
static void final_print_results (); // finalise outputs and close files

static char * my_strdup(const char *s1); // returns "" string if passed a NULL pointer

/*************************************************************************
 * Initializations of parameters to defaults
 *************************************************************************/

void rsc_set_defaults() {
  default_args.bg_filename = NULL;
  default_args.outputdir = default_output_dirname;
  default_args.bg_format = MOTIF_BG;
  default_args.motif_filenames = NULL;
  default_args.number_motif_files = 0;
  default_args.sequence_filename = NULL;
  default_args.commandline = NULL;
  default_args.pseudocount = 0.25;
  default_args.scoring = TOTAL_HITS ; 
  default_args.verbose = NORMAL_VERBOSE;
  default_args.rs_method = QUICK_RS;
  default_args.positive_list = POS_FL;
  default_args.pvalue_method = FISHER_METHOD; 
  default_args.fisher_pwm_threshold = 1;
  default_args.fisher_fl_threshold = 1e-3;
  default_args.pvalue_threshold = 2e-4;
  default_args.pvalue_report_threshold = 1e-3;
  default_args.length_correction = FALSE;
  default_args.log_pwmscores = FALSE;
  default_args.log_fscores = FALSE;
  default_args.linreg_normalise = FALSE;
  default_args.linreg_dump_dir = 0;
  default_args.linreg_switchxy = TRUE;
  default_args.clobber = FALSE; // don't nuke output directory
  default_args.silent = FALSE;
  default_args.fix_partition = -1; //i.e. disabled.
  default_args.text_output = NULL;
  default_args.html_output = NULL;
  default_args.json_output = NULL;
  //Now copy the defaults into the real args
  memcpy(&args, &default_args, sizeof(rsc_arg_t));
}


/*************************************************************************
 * Entry point for ame
 *************************************************************************/
int main(int argc, char *argv[]) {

  int i;

  char *version_message = "AME (Analysis of Motif Enrichment): Compiled on " __DATE__ " at " __TIME__ "\n"
      "------------------------------\n"
      "Copyright (c) Robert McLeay <r.mcleay@imb.uq.edu.au> & Timothy Bailey <t.bailey@imb.uq.edu.au>, 2009.\n\n";

  /* Record the execution start and end times */
  if (verbosity >= HIGH_VERBOSE) {
    t0 = time(NULL);
    c0 = clock();
  }

  rsc_set_defaults(); //Set default cmd line args
  rsc_getopt(argc, argv); //Get command line args

  // skip this stuff if testing: don't really care if paths change or compiled on a different date
  if (!args.silent) {
    rsc_print_results ("%s", version_message);
    rsc_print_results ("Command line\n%s\n\n", args.commandline);
  }

  if (args.fix_partition > 0) {
    rsc_print_results("Not in partition maximisation mode. "
        "Fixing partition at %i.\n\n", args.fix_partition);
  } else {
    rsc_print_results("In partition maximisation mode.\n\n");
  }

  rsc_print_results("Threshold p-value for reporting results: %g\n", args.pvalue_report_threshold);

  rsc_load_motifs(); //Load the motifs from the file
  rsc_scan_sequences(); //Load each sequence, scanning with each motif to determine score

  if (args.pvalue_method != RANKSUM_METHOD) {
    fisher_exact_init(get_num_strings(seq_ids)); // Generate the table of log factorials
    factorial_array = fisher_exact_get_log_factorials();
  }

  rsc_get_scores(); //Do the significance tests to associate each motif with the set.

  //print_cismlq(stdout, output);

  rsc_terminate(0); //Successfully end.

  return 0; //shuts up a warning.
}

// send line unaltered to the text output
void rsc_print_results (char* format, ...) {
  va_list arg_ptrs;
  va_start(arg_ptrs, format);
  vfprintf(args.text_output, format, arg_ptrs);
  va_end(arg_ptrs);
}

static void record_result(rsc_result_t* result, double corrected_pvalue) {
  int i, j, len, alen;
  MATRIX_T *freqs;
  ARRAY_T *row;
  jsonwr_start_object_value(args.json_output);
  jsonwr_lng_prop(args.json_output, "db", result->db_idx);
  jsonwr_str_prop(args.json_output, "id", get_motif_id(result->motif));
  if (get_motif_id2(result->motif)[0] != '\0')
    jsonwr_str_prop(args.json_output, "alt", get_motif_id2(result->motif));
  jsonwr_lng_prop(args.json_output, "len", get_motif_length(result->motif));
  jsonwr_dbl_prop(args.json_output, "motif_evalue", get_motif_evalue(result->motif));
  jsonwr_dbl_prop(args.json_output, "motif_nsites", get_motif_nsites(result->motif));
  if (has_motif_url(result->motif)) 
    jsonwr_str_prop(args.json_output, "url", get_motif_url(result->motif));
  alen = alph_size(get_motif_alph(result->motif), ALPH_SIZE);
  len = get_motif_length(result->motif);
  freqs = get_motif_freqs(result->motif);
  jsonwr_property(args.json_output, "pwm");
  jsonwr_start_array_value(args.json_output);
  for (i = 0; i < len; i++) {
    row = get_matrix_row(i, freqs);
    jsonwr_start_array_value(args.json_output);
    for (j = 0; j < alen; j++) {
      jsonwr_dbl_value(args.json_output, get_array_item(j, row));
    }
    jsonwr_end_array_value(args.json_output);
  }
  jsonwr_end_array_value(args.json_output);
    
  if (args.fix_partition <= 0) 
    jsonwr_lng_prop(args.json_output, "split", result->split);
  if (args.pvalue_method == LINREG_METHOD) {
    jsonwr_dbl_prop(args.json_output, "mean_square_error", result->pboth);
    jsonwr_dbl_prop(args.json_output, "m", result->pleft);
    jsonwr_dbl_prop(args.json_output, "b", result->pright);
  } else if (args.pvalue_method == SPEARMAN_METHOD) {
    jsonwr_dbl_prop(args.json_output, "spearmans_rho", result->pboth);
  } else { // RANKSUM_METHOD, FISHER_METHOD, MULTIHG_METHOD, LONG_MULTIHG_METHOD
    jsonwr_dbl_prop(args.json_output, "pvalue", result->pright);
    jsonwr_dbl_prop(args.json_output, "corrected_pvalue", corrected_pvalue);
  }
  jsonwr_end_object_value(args.json_output);
}

// close the output files
static void final_print_results () {
  fclose(args.text_output);
  if (args.html_output) {
    if (htmlwr_output(args.html_output) != NULL) {
      die("Expected only one replacement variable in template.\n");
    }
    htmlwr_destroy(args.html_output);
    args.html_output = NULL;
  }
}


void rsc_getopt(int argc, char *argv[]) {
  const int num_options = 23; // change this if the number of options changes
  cmdoption const motif_scan_options[] = {
    {"bgfile", REQUIRED_VALUE},
    {"bgformat", REQUIRED_VALUE},
    {"o", REQUIRED_VALUE},
    {"oc", REQUIRED_VALUE},
    {"pseudocount", REQUIRED_VALUE},
    {"fl-threshold", REQUIRED_VALUE},
    {"pwm-threshold", REQUIRED_VALUE},
    {"pvalue-threshold", REQUIRED_VALUE},
    {"pvalue-report-threshold", REQUIRED_VALUE},
    {"scoring", REQUIRED_VALUE},
    {"method", REQUIRED_VALUE},
    {"rsmethod", REQUIRED_VALUE},
    {"poslist", REQUIRED_VALUE},
    {"verbose", REQUIRED_VALUE},
    {"silent", NO_VALUE},
    {"length-correction", NO_VALUE},
    {"linreg-dumpdir", REQUIRED_VALUE},
    {"linreg-switchxy", NO_VALUE},
    {"log-fscores", NO_VALUE},
    {"log-pwmscores", NO_VALUE},
    {"normalise-affinity", NO_VALUE},
    {"fix-partition", REQUIRED_VALUE},
    {"help", NO_VALUE},
  };

  int option_index = 0;
  char* option_name = NULL;
  char* option_value = NULL;
  const char * message = NULL;
  BOOLEAN_T bad_options = FALSE;
  int i;

  if (simple_setopt(argc, argv, num_options, motif_scan_options) != NO_ERROR) {
    die("Error processing command line options: option name too long.\n");
  }

  /*
   * Now parse the command line options
   */
  //simple_getopt will return 0 when there are no more options to parse
  while(simple_getopt(&option_name, &option_value, &option_index) > 0) {
    //fprintf(stderr, "Option name: %s || %s\n", option_name, option_value);
    if (args.verbose >= HIGHER_VERBOSE) {
      fprintf(stderr, "ame Option: %s - %s\n",option_name, option_value);
    }
    if (strcmp(option_name, "bgfile") == 0) {
      args.bg_filename = option_value;
    } else if (strcmp(option_name, "o") == 0) {
      args.outputdir = option_value;
    } else if (strcmp(option_name, "oc") == 0) {
      args.outputdir = option_value;
      args.clobber = TRUE;
    } else if (strcmp(option_name, "bgformat") == 0) {
      if (atoi(option_value)==MOTIF_BG) {
        args.bg_format = MOTIF_BG;
      } else if (atoi(option_value)==FILE_BG) {
        args.bg_format = FILE_BG;
      } else if (atoi(option_value)==UNIFORM_BG) {
        args.bg_format = UNIFORM_BG;
      } else {
        rsc_usage();
        rsc_terminate(1);
      }

    } else if (strcmp(option_name, "rsmethod") == 0) {
      if (strcmp(option_value,"quick")==0) {
        args.rs_method = QUICK_RS;
        fprintf(stderr, "Using quick ranksum calculation method.\n");
      } else if (strcmp(option_value,"better") == 0) {
        args.rs_method = BETTER_RS;
        fprintf(stderr, "Using more accurate ranksum calculation method.\n");
      } else {
        rsc_usage();
        rsc_terminate(1);
      }
    } else if (strcmp(option_name, "pseudocount") == 0) {
      args.pseudocount = atof(option_value);
    } else if (strcmp(option_name, "pwm-threshold") == 0) {
      args.fisher_pwm_threshold = atof(option_value);
    } else if (strcmp(option_name, "fl-threshold") == 0) {
      args.fisher_fl_threshold = atof(option_value);
    } else if (strcmp(option_name, "pvalue-threshold") == 0) {
      args.pvalue_threshold = atof(option_value);
    } else if (strcmp(option_name, "pvalue-report-threshold") == 0) {
      args.pvalue_report_threshold = atof(option_value);
    } else if (strcmp(option_name, "scoring") == 0) {
      if (strcmp(option_value,"avg")==0) {
        args.scoring = AVG_ODDS;
        fprintf(stderr, "Using average odds scoring.\n");
      } else if (strcmp(option_value,"sum") == 0) {
        args.scoring = SUM_ODDS;
        fprintf(stderr, "Using sum of odds scoring.\n");
      } else if (strcmp(option_value,"max") == 0) {
        args.scoring = MAX_ODDS;
        fprintf(stderr, "Using maximum odds scoring.\n");
      } else if (strcmp(option_value,"totalhits") == 0) {
        args.scoring = TOTAL_HITS;
        fprintf(stderr, "Using total hits scoring.\n");
      } else {
        rsc_usage();
        rsc_terminate(1);
      }
    } else if (strcmp(option_name, "poslist") == 0) {
      if (strcmp(option_value,"fl")==0) {
        args.positive_list = POS_FL;
        fprintf(stderr, "Using fluorescence as positive.\n");
      } else if (strcmp(option_value,"pwm") == 0) {
        args.positive_list = POS_PWM;
        fprintf(stderr, "Using PWM scores as positive.\n");
      } else {
        rsc_usage();
        rsc_terminate(1);
      }
    } else if (strcmp(option_name, "method") == 0) {
      if (strcmp(option_value,"ranksum")==0) {
        args.pvalue_method = RANKSUM_METHOD;
      } else if (strcmp(option_value,"fisher") == 0) {
        args.pvalue_method = FISHER_METHOD;
      } else if (strcmp(option_value,"mhg") == 0) {
        args.pvalue_method = MULTIHG_METHOD;
      } else if (strcmp(option_value,"4dmhg") == 0) {
        args.pvalue_method = LONG_MULTIHG_METHOD;
      } else if (strcmp(option_value,"linreg") == 0) {
        args.pvalue_method = LINREG_METHOD;
      } else if (strcmp(option_value,"spearman") == 0) {
        args.pvalue_method = SPEARMAN_METHOD;
      } else {
        rsc_usage();
        rsc_terminate(1);
      }
    } else if (strcmp(option_name, "verbose") == 0) {
      args.verbose = atoi(option_value);
      verbosity = args.verbose;
      if (args.verbose <= INVALID_VERBOSE || args.verbose > DUMP_VERBOSE) {
        rsc_usage();
        rsc_terminate(1);
      }
    } else if (strcmp(option_name, "silent") == 0) {
      args.silent = TRUE; // omit from usage message: for generating test output
    } else if (strcmp(option_name, "length-correction") == 0) {
      fprintf(stderr, "Enabling length correction mode for multiHG test.\n");
      args.length_correction = TRUE;
    } else if (strcmp(option_name, "log-fscores") == 0) {
      args.log_fscores = TRUE;
    } else if (strcmp(option_name, "log-pwmscores") == 0) {
      args.log_pwmscores = TRUE;
    } else if (strcmp(option_name, "normalise-affinity") == 0) {
      args.linreg_normalise = TRUE;
      fprintf(stderr, "Normalising per-motif scores.\n");
    } else if (strcmp(option_name, "linreg-dumpdir") == 0) {
      args.linreg_dump_dir = option_value;    
    } else if (strcmp(option_name, "fix-partition") == 0) {
      args.fix_partition = atoi(option_value);
    } else if (strcmp(option_name, "linreg-switchxy") == 0) {
      args.linreg_switchxy = FALSE;
    } else if (strcmp(option_name, "help") == 0) {
      fprintf(stderr, "%s",rsc_get_usage()); //was not to stderr
      rsc_terminate(0);
    } else {
      fprintf(stderr, "Error: %s is not a recognised switch.\n", option_name);
      rsc_usage();
      rsc_terminate(1);
    }

    option_index++;
  }

  if (args.pvalue_method == LINREG_METHOD || args.pvalue_method == SPEARMAN_METHOD) {
    if (args.linreg_switchxy) {
      fprintf(stderr, "In LR/Spearman mode, x=PWM, y=fluorescence\n");
    } else {
      //Standard mode
      fprintf(stderr, "In LR/Spearman mode, x=fluorescence, y=PWM\n");
    }
  }

  // Must have sequence and motif files
  if (argc < option_index + 2) {
    fprintf(stderr, "Error: Must specify a sequence file and motif file(s).\n");
    rsc_usage();
    rsc_terminate(1);
  }
  args.sequence_filename = argv[option_index];
  args.motif_filenames = &argv[option_index+1]; // FIXME: must now iterate until argc getting all motif DB filenames
  args.number_motif_files = argc-option_index-1;
  if (args.bg_format == MOTIF_BG) { //bgfile is the same as the motif file.
    //TODO: fix ?? FIXME --  motif file name not used so no problem changing to motif DB of multiple files
    args.bg_filename = NULL;
  }

  /* Now validate the options.
   *
   * Illegal combinations are:
   *   - sequence bg and no sequence
   *   - no motif
   *   - no sequences
   *   - each file exists.
   */

  if (args.motif_filenames == NULL) {
    fprintf(stderr, "Error: Motif file not specified.\n");
    bad_options = TRUE;
  } else {
    int i;
    for (i = 0; i < args.number_motif_files; i++) {
      if (!file_exists(args.motif_filenames[i])) {
        fprintf(stderr, "Error: Specified motif '%s' file does not exist.\n", args.motif_filenames[i]);
        bad_options = TRUE;
      }
    }
  }
  if (args.sequence_filename == NULL) {
    fprintf(stderr, "Error: Sequence file not specified.\n");
    bad_options = TRUE;
  } else if (!file_exists(args.sequence_filename)) {
    fprintf(stderr, "Error: Specified sequence file does not exist.\n");
    bad_options = TRUE;
  }

  if (args.positive_list == POS_PWM && args.rs_method == BETTER_RS) {
    fprintf(stderr, "Error: Better RS method cannot be combined with PWM positive list option.\n");
    bad_options = TRUE;
  }

  if (args.pvalue_method == MULTIHG_METHOD && args.scoring != TOTAL_HITS) {
    fprintf(stderr, "Error: Must use total hits with multihg-method.\n");
    bad_options = TRUE;
  }

  if (bad_options) {
    rsc_usage();
    rsc_terminate(1);
  }
  // make enough space for all the command line args, with one space between each
  int line_length = 0;
  for (i = 0; i < argc; i++)
    line_length += strlen(i == 0 ? basename(argv[0]) : argv[i]);
  // add on argc to allow one char per word for separating space + terminal '\0'
  args.commandline = (char*) malloc(sizeof(char)*(line_length+argc));
  int nextpos = 0;
  for (i = 0; i < argc; i++) {
    // been here before? put in a space before adding the next word
    if (nextpos) {
      args.commandline[nextpos] = ' ';
      nextpos ++;
    }
    char * nextword = i == 0 ? basename(argv[0]) : argv[i];
    strcpy(&args.commandline[nextpos], nextword);
    nextpos += strlen (nextword);
  }
  if (create_output_directory(args.outputdir, args.clobber, 
        args.verbose > QUIET_VERBOSE)) { // only warn in higher verbose modes
    fprintf(stderr, "failed to create output directory `%s' or already exists\n", args.outputdir);
    exit(1);
  }
  char *path = make_path_to_file(args.outputdir, text_filename);
  args.text_output = fopen(path, "w"); //FIXME check for errors: MEME doesn't either and we at least know we have a good directory
  myfree(path);
  // setup the html output writer 
  args.html_output = htmlwr_create(get_meme_etc_dir(), template_filename);
  htmlwr_set_dest_name(args.html_output, args.outputdir, html_filename);
  htmlwr_replace(args.html_output, "ame_data.js", "data");
  args.json_output = htmlwr_output(args.html_output);
  // write out some information
  jsonwr_str_prop(args.json_output, "version", VERSION);
  jsonwr_str_prop(args.json_output, "revision", REVISION);
  jsonwr_str_prop(args.json_output, "release", ARCHIVE_DATE);
  jsonwr_str_array_prop(args.json_output, "cmd", argv, argc);
  // options
  jsonwr_property(args.json_output, "options");
  jsonwr_start_object_value(args.json_output);
  jsonwr_str_prop(args.json_output, "scoring", scoring_method_names[args.scoring]);
  jsonwr_str_prop(args.json_output, "rs_method", (args.rs_method ? "better" : "quick"));
  jsonwr_str_prop(args.json_output, "positive_list", (args.positive_list ? "pwm" : "fluorescence"));
  jsonwr_str_prop(args.json_output, "pvalue_method", pvalue_method_names[args.pvalue_method]);
  jsonwr_dbl_prop(args.json_output, "pseudocount", args.pseudocount);
  jsonwr_dbl_prop(args.json_output, "fisher_pwm_threshold", args.fisher_pwm_threshold);
  jsonwr_dbl_prop(args.json_output, "fisher_fl_threshold", args.fisher_fl_threshold);
  jsonwr_dbl_prop(args.json_output, "pvalue_threshold", args.pvalue_threshold);
  jsonwr_dbl_prop(args.json_output, "pvalue_report_threshold", args.pvalue_report_threshold);
  jsonwr_bool_prop(args.json_output, "length_correction", args.length_correction);
  jsonwr_bool_prop(args.json_output, "log_fscores", args.log_fscores);
  jsonwr_bool_prop(args.json_output, "log_pwmscores", args.log_pwmscores);
  jsonwr_bool_prop(args.json_output, "linreg_normalise", args.linreg_normalise);
  jsonwr_bool_prop(args.json_output, "linreg_switchxy", args.linreg_switchxy);
  jsonwr_bool_prop(args.json_output, "fix_partition", (args.fix_partition > 0));
  if (args.fix_partition > 0) 
    jsonwr_lng_prop(args.json_output, "partition", args.fix_partition);
  jsonwr_end_object_value(args.json_output);
}

const char* rsc_get_usage() {
  // Define the usage message.
  //TODO: sprintf in default values
  return
    "USAGE: ame [options] <sequence file> <motif file>+\n"
    "   Key Options:\n"
    "     --method  [fisher|mhg|4dmhg|ranksum|linreg|spearman] Select the association function for motif significance\n"
    "     --scoring [avg|max|sum|totalhits]                    Motif-to-sequence affinity function:\n"
    "\n"
    "         Hints:   Use avg (recommended), sum, or max for ranksum, linreg, spearman methods.\n"
    "                  Use totalhits for fisher, mhg, 4dmhg (and possibly other) methods.\n"
    "\n"
    "     --fix-partition <int>\n"
    "                            Number of positive sequences; the balance are\n"
    "                            used as the background\n"
    "                            ** Only applies to the Fisher, mHG and 4D-mHG tests **\n"
    "\n"
    "   Output options:\n"
    "     --o  <output dir>       name of directory for output files will not\n"
    "                             replace existing directory\n"
    "     --oc <output dir>       name of directory for output files will\n"
    "                             replace existing directory\n"
    "     default: " DEFAULTDIR " will not replace existing directory\n"
    "   File format options:\n"
    "     --bgformat [0|1|2]      Source for determining background frequencies\n"
    "                                           0: uniform background\n"
    "                                           1: MEME motif file\n"
    "                                           2: Background file\n"
    "     --bgfile <background>   File containing background frequencies\n"
    "     --pvalue-report-threshold <float, default=1e-3>\n"
    "                            Corrected p-value threshold for reporting a motif\n"
    "\n"
    "   Ranksum-specific options:\n"
    "     --rsmethod [better|quick]     Whether to use a slower and more accurate ranksum method or a quicker one\n"
    "     --poslist  [fl (default)|pwm] For partition max., threshold on either X (pwm) or Y (fluorescence)\n"
    "\n"
    "   LR- and Spearman- specific options:\n"
    "     --log-fscores           Regress on the log_e of the fluorescence scores\n"
    "     --log-pwmscores         Regress on the log_e of the PWM scores\n"
    "     --normalise-affinity    Normalise the motif scores so that the motifs are comparable\n"
    "     --linreg-switchxy       Make the x-points fluorescence scores and the y-points PWM scores\n"
    "\n"
    "   Fisher, MHG, 4D-MHG, Ranksum in TOTALHITS affinity mode options:\n"
    "     --length-correction                               Correct for length bias by subtracting expected hits\n"
    "     --pvalue-threshold <float, default=2e-4>          Threshold to consider a single motif hit significant\n"
    "\n"
    "   Fisher Test with either AVG or MAX affinity (undefined results in TOTALHITS mode) options:\n"
    "     --fl-threshold  <float, default=1e-3> (Requires --poslist fl)  Max fluorescence p-value to consider a 'positive'\n"
    "     --pwm-threshold <float, default=1>    (Requires --poslist pwm) Min PWM score to call a sequence a 'positive'\n"
    "     --poslist       [fl (default)|pwm]    For partition max., threshold on either X (pwm) or Y (fluorescence)\n"
    "\n"
    "          Hints: Be careful when switching the poslist. In the case of the Fisher test, it switches between\n"
    "                 using X and Y for determining true positives in the contingency matrix, in addition to switching\n"
    "                 which of X and Y is used for partition maximisation.\n"
    "\n"
    "   Miscellaneous Options:\n"
    "     --pseudocount <float, default = 0.25> Pseudocount for motif affinity scan\n"
    "     --verbose     <1...5>                 Integer describing verbosity. Best used as first argument in list.\n"
    "     --help                                Show this message again\n"
    "\n"
    "   Notes:\n"
    "     By default, this tool performs unconstrained partition maximisation. With verbose set to 3, however, it will\n"
    "     output a constrained partition maximisation score for each sequence in the input set. To generate the fluorescence\n"
    "     based constrained partition maximisation plots in the paper, a Perl script was used to convert sequence numbers\n"
    "     into fluorescence thresholds from the verbose output.\n"
    "\n"
    "     Additionally, the linreg mode in this tool reports raw values only. For the full approach used in the paper,\n"
    "     please use the tool RAMEN, which was also included in this download. RAMEN also supports simulation of p-values.\n"
    "\n"
    "   WARNING:\n"
    "     This tool will not resort input sequences. It assumes that input FastA files are sorted from most-likely to be bound\n"
    "     to least likely to be bound in descending order.\n" 
    "\n"
    "   Miscellaneous Options:\n"
    "     --verbose     <1...5>  Integer describing verbosity. Best placed first\n"
    "     --help                 Show this message again\n"
    "\n"
    "   Citing AME:\n"
    "     If AME is of use to you in your research, please cite:\n"
    "\n"
    "          Robert C. McLeay, Timothy L. Bailey (2009).\n"
    "          \"Motif Enrichment Analysis: a unified framework and an evaluation\n"
    "          on ChIP data.\"\n"
    "          BMC Bioinformatics 2010, 11:165, doi:10.1186/1471-2105-11-165.\n"
    "\n"
    "   Contact the authors:\n"
    "     You can contact the authors via email:\n"
    "\n"
    "         Robert McLeay <r.mcleay@imb.uq.edu.au>, and\n"
    "         Timothy Bailey <t.bailey@imb.uq.edu.au>.\n"
    "\n"
    "     Bug reports should be directed to Robert McLeay.\n"
    "\n";

  /*

     "\n"
     "   File format options:\n"
     "     --bgformat     [0|1|2]            Source used to determine background frequencies\n"
     "                                           0: uniform background\n"
     "                                           1: MEME motif file\n"
     "                                           2: Background file\n"
     "     --bgfile       <background>       File containing background frequencies\n"

     "\n"
     "   Citing ame:\n"
     "     If ame is of use to you in your research, please cite:\n"
     "\n"
     "          Robert C. McLeay, Timothy L. Bailey (2009).\n"
     "          \"Motif Enrichment Analysis: a unified framework and an evaluation on ChIP data.\"\n"
     "          BMC Bioinformatics 2010, 11:165, doi:10.1186/1471-2105-11-165.\n"
     "\n"
     "   Contact the authors:\n"
     "     You can contact the authors via email:\n"
     "\n"
     "         Robert McLeay <r.mcleay@imb.uq.edu.au>, and\n"
     "         Timothy Bailey <t.bailey@imb.uq.edu.au>.\n"
     "\n"
     "     Bug reports should be directed to Robert McLeay.\n"
     "\n";
     */
}

void rsc_usage() {
  fprintf(stderr, "%s", rsc_get_usage());
}

/*
 * TODO: This code taken from ama.c - let's rewrite it properly.
 */
void rsc_load_motifs() {
  ARRAYLST_T* read_motifs;
  MREAD_T *mread;
  int num_motifs_before_rc = 0;
  int i, j, before_count, db_size;
  int *db_sizes;
  char *bg_src;
  ALPH_T alph;

  db_sizes = malloc(sizeof(int) * args.number_motif_files);
  before_count = 0;

  switch (args.bg_format) {
    case UNIFORM_BG:
      bg_src = "--uniform--";
      break;
    case MOTIF_BG:
      bg_src = "--motif--";
      break;
    case FILE_BG:
      bg_src = args.bg_filename;
      break;
    default:
      die("Illegal bg_format value.\n");
      bg_src = NULL; // make compiler happy
  }

  // write motif dbs to html data section
  jsonwr_property(args.json_output, "motif_dbs");
  jsonwr_start_array_value(args.json_output);
  // clear the motifs structure
  memset(&motifs, 0, sizeof(rsc_motifs_t));
  read_motifs = arraylst_create();
  for (i = 0; i < args.number_motif_files; i++) {
    mread = mread_create(args.motif_filenames[i], OPEN_MFILE);
    if (i == 0) mread_set_bg_source(mread, bg_src);
    else mread_set_background(mread, motifs.bg_freqs);
    mread_set_pseudocount(mread, args.pseudocount);

    mread_load(mread, read_motifs);
    if (i == 0) motifs.bg_freqs = mread_get_background(mread);

    mread_destroy(mread);
    // keep track of the motif counts
    db_size = arraylst_size(read_motifs) - before_count;
    db_sizes[i] = db_size * 2; // double for RC motifs
    before_count = arraylst_size(read_motifs);
    // write motif db to html data
    jsonwr_start_object_value(args.json_output);
    jsonwr_str_prop(args.json_output, "source", args.motif_filenames[i]);
    jsonwr_lng_prop(args.json_output, "count", db_size);
    jsonwr_end_object_value(args.json_output);
  }
  // finish writing motif dbs
  jsonwr_end_array_value(args.json_output);
  // write background to json data
  alph = (arraylst_size(read_motifs) > 0 ? 
      get_motif_alph((MOTIF_T*)arraylst_get(0, read_motifs)) : 
      INVALID_ALPH);
  jsonwr_property(args.json_output, "background");
  jsonwr_start_object_value(args.json_output);
  jsonwr_str_prop(args.json_output, "alphabet", alph_name(alph));
  jsonwr_str_prop(args.json_output, "source", bg_format_names[args.bg_format]);
  if (args.bg_format == FILE_BG)
    jsonwr_str_prop(args.json_output, "file", args.bg_filename);
  jsonwr_property(args.json_output, "frequencies");
  jsonwr_start_array_value(args.json_output);
  for (i = 0; i < alph_size(alph, ALPH_SIZE); i++) {
    jsonwr_dbl_value(args.json_output, get_array_item(i, motifs.bg_freqs));
  }
  jsonwr_end_array_value(args.json_output);
  jsonwr_end_object_value(args.json_output);

  // reverse complement the originals adding to the original read in list
  num_motifs_before_rc = arraylst_size(read_motifs);
  add_reverse_complements(read_motifs);        

  //Allocate array for the motifs
  motif_list_to_array(read_motifs, &(motifs.motifs), &(motifs.num));
  //free the list of motifs
  free_motifs(read_motifs);
  
  // check reverse complements.
  assert(motifs.num / 2 == num_motifs_before_rc); 

  // Allocate a pssm, odds matrix and db index for each
  // including reverse complements.
  motifs.pssms = malloc(sizeof(PSSM_T) * motifs.num);
  motifs.odds = malloc(sizeof(MATRIX_T*) * motifs.num); 
  motifs.db_idx = malloc(sizeof(int) * motifs.num);
  memset(motifs.pssms, 0, sizeof(PSSM_T) * motifs.num);
  memset(motifs.odds, 0, sizeof(MATRIX_T*) * motifs.num);
  memset(motifs.db_idx, 0, sizeof(int) * motifs.num);

  //Allocate our pv_lookup space if required.
  if (TOTAL_HITS == args.scoring) {
    motifs.pv_lookup = malloc(sizeof(ARRAY_T*) * num_motifs_before_rc); 
  }

  // We need to create odds matrices if we're doing AMA-type scoring
  // Or, we create PSSMs and pv_lookup tables for the total-hits style instead.
  for (i = 0, j = 0, before_count = 0; i < motifs.num; i++) {
    // setup db index tracking
    if ((i - before_count) >= db_sizes[j]) {
      before_count += db_sizes[j];
      j++;
    }
    motifs.db_idx[i] = j;
    // create odds or log-odds matrix
    if (TOTAL_HITS == args.scoring) {
      // Build and scale PSSM for each motif
      PSSM_T *newpssm = build_motif_pssm(motif_at(motifs.motifs, i),
          motifs.bg_freqs, motifs.bg_freqs, NULL, 1.0, range, gcbins, FALSE);
      motifs.pssms[i] = *newpssm;
      if (i % 2 != 0) {
        assert(get_num_rows(motifs.pssms[i].matrix) == get_num_rows(motifs.pssms[i-1].matrix)); 
      }
    } else {
      //convert_to_odds_matrix(motif_at(motifs.motifs, i), motifs.bg_freqs);
      motifs.odds[i] = create_odds_matrix(motif_at(motifs.motifs, i), motifs.bg_freqs);
      if (i % 2 != 0) {
        assert(get_num_rows(motifs.odds[i]) == get_num_rows(motifs.odds[i-1])); 
      }
    }
  }
  // cleanup
  free(db_sizes);
  // reset motif count to before rev comp
  motifs.num = num_motifs_before_rc;
}

void rsc_scan_sequences() {
  FILE* seq_file = NULL;
  SEQ_T* sequence = NULL;
  int i;
  int j;
  SEQ_T** seq_list;
  int num_seqs;
  int seq_len;
  //For the bdb_bg mode:
  ARRAY_T* seq_bg_freqs;
  MATRIX_T* pssm;
  MATRIX_T* rev_pssm;
  double atcontent;
  double roundatcontent;
  char atcontentstr[6];
  char keydata[16];
  double scaled_pvalue_threshold = args.pvalue_threshold;
  double avg_seq_length = 0;
  ALPH_T alph;

  // get the alphabet from the global motifs
  alph = INVALID_ALPH;
  if (motifs.num > 0) {
    alph = get_motif_alph(motif_at(motifs.motifs, 0));
  }
  assert(alph != INVALID_ALPH);

  //Open the file.
  if (open_file(args.sequence_filename, "r", FALSE, "FASTA", "sequences", &seq_file) == 0) {
    fprintf(stderr, "Couldn't open the file %s.\n", args.sequence_filename);
    rsc_terminate(1);
  }

  //Start reading in the sequences
  read_many_fastas(alph, seq_file, MAX_SEQ_LENGTH, &num_seqs, &seq_list);

  // write to html data section
  jsonwr_property(args.json_output, "sequence_db");
  jsonwr_start_object_value(args.json_output);
  jsonwr_str_prop(args.json_output, "source", args.sequence_filename);
  jsonwr_lng_prop(args.json_output, "count", num_seqs);
  jsonwr_end_object_value(args.json_output);

  seq_ids = new_string_list();
  seq_fscores = allocate_array(num_seqs);

  //Allocate the required space for results
  results = malloc(sizeof(double*) * motifs.num);
  for (i=0;i<motifs.num;i++) {
    results[i] = malloc(sizeof(double)*num_seqs);
  }

  for (j=0;j<num_seqs;j++) {

    //copy the pointer in
    sequence = seq_list[j];
    //Store some sequence information for later
    add_string(get_seq_name(sequence),seq_ids);
    seq_len = get_seq_length(sequence);
    set_array_item(j,atof(get_seq_description(sequence)),seq_fscores);

    BOOLEAN_T needs_postprocessing; // ama_sequence_scan always sets this to TRUE
    for (i = 0;i<motifs.num;i++) {
      results[i][j] = 0;
      int motifi = 2*i;     // every second motif in the list is a reverse complement
      if (args.scoring == TOTAL_HITS) {
        //Configure the length correction, if any
        if (args.length_correction) {
          //Robert's backwards bonferroni (of course somebody else would have invented this)
          scaled_pvalue_threshold = rsc_bonferroni_correction(args.pvalue_threshold, 1.0/seq_len);
        }
        results[i][j] = ramen_sequence_scan(sequence, 
            motif_at(motifs.motifs, motifi),
            motif_at(motifs.motifs, motifi+1),
            &motifs.pssms[motifi],
            &motifs.pssms[motifi+1],
            NULL, NULL, //no need to pass odds
            args.scoring, 0, TRUE,
            scaled_pvalue_threshold, motifs.bg_freqs);
      } else { 
        results[i][j] = ramen_sequence_scan(sequence, 
            motif_at(motifs.motifs, motifi),
            motif_at(motifs.motifs, motifi+1),
            NULL, NULL, //no need to pass pssm	
            motifs.odds[motifi],
            motifs.odds[motifi+1],
            args.scoring, 0, TRUE,
            0, motifs.bg_freqs);

        if (TRUE == args.linreg_normalise) {
          int k, len, A, C, G, T;
          double maxscore;
          MATRIX_T *odds;

          A = alph_index(alph, 'A');
          C = alph_index(alph, 'C');
          G = alph_index(alph, 'G');
          T = alph_index(alph, 'T');
          odds = motifs.odds[motifi];
          len = get_num_rows(odds);
          for (k = 0, maxscore = 1; k < len; k++) {
            double maxp = 0;
            if (maxp < get_matrix_cell(k, A, odds)) maxp = get_matrix_cell(k, A, odds);
            if (maxp < get_matrix_cell(k, C, odds)) maxp = get_matrix_cell(k, C, odds);
            if (maxp < get_matrix_cell(k, G, odds)) maxp = get_matrix_cell(k, G, odds);
            if (maxp < get_matrix_cell(k, T, odds)) maxp = get_matrix_cell(k, T, odds);
            maxscore *= maxp;
          }
          results[i][j] /= maxscore;
          if (j == 0 && args.verbose >= HIGHER_VERBOSE)
            fprintf(stderr, "\nScaling %s down by 1 / %.4g", 
                get_motif_st_id(motif_at(motifs.motifs, motifi)), maxscore);
        }
      }
    }
  }
}

void rsc_get_scores(int argc, char *argv[]) {
  int i;
  int j;
  double sorted_scores[get_num_strings(seq_ids)];
  int seq_num;
  rsc_result_t** rsr;
  rsc_result_t* result;
  rsc_rank_t** rankings;


  seq_num = get_num_strings(seq_ids); //number of sequences
  //allocate space for final one result per motif array.
  rsr = malloc(sizeof(rsc_result_t*)*motifs.num);

  for(i=0;i<motifs.num;i++) {
    double highest_score = 0; //We use this to make sure that a pwm has scored on at least 1 seq.

    //fprintf(stdout, "Doing rank scoring %i of %i motifs...\n", i+1, motifs.num);



    //Create the list of ranks
    rankings = malloc(sizeof(rsc_rank_t*)*seq_num);
    for (j=0; j < seq_num; j++) {
      rankings[j] = malloc(sizeof(rsc_rank_t));
      rankings[j]->pwm_score = results[i][j];
      if (results[i][j] > highest_score)
        highest_score = results[i][j]; //keep track of the pwm high score.
      rankings[j]->f_score = get_array_item(j, seq_fscores); //we don't  use this yet
      rankings[j]->f_rank = j+1;
    }

    //Try shuffling here.
    shuffle((void**)rankings, seq_num);

    //Sort it
    qsort (rankings, seq_num, sizeof(rsc_rank_t*), rsc_compare_ranks_pwm_score);

    //Assign pwm ranks
    for (j=0; j < seq_num; j++) {
      rankings[j]->pwm_rank = j+1;
    }

    //Positive FL is the default.
    if (POS_PWM != args.positive_list) {
      //resort via fluoresence rank
      qsort (rankings, seq_num, sizeof(rsc_rank_t*), rsc_compare_ranks_f_rank);
    }

    ///Debugging code.
    if (args.verbose >= HIGHER_VERBOSE) {
      fprintf(stdout, "\n");
      for (j=0; j < seq_num; j++) {
        fprintf(stdout, "M2: %s - Seq: %s Rankings[%i] -\tpwm: %.8f\tprank: %i\tf: %.8f\tfrank: %i\n", 
            get_motif_st_id(motif_at(motifs.motifs,i*2)), get_nth_string(j,seq_ids), j,
            rankings[j]->pwm_score, rankings[j]->pwm_rank,
            rankings[j]->f_score, rankings[j]->f_rank);
      }
    }

    if (highest_score > 0) {
      //Determine the lowest subset pvalue
      if (RANKSUM_METHOD == args.pvalue_method) {
        rsr[i] = rsc_do_ranksum_test(rankings, i);
      } else if (FISHER_METHOD == args.pvalue_method) {
        rsr[i] = rsc_do_fisher_test(rankings, i);
      } else if (MULTIHG_METHOD == args.pvalue_method || 
          LONG_MULTIHG_METHOD == args.pvalue_method) {
        rsr[i] = rsc_do_multihg_test(rankings, i);
      } else if (LINREG_METHOD == args.pvalue_method) {
        rsr[i] = rsc_do_linreg_test(rankings,i);
      } else if (SPEARMAN_METHOD == args.pvalue_method) {
        rsr[i] = rsc_do_spearman_test(rankings,i);
      }
    } else {
      //If no sequence has scored at all, then we give a null result.
      // Bugfix, rsr[i] was not assigned the null result.
      rsr[i] = init_result (NULL, motifs.db_idx[i], motif_at(motifs.motifs, i), seq_num, 1, 1, 1, -1);
    }

    //Free up some space - TODO: Move this into a more appropriate place
    for (j=0; j < seq_num - 1; j++) {
      free(rankings[j]);
    }
    free(rankings);
  }

  if (LINREG_METHOD == args.pvalue_method) {
    qsort(rsr, motifs.num, sizeof(rsc_result_t*), rsc_compare_mse);
  } else {
    qsort(rsr, motifs.num, sizeof(rsc_result_t*), rsc_compare_scores);
  }

  if (LINREG_METHOD != args.pvalue_method && SPEARMAN_METHOD != args.pvalue_method) {
    int thresh = 1;
    if (args.fix_partition <= 0) thresh = seq_num;
    rsc_print_results("Motif p-values are corrected by #Motifs * "
        "#ThresholdsTested - (%i * %i = %i)\n\n",
        motifs.num, thresh, motifs.num * thresh);  
  } 
  jsonwr_property(args.json_output, "motifs");
  jsonwr_start_array_value(args.json_output);
  for (i = 0; i < motifs.num; ++i) {
    int number_of_tests;
    double corrected_p_val = 0;

    result = rsr[i];

    if (args.fix_partition > 0) {
      number_of_tests = motifs.num * 1;
    } else {
      number_of_tests = motifs.num * seq_num;
    }

    if (RANKSUM_METHOD == args.pvalue_method) {
      corrected_p_val = rsc_bonferroni_correction(result->pboth, number_of_tests);
      if (corrected_p_val < args.pvalue_report_threshold) {
        rsc_print_results("%i. Ranksum p-values of motif %s top %i seqs "
            "(left,right,twotailed): %.4g %.4g %.4g U-value: %.4g (Corrected "
            "p-values: %.4g %.4g %.4g)\n",
            i+1, get_motif_id(result->motif), result->split, result->pleft, 
            result->pright, result->pboth, result->u, 
            rsc_bonferroni_correction(result->pleft,number_of_tests), 
            rsc_bonferroni_correction(result->pright,number_of_tests), 
            corrected_p_val);
        record_result(result, corrected_p_val); 
      }
    } else if (FISHER_METHOD == args.pvalue_method) {
      corrected_p_val = rsc_bonferroni_correction(result->pright,number_of_tests);
      if (corrected_p_val < args.pvalue_report_threshold) {
        rsc_print_results("%i. Fisher-exact p-value of motif %s %s top %i seqs: %.4g (Corrected p-value: %.4g)\n",
            i+1, get_motif_id(result->motif), get_motif_id2(result->motif), result->split, result->pright, corrected_p_val);
        record_result(result, corrected_p_val);
      }
    } else if (MULTIHG_METHOD == args.pvalue_method) {
      corrected_p_val = rsc_bonferroni_correction(result->pright,number_of_tests);
      if (corrected_p_val < args.pvalue_report_threshold) {
        rsc_print_results("%i. MultiHG p-value of motif %s top %i seqs: %.4g (Corrected p-value: %.4g)\n",
            i+1, get_motif_id(result->motif), result->split, result->pright, corrected_p_val);
        record_result(result, corrected_p_val);
      }
    } else if (LONG_MULTIHG_METHOD == args.pvalue_method) {
      corrected_p_val = rsc_bonferroni_correction(result->pright,number_of_tests);
      if (corrected_p_val < args.pvalue_report_threshold) {
        rsc_print_results("%i. 4D-MultiHG p-value of motif %s top %i seqs: %.4g "
            "(Corrected p-value: %.4g)\n", i+1, get_motif_id(result->motif), 
            result->split, result->pright, corrected_p_val);
        record_result(result, corrected_p_val);
      }
    } else if (LINREG_METHOD == args.pvalue_method) {
      rsc_print_results("%i. LinReg MSE of motif %s top %i seqs: %.4e (m: %.4e b: %.4e)\n",
          i+1, get_motif_id(result->motif), result->split, result->pboth, result->pleft, result->pright);
      record_result(result, 0.0);
    } else if (SPEARMAN_METHOD == args.pvalue_method) {
      rsc_print_results("%i. Spearman Rho of motif %s top %i seqs: %.4e\n",
          i+1, get_motif_id(result->motif), result->split, result->pboth);
      record_result(result, 0.0);
    }
  }
  jsonwr_end_array_value(args.json_output);
}

/*
 * Using the spearman rank correlation test,
 *
 */
rsc_result_t* rsc_do_spearman_test(rsc_rank_t** rankings, int motif_index) {
  //Assorted vars
  int seq_num;
  int j;

  //Vars for the test
  double* x;
  double* y;

  //Vars for scoring
  double rank_score;
  rsc_result_t* lowest_motif_result;

  //Allocate memory or set initial values
  seq_num = get_num_strings(seq_ids); //number of sequences
  lowest_motif_result = malloc(sizeof(rsc_result_t)); //allocate space, as a ptr to this will go in the array later
  //that's why we don't free it in this loop.
  x = malloc(sizeof(double)*seq_num);
  y = malloc(sizeof(double)*seq_num);

  //Now we need to copy the scores into two double arrays
  //Use LOG macro so that log(0) 'works'
  for (j=0; j < seq_num; j++) {
    if (args.log_fscores == TRUE) {
      x[j] = LOG(rankings[j]->f_score);
    } else {
      x[j] = rankings[j]->f_score;
    }

    if (args.log_pwmscores == TRUE) {
      y[j] = LOG(rankings[j]->pwm_score);
    } else {
      y[j] = rankings[j]->pwm_score;
    }
    if (args.verbose == 5) {
      fprintf(stderr, "Rank %i: LR F-Score %.4g (%.4g) LR PWM-Score: %.4g "
          "(%.4g)\n", j, x[j], rankings[j]->f_score,y[j], rankings[j]->pwm_score);
    }
  }

  //Get the result
  rank_score = spearman_rank_correlation(seq_num,x,y);

  if (args.verbose >= HIGH_VERBOSE) {
    rsc_print_results("Spearman MSE of motif %s top %i seqs: %.4g\n", 
        get_motif_st_id(motif_at(motifs.motifs, motif_index*2)), seq_num, 
        rank_score);
  }

  init_result (lowest_motif_result, motifs.db_idx[motif_index*2], motif_at(motifs.motifs, motif_index*2),
      seq_num,
      -1, -1,  //Not really p-values, but they'll do...
      rank_score, -1);

  return lowest_motif_result;
}
/*
 * Using the linreg test,
 *
 * this method returns the lowest scoring subdivision of a set of sequences for a given motif.
 * It's not self-contained, as it requires to hook into the global variables results, motifs, seq_ids.
 */
rsc_result_t* rsc_do_linreg_test(rsc_rank_t** rankings, int motif_index) {
  //Assorted vars
  int seq_num;
  int j;

  //Vars for the regression
  double* x;
  double* y;
  double m = 0;
  double b = 0;

  //Vars for scoring
  double lowest_mse = 9999;
  rsc_result_t* lowest_motif_result;

  //Allocate memory or set initial values
  seq_num = get_num_strings(seq_ids); //number of sequences
  lowest_motif_result = malloc(sizeof(rsc_result_t)); //allocate space, as a ptr to this will go in the array later
  //that's why we don't free it in this loop.
  x = malloc(sizeof(double)*seq_num);
  y = malloc(sizeof(double)*seq_num);

  //Now we need to copy the scores into two double arrays
  //Use LOG macro so that log(0) 'works'
  for (j=0; j < seq_num; j++) {
    if (args.log_fscores == TRUE) {
      x[j] = LOG(rankings[j]->f_score);
    } else {
      x[j] = rankings[j]->f_score;
    }

    if (args.log_pwmscores == TRUE) {
      y[j] = LOG(rankings[j]->pwm_score);
    } else {
      y[j] = rankings[j]->pwm_score;
    }
    if (args.verbose == 5) {
      fprintf(stderr, "Rank %i: LR F-Score %.4g (%.4g) LR PWM-Score: %.4g "
          "(%.4g)\n", j, x[j], rankings[j]->f_score,y[j], 
          rankings[j]->pwm_score);
    }
  }

  // TODO: Remove this for production
  if(args.linreg_dump_dir > 0) {
    FILE *fh;
    char* filename;
    filename = malloc(sizeof(char)*(strlen(args.linreg_dump_dir) + 50));
    sprintf(filename, "%s/%s.tsv", args.linreg_dump_dir, 
        get_motif_st_id(motif_at(motifs.motifs, motif_index*2)));

    fh = fopen(filename, "w");
    fputs("X\tY\n", fh);
    for (j=0; j < seq_num; j++) {
      fprintf(fh, "%.10e %.10e\n", x[j], y[j]);
    }
    fclose(fh);
    free(filename);
  }


  //We start with a minimum of three sequences so that the data
  //is over-described.
  for (j=3; j < seq_num; j++) {

    /*extern double regress(
      int n,                        / number of points /
      double *x,                    / x values /
      double *y,                    / y values /
      double *m,                    / slope /
      double *b                     / y intercept /
      );*/

    double mse = 0;
    if (args.linreg_switchxy) {
      mse = regress(j+1, y, x, &m, &b);
    } else {
      mse = regress(j+1, x, y, &m, &b);
    }

    if (args.verbose >= HIGH_VERBOSE) {
      rsc_print_results("LinReg MSE of motif %s top %i seqs: %.4g (m: %.4g b: %.4g)\n",
          get_motif_st_id(motif_at(motifs.motifs,motif_index*2)), j+1, mse, m, b);
    }

    //Add to our motif list if lowest MSE
    if (lowest_mse > mse) {
      lowest_mse = mse;
      init_result (lowest_motif_result, motifs.db_idx[motif_index*2], motif_at(motifs.motifs,motif_index*2), j+1,
          m, b,  //Not really p-values, but they'll do...
          mse, -1);
    }

  }

  return lowest_motif_result;
}

/*
 * Using the Ranksum test,
 *
 * this method returns the lowest scoring subdivision of a set of sequences for a given motif.
 * It's not self-contained, as it requires to hook into the global variables results, motifs, seq_ids.
 */
rsc_result_t* rsc_do_ranksum_test(rsc_rank_t** rankings, int motif_index) {
  int n,na; // for rs stats test.
  double ta_obs; // for rs stats test.
  int seq_num;
  int j;
  double lowest_pval;
  RSR_T* r;
  rsc_result_t* lowest_motif_result;

  seq_num = get_num_strings(seq_ids); //number of sequences
  n = seq_num;
  na = ta_obs = 0;
  lowest_pval = 1; //1 is highest possible pval
  lowest_motif_result = malloc(sizeof(rsc_result_t)); //allocate space, as a ptr to this will go in the array later
  //that's why we don't free it in this loop.

  for (j=0; j < seq_num - 1; j++) {
    /* We now are not going to compare about ties for right now - instead we'll just
     * count them up.
     * We need to keep track of:
     *      int n,       number of samples
     int na,       number of positives
     double ta_obs   sum of ranks of positives
     */
    na++;
    if (POS_PWM == args.positive_list) {
      ta_obs += rankings[j]->f_rank; //We consider fluorescence to tell us what's positive
    } else {
      ta_obs += rankings[j]->pwm_rank; //We consider the pwm to tell us what's positive
    }

    if (QUICK_RS == args.rs_method) {
      //This thing is working backwards.
      // Should be: r = ranksum_from_stats(n, na, ta_obs);
      r = ranksum_from_stats(n, n - na, n*(n+1)/2 - ta_obs);
    } else {
      // Beware 2d array pointer arithmetic.
      // This needs to be fixed badly.
      r = ranksum_from_sets(results[motif_index*2] + (j+1), seq_num-(j+1), 
          results[motif_index*2], j+1);
    }

    if (args.verbose >= HIGH_VERBOSE) {
      fprintf(stderr, "Ranksum p-values of motif %s top %i seqs "
          "(left,right,twotailed): %g %g %g U-value: %.4g \n",
          get_motif_st_id(motif_at(motifs.motifs,motif_index*2)), j+1, 
          RSR_get_p_left(r), RSR_get_p_right(r), RSR_get_p_twotailed(r), RSR_get_u(r));
    }
    //Add to our motif list
    if (lowest_pval > RSR_get_p_right(r)) {
      lowest_pval = RSR_get_p_right(r);
      init_result (lowest_motif_result, motifs.db_idx[motif_index*2], motif_at(motifs.motifs, motif_index*2), j+1,
          RSR_get_p_left(r), RSR_get_p_right(r), RSR_get_p_twotailed(r),
          RSR_get_u(r));
    }

  }

  return lowest_motif_result;
}

/*
 * Using the Ranksum test,
 *
 * this method returns the lowest scoring subdivision of a set of sequences for a given motif.
 * It's not self-contained, as it requires to hook into the global variables results, motifs, seq_ids.
 */
rsc_result_t* rsc_do_fisher_test(rsc_rank_t** rankings, int motif_index) {
  int seq_num;
  int i,j;
  double lowest_pval;
  double p,pleft,pright,pboth;
  RSR_T* r;
  rsc_result_t* lowest_motif_result;
  int tp,fn,fp,tn,a,b,c,d;

  seq_num = get_num_strings(seq_ids); //number of sequences
  lowest_pval = 100; //a large number.
  lowest_motif_result = malloc(sizeof(rsc_result_t)); //allocate space, as a ptr to this will go in the array later
  //that's why we don't free it in this loop.

  //Used for testing so that we can be sure that we're initialised this variable
  lowest_motif_result->u = 0;

  /* Need to do truth table:
   *
   *     fl+, pwm + (tp)    fl+, pwm- (fn)
   *     fl-, pwm + (fp)    fl-, pwm- (tn)
   *
   */
  fisher_exact_init(seq_num); //initialise our factorial datastructure

  if (args.fix_partition > 0) {
    /*
     * We do just one partition.
     */
    int i = args.fix_partition - 1;
    tp = fn = fp = tn = 0;
    for (j=0;j< seq_num; j++) {
      if (j <= i) {
        //We're doing above the threshold
        if (POS_FL == args.positive_list) {
          //Well, we're sorted by FL score, so we check the PWM score, and if it's over
          //our threshold, then we add one to the tp.
          (rankings[j]->pwm_score >= args.fisher_pwm_threshold) ? tp++ : fp++;
        } else {
          //Well, we're sorted by PWM score, so we check the FL score, and if it's under
          //our threshold, then we add one to the tp.
          (rankings[j]->f_score <= args.fisher_pwm_threshold) ? tp++ : fp++;
        }
      } else {
        //We're doing below the threshold
        if (POS_FL == args.positive_list) {
          //Well, we're sorted by FL score, so we check the PWM score, and if it's over
          //our threshold, then we add one to the tn.
          (rankings[j]->pwm_score < args.fisher_pwm_threshold) ? tn++ : fn++;
        } else {
          //Well, we're sorted by PWM score, so we check the FL score, and if it's under
          //our threshold, then we add one to the tn.
          (rankings[j]->f_score > args.fisher_fl_threshold) ? tn++ : fn++;
        }
      }
    }
    /* Map to the algebra used in the formula:
     *
     * if score >= threshold tp++
     *         else          fp++
     *
     * ------ threshold ----
     *
     * if score < threshold tn++
     *         else         fn++
     *
     *         T   F
     *       ________
     * Above |tp   fp
     * Below |fn   tn
     *
     * Standard formula is:
     *
     *         T   F
     *       ________
     * Above |a    b
     * Below |c    d
     *
     */
    a = tp;
    b = fp;
    c = fn;
    d = tn;

    fisher_exact(a,b,c,d, &pboth, &pleft, &pright, &p);
    lowest_pval = pboth;
    init_result (lowest_motif_result, motifs.db_idx[motif_index*2], motif_at(motifs.motifs,motif_index*2), 
        i+1, pboth, pboth, pboth, -1);
    if (args.verbose >= HIGH_VERBOSE) {
      fprintf(stdout, "M3: %s Threshold: %i a: %i b: %i c: %i d: %i Ptwo: %g "
          "P-left: %g P-right: %g P-this %g\n",
          get_motif_st_id(motif_at(motifs.motifs,motif_index*2)), i, a,b,c,d, 
          pboth, pleft, pright, p);
    }
  } else {
    /*
     * We do partition maximisation
     */
    for (i=0; i < seq_num - 1; i++) {
      //TODO: Make this not n^2
      tp = fn = fp = tn = 0;
      for (j=0;j< seq_num; j++) {
        if (j <= i) {
          //We're doing above the threshold
          if (POS_FL == args.positive_list) {
            //Well, we're sorted by FL score, so we check the PWM score, and if it's over
            //our threshold, then we add one to the tp.
            (rankings[j]->pwm_score >= args.fisher_pwm_threshold) ? tp++ : fp++;
          } else {
            //Well, we're sorted by PWM score, so we check the FL score, and if it's under
            //our threshold, then we add one to the tp.
            (rankings[j]->f_score <= args.fisher_pwm_threshold) ? tp++ : fp++;
          }
        } else {
          //We're doing below the threshold
          if (POS_FL == args.positive_list) {
            //Well, we're sorted by FL score, so we check the PWM score, and if it's over
            //our threshold, then we add one to the tn.
            (rankings[j]->pwm_score < args.fisher_pwm_threshold) ? tn++ : fn++;
          } else {
            //Well, we're sorted by PWM score, so we check the FL score, and if it's under
            //our threshold, then we add one to the tn.
            (rankings[j]->f_score > args.fisher_fl_threshold) ? tn++ : fn++;
          }
        }
      }

      /* Map to the algebra used in the formula:
       *
       * if score >= threshold tp++
       *         else          fp++
       *
       * ------ threshold ----
       *
       * if score < threshold tn++
       *         else         fn++
       *
       *         T   F
       *       ________
       * Above |tp   fp
       * Below |fn   tn
       *
       * Standard formula is:
       *
       *         T   F
       *       ________
       * Above |a    b
       * Below |c    d
       *
       */
      a = tp;
      b = fp;
      c = fn;
      d = tn;

      fisher_exact(a,b,c,d,&pboth, &pleft, &pright, &p);

      /*fprintf(stderr, "choose(%i,%i) * choose(%i,%i) / choose(%i,%i)\n", 
          tp+fp,tp, fp+tn,fp, seq_num,tp+fp);
        fprintf(stderr, "     %g      %g        %g  \n", 
          (long double)choose(tp+fp,tp), (long double)choose(fp+tn,fp), 
          (long double)choose(seq_num,tp+fp));
      //fprintf(stderr, "t1: %i t2: %i t3: %i\n\n", a, b, c); // */

      if (args.verbose >= HIGH_VERBOSE) {
        fprintf(stderr, "Fisher-exact p-value of motif %s top %i seqs (twotailed): %g\n",
            get_motif_st_id(motif_at(motifs.motifs, motif_index*2)), i+1, pboth);
      }

      //Add to our motif list
      if (lowest_pval >= pboth) {
        lowest_pval = pboth;
        init_result (lowest_motif_result, motifs.db_idx[motif_index*2], motif_at(motifs.motifs,motif_index*2),
            i+1, pboth, pboth, pboth, -1);
      }
      if (args.verbose >= HIGHER_VERBOSE) {
        fprintf(stderr, "M: %s Threshold: %i a: %i b: %i c: %i d: %i Ptwo: %g "
            "P-left: %g P-right: %g P-this %g\n",
            get_motif_st_id(motif_at(motifs.motifs, motif_index*2)), 
            i, a,b,c,d, pboth, pleft, pright, p);
      }
    }

  }

  assert(lowest_motif_result != NULL);
  assert(lowest_motif_result->u != 0);
  return lowest_motif_result;
}

/*
 * Using the Ranksum test,
 *
 * this method returns the lowest scoring subdivision of a set of sequences for a given motif.
 * It's not self-contained, as it requires to hook into the global variables results, motifs, seq_ids.
 */
rsc_result_t* rsc_do_multihg_test(rsc_rank_t** rankings, int motif_index) {
  int seq_num;
  int i,j,k;
  double lowest_pval;
  double p;
  RSR_T* r;
  rsc_result_t* lowest_motif_result;
  int b0,b1,b2,b3;
  int n,N;
  int i0,i1,i2,i3,B0,B1,B2,B3;

  seq_num = get_num_strings(seq_ids); //number of sequences
  lowest_pval = 100; //a large number.
  lowest_motif_result = malloc(sizeof(rsc_result_t)); //allocate space, as a ptr to this will go in the array later
  //that's why we don't free it in this loop.

  //Used for testing so that we can be sure that we're initialised this variable
  lowest_motif_result->u = 0;

  //Get per-class totals
  B0 = B1 = B2 = B3 = 0;
  for (i=0; i < seq_num; i++) {
    if (rankings[i]->pwm_score == 0) {
      B0++;
    } else if (rankings[i]->pwm_score == 1) {
      B1++;
    } else if (rankings[i]->pwm_score == 2) {
      B2++;
    } else {
      B3++;
    }
  }
  // If we are only 3d rather than 4d...
  if (args.pvalue_method == MULTIHG_METHOD) {
    B2+=B3;
  }

  /*
   * Do it for a fixed partition only.
   */
  if (args.fix_partition > 0) {
    int i = args.fix_partition - 1;
    /* Need to do count table:
     *
     *     a0 - above threshold, 0 hits
     *     a1 - above threshold, 1 motif hit
     *     a2 - above threshold, >=2 motif hits
     *
     */
    b0 = b1 = b2 = b3 = 0;
    for (j=0;j<=i; j++) {
      if (rankings[j]->pwm_score == 0) {
        b0++;
      } else if (rankings[j]->pwm_score == 1) {
        b1++;
      } else if (rankings[j]->pwm_score == 2) {
        b2++;
      } else {
        b3++;
      }
    }

    if (args.pvalue_method == MULTIHG_METHOD) {

      b2 += b3;
      /* Formula details:
       *
       * Please see page 31 of Robert's lab book.
       *
       *
       * The notation is the same as Eden et al. 2007.
       *
       * If we count two or more. (3>2, so we add it to B2 etc), since this
       * piece of code has a max dimension of 3.
       *
       */
      n = i+1; //n is the threshold
      N = seq_num; //total set size;
      p = LOGZERO;

      for (i0=b0; i0<=B0 && i0<=n; i0++) {
        for (i1=b1; i1<=B1 && i1<=n-i0; i1++) {
          for (i2=b2; i2<=B2 && i2<=n-(i0+i1); i2++) {
            //fprintf(stderr, "b0: %i b1: %i b2: %i B0: %i B1: %i B2: %i n: %i N: %i\n",
            //                                           b0,b1,b2,B0,B1,B2, n, N);
            p = LOG_SUM(p, //We're in log space, remember.
                (
                 (
                  factorial_array[n] - (factorial_array[i0] + 
                    factorial_array[i1] + factorial_array[i2])
                 ) + (
                   (factorial_array[N-n]) - (factorial_array[B0-i0] + 
                     factorial_array[B1-i1] + factorial_array[B2-i2])
                   )
                ) - (
                  (factorial_array[N] - (factorial_array[B0] + 
                      factorial_array[B1] + factorial_array[B2]))
                  )
                );
            //fprintf(stderr, "Current p for i0: %i i1 %i i2 %i: %g\n",i0,i1,i2,exp(p));
          }
        }
      }

    } else { // We're going to do the complicated method with four loops.

      /* Formula details:
       *
       * Please see page 31 of Robert's lab book.
       *
       *
       * The notation is the same as Eden et al. 2007.
       *
       *
       */
      n = i+1; //n is the threshold
      N = seq_num; //total set size;
      p = LOGZERO;

      for (i0=b0; i0<=B0 && i0<=n; i0++) {
        for (i1=b1; i1<=B1 && i1<=n-i0; i1++) {
          for (i2=b2; i2<=B2 && i2<=n-(i0+i1); i2++) {
            for (i3=b3; i3<=B3 && i3<=n-(i0+i1+i2); i3++) {
              p = LOG_SUM(p, //We're in log space, remember.
                  (
                   (
                    factorial_array[n] - (factorial_array[i0] + 
                      factorial_array[i1] + factorial_array[i2] + 
                      factorial_array[i3])
                   ) + (
                     (factorial_array[N-n]) - (factorial_array[B0-i0] + 
                       factorial_array[B1-i1] + factorial_array[B2-i2] + 
                       factorial_array[B3-i3])
                     )
                  ) - (
                    (factorial_array[N] - (factorial_array[B0] + 
                            factorial_array[B1] + factorial_array[B2]) + 
                     factorial_array[B3])
                    )
                  );
              //fprintf(stderr, "Current p for i0: %i i1 %i i2 %i: %g\n",
              //  i0,i1,i2,exp(p));
            }
          }
        }
      }

    }


    //Add to our motif list
    if (args.verbose >= HIGH_VERBOSE) {
      fprintf(stderr, "Motif: %s Threshold: %i P-value: %g\n", 
          get_motif_st_id(motif_at(motifs.motifs, motif_index*2)), i, exp(p));
    }
    lowest_pval = p;
    init_result (lowest_motif_result, motifs.db_idx[motif_index*2],
        motif_at(motifs.motifs, motif_index*2), i+1,
        lowest_motif_result->pleft, // exp(p)
        p,
        lowest_motif_result->pboth, // exp(p)
        -1);
  } else {

    //TODO: Make this not n^2
    for (i=0; i < seq_num - 1; i++) {
      /* Need to do count table:
       *
       *     a0 - above threshold, 0 hits
       *     a1 - above threshold, 1 motif hit
       *     a2 - above threshold, >=2 motif hits
       *
       */
      b0 = b1 = b2 = b3 = 0;
      for (j=0;j<=i; j++) {
        if (rankings[j]->pwm_score == 0) {
          b0++;
        } else if (rankings[j]->pwm_score == 1) {
          b1++;
        } else if (rankings[j]->pwm_score == 2) {
          b2++;
        } else {
          b3++;
        }
      }

      if (args.pvalue_method == MULTIHG_METHOD) {

        b2 += b3;
        /* Formula details:
         *
         * Please see page 31 of Robert's lab book.
         *
         *
         * The notation is the same as Eden et al. 2007.
         *
         * If we count two or more. (3>2, so we add it to B2 etc), since this
         * piece of code has a max dimension of 3.
         *
         */
        n = i+1; //n is the threshold
        N = seq_num; //total set size;
        p = LOGZERO;

        for (i0=b0; i0<=B0 && i0<=n; i0++) {
          for (i1=b1; i1<=B1 && i1<=n-i0; i1++) {
            for (i2=b2; i2<=B2 && i2<=n-(i0+i1); i2++) {
              //fprintf(stderr, "b0: %i b1: %i b2: %i B0: %i B1: %i B2: %i n: %i N: %i\n",
              //                                           b0,b1,b2,B0,B1,B2, n, N);
              p = LOG_SUM(p, //We're in log space, remember.
                  (
                   (
                    factorial_array[n] - (factorial_array[i0] + 
                      factorial_array[i1] + factorial_array[i2])
                   ) + (
                     (factorial_array[N-n]) - (factorial_array[B0-i0] + 
                       factorial_array[B1-i1] + factorial_array[B2-i2])
                     )
                  ) - (
                    (factorial_array[N] - (factorial_array[B0] + 
                        factorial_array[B1] + factorial_array[B2]))
                    )
                  );
              //fprintf(stderr, "Current p for i0: %i i1 %i i2 %i: %g\n",
              //  i0,i1,i2,exp(p));
            }
          }
        }

      } else { // We're going to do the complicated method with four loops.

        /* Formula details:
         *
         * Please see page 31 of Robert's lab book.
         *
         *
         * The notation is the same as Eden et al. 2007.
         *
         *
         */
        n = i+1; //n is the threshold
        N = seq_num; //total set size;
        p = LOGZERO;

        for (i0=b0; i0<=B0 && i0<=n; i0++) {
          for (i1=b1; i1<=B1 && i1<=n-i0; i1++) {
            for (i2=b2; i2<=B2 && i2<=n-(i0+i1); i2++) {
              for (i3=b3; i3<=B3 && i3<=n-(i0+i1+i2); i3++) {
                p = LOG_SUM(p, //We're in log space, remember.
                    (
                     (
                      factorial_array[n] - (factorial_array[i0] + 
                        factorial_array[i1] + factorial_array[i2] + 
                        factorial_array[i3])
                     ) + (
                       (factorial_array[N-n]) - (factorial_array[B0-i0] + 
                         factorial_array[B1-i1] + factorial_array[B2-i2] + 
                         factorial_array[B3-i3])
                       )
                    ) - (
                      (factorial_array[N] - (factorial_array[B0] + 
                              factorial_array[B1] + factorial_array[B2]) + 
                       factorial_array[B3])
                      )
                    );
                //fprintf(stderr, "Current p for i0: %i i1 %i i2 %i: %g\n",
                //    i0,i1,i2,exp(p));
              }
            }
          }
        }

      }


      //TODO :fix below here.
      //Add to our motif list
      if (args.verbose >= HIGH_VERBOSE) {
        fprintf(stderr, "Motif: %s Threshold: %i P-value: %g\n", 
            get_motif_st_id(motif_at(motifs.motifs, motif_index*2)), i, exp(p));
      }
      if (lowest_pval >= p) {
        lowest_pval = p;
        init_result (lowest_motif_result, motifs.db_idx[motif_index*2],
            motif_at(motifs.motifs, motif_index*2),
            i+1,
            lowest_motif_result->pleft, // exp(p)
            p,
            lowest_motif_result->pboth, // exp(p)
            -1);
      }

    }
  }

  assert(lowest_motif_result != NULL);
  assert(lowest_motif_result->u != 0);
  lowest_motif_result->pright = exp(lowest_motif_result->pright); //Small increase in speed.
  return lowest_motif_result;
}

void rsc_terminate(int status) {
  /* Display time of execution */
  if (verbosity >= HIGH_VERBOSE) {
    t1 = time(NULL);
    c1 = clock();
    fprintf (stderr, "Elapsed wall clock time: %ld seconds\n", (long)(t1 - t0));
    fprintf (stderr, "Elapsed CPU time:        %f seconds\n", 
        (float)(c1 - c0) / CLOCKS_PER_SEC);
  }
  // don't risk closing the files if something went wrong: 
  //  should be more specific on when not to do this
  if (!status && args.sequence_filename) final_print_results();
  exit(status);
}

/*
 *
 * This method does NOT contrain A=T and G=C.
 *
 */
ARRAY_T* rsc_get_bg_freqs(SEQ_T* seq) {
  double a_freq=0.0;
  double c_freq=0.0;
  double g_freq=0.0;
  double t_freq=0.0;
  int c = 0;
  int index,j = 0;
  int seq_len = get_seq_length(seq);
  ARRAY_T* bg_freqs = allocate_array(alph_size(DNA_ALPH, ALL_SIZE));

  for (j=0;j<seq_len;j++) {
    if (toupper(get_seq_char(j,seq))=='A')
      ++a_freq;
    else if (toupper(get_seq_char(j,seq))=='C')
      ++c_freq;
    else if (toupper(get_seq_char(j,seq))=='G')
      ++g_freq;
    else if (toupper(get_seq_char(j,seq))=='T')
      ++t_freq;
    ++c;
  }

  set_array_item(alph_index(DNA_ALPH, 'A'), a_freq/c, bg_freqs);
  set_array_item(alph_index(DNA_ALPH, 'C'), c_freq/c, bg_freqs);
  set_array_item(alph_index(DNA_ALPH, 'G'), g_freq/c, bg_freqs);
  set_array_item(alph_index(DNA_ALPH, 'T'), t_freq/c, bg_freqs);
  /* Compute values for ambiguous characters. */
  calc_ambigs(DNA_ALPH, FALSE, bg_freqs);
  return bg_freqs;
}



/*
 * COMPARISON METHODS FOLLOW
 */

int rsc_compare_scores (const void *a, const void *b)
{
  rsc_result_t r1 = **(rsc_result_t**)a;
  rsc_result_t r2 = **(rsc_result_t**)b;
  if (r2.pright - r1.pright < 0.0) {
    return 1;
  } else if (r2.pright - r1.pright > 0.0){
    return -1;
  } else {
    return 0;
  }
  //This compares it in reverse order
  //  return (int) (r2.pright - r1.pright);
}

int rsc_compare_mse (const void *a, const void *b)
{
  rsc_result_t r1 = **(rsc_result_t**)a;
  rsc_result_t r2 = **(rsc_result_t**)b;
  if (r2.pboth - r1.pboth < 0.0) {
    return 1;
  } else if (r2.pboth - r1.pboth > 0.0){
    return -1;
  } else {
    return 0;
  }
  //This compares it in reverse order
  //  return (int) (r2.pright - r1.pright);
}

int rsc_compare_doubles (const double *a, const double *b)
{
  //This does it in reverse order
  return (int) (*b - *a);
}

int rsc_compare_ranks_f_rank (const void *a, const void *b) {
  rsc_rank_t r1 = **(rsc_rank_t**)a;
  rsc_rank_t r2 = **(rsc_rank_t**)b;

  //fprintf(stderr, "Comparing: %i to %i\n", r1.f_rank, r2.f_rank);

  if (r2.f_rank - r1.f_rank < 0.0) {
    return 1;
  } else if (r2.f_rank - r1.f_rank > 0.0){
    return -1;
  } else {
    return 0;
  }
  //This compares it smallest first

}

int rsc_compare_ranks_pwm_score (const void *a, const void *b) {
  rsc_rank_t r1 = **(rsc_rank_t**)a;
  rsc_rank_t r2 = **(rsc_rank_t**)b;

  //fprintf(stderr, "\nComparing: %g to %g", r1.pwm_score, r2.pwm_score);

  if (r1.pwm_score - r2.pwm_score < 0.0) {
    return 1;
  } else if (r1.pwm_score - r2.pwm_score > 0.0){
    return -1;
  } else {
    return 0;
  }
  //This compares it largest first
}

/*
 * DEBUGGING METHODS FOLLOW.
 */

void rsc_dump_motif_freqs(MOTIF_T* m, MATRIX_T* freqs) {
  int i;

  assert(get_motif_alph(m) == DNA_ALPH);
  //Let's do some debugging output
  //MATRIX_T* freqs =  get_motif_freqs(m);
  rsc_print_results("%s\tA\tC\tG\tT\n",get_motif_st_id(m));
  for (i = 0; i < get_num_rows(freqs); i++) {
    rsc_print_results("%s\t%.4f\t%.4f\t%.4f\t%.4f\n", get_motif_st_id(m),
        get_matrix_cell(i, alph_index(DNA_ALPH, 'A'),freqs),
        get_matrix_cell(i, alph_index(DNA_ALPH, 'C'),freqs),
        get_matrix_cell(i, alph_index(DNA_ALPH, 'G'),freqs),
        get_matrix_cell(i, alph_index(DNA_ALPH, 'T'),freqs));
  }
}

unsigned long long choose(unsigned n, unsigned k) {
  unsigned i;
  long double accum;

  //fprintf(stderr, "%i choose %i: ",n,k);

  if (k > n)
    return 0;

  if (k > n/2)
    k = n-k; // faster

  accum = 1;
  for (i = 1; i <= k; i++)
    accum = accum * (n-k+i) / i;

  //fprintf(stderr, "%i\n", (int)(accum+0.5));
  return accum + 0.5; // avoid rounding error
}

long double logchoose(unsigned n, unsigned k) {
  unsigned i;
  long double accum;

  //fprintf(stderr, "%i choose %i: ",n,k);

  if (k > n)
    return 0;

  if (k > n/2)
    k = n-k; // faster

  accum = 0;
  for (i = 1; i <= k; i++)
    accum += log((n-k+i) / i);

  //fprintf(stderr, "%i\n", (int)(accum+0.5));
  return accum; // avoid rounding error
}


/* Arrange the N elements of ARRAY in random order.
   Only effective if N is much smaller than RAND_MAX;
   if this may not be the case, use a better random
   number generator. */
void shuffle(void** array, size_t n)
{
  void *t;
  if (n > 1) {
    size_t i;
    for (i = 0; i < n - 1; i++) {
      size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
      t = (void*)array[j];
      array[j] = array[i];
      array[i] = t;
    }
  }
}

// if p too small, the power forumula rounds to 0
double rsc_bonferroni_correction(double p, double numtests) {
  /* double correction = 1 - pow(1-p, numtests);
     if (correction)
     return correction;
     else
     return p*numtests; */
  return exp(LOGEV(log(numtests), log(p)));
}

double rsc_get_motif_evalue(MOTIF_T* m, ARRAY_T* bg_freqs) {
  int i;
  double eval = 1;
  ALPH_T alph;

  alph = get_motif_alph(m);

  for (i=0;i<get_motif_length(m);i++) {
    eval *=  (get_matrix_cell(i, alph_index(alph, 'A'), get_motif_freqs(m))
        * get_array_item(alph_index(alph, 'A'), bg_freqs)
        + get_matrix_cell(i, alph_index(alph, 'C'), get_motif_freqs(m))
        * get_array_item(alph_index(alph, 'C'), bg_freqs)
        + get_matrix_cell(i, alph_index(alph, 'G'), get_motif_freqs(m))
        * get_array_item(alph_index(alph, 'G'), bg_freqs)
        + get_matrix_cell(i, alph_index(alph, 'T'), get_motif_freqs(m)))
      * get_array_item(alph_index(alph, 'T'), bg_freqs);
  }
  return eval;
}

// copy a string into new memory of exactly the right size; create a "" string
// if the original is a NULL pointer
char * my_strdup(const char *s1) {
  if (s1) {
    return strdup (s1);
  } else {
    char * newstr = malloc(sizeof(char));
    newstr[0] = '\0';
    return newstr;
  }
}

// create a new instance of a result, with a given motif and initial values
// if a non-null pointer is passed in, use its value, otherwise pass overwrite
// the location pointed to by new_result; in either case return new_result
rsc_result_t * init_result (rsc_result_t *new_result,
    int db_idx,
    MOTIF_T *motif,
    int split,
    double pleft,
    double pright,
    double pboth,
    double u) {
  if (!new_result) new_result = malloc(sizeof(rsc_result_t));
  new_result->u = u;
  new_result->split = split;
  new_result->pleft = pleft;
  new_result->pright = pright;
  new_result->pboth = pboth;
  new_result->db_idx = db_idx;
  new_result->motif = duplicate_motif(motif);
  return new_result;
}


