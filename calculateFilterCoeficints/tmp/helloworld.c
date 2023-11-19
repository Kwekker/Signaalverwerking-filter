#include <stdio.h>
#include <stdlib.h>
#include <argp.h>

const char *argp_program_version =
  "cfc3 0.1";
const char *argp_program_bug_address =
  "<example@example.com>";

/* Program documentation. */
static char doc[] =
  "cfc3 stands for calculate digital filter coefficients 3 where 3 means three analog poles.";

/* A description of the arguments we accept. */
static char args_doc[] = "pole1 pole2 pole3";

/* The options we understand. */
static struct argp_option options[] = {
  { 0, 0, 0, 0, "Required settings:", 1},
  {"output",   'o', "FILE", 0,
   "Save the digital filter coefficients in FILE"},
  {"fsample", 'f', "NUM", 0, "Sample frequency"},
  { 0, 0, 0, 0, "Set level of details in the CLI:", 2},
  {"verbose",  'v', 0,      0,  "Produce verbose output"},
  {"quiet",    'q', 0,      0,  "Don't produce any output"},
  {"silent",   's', 0,      OPTION_ALIAS},
  {0}
};

/* Used by main to communicate with parse_opt. */
struct arguments {
  char *args[3];                /* arg1=pole1 & arg2=pole2 & arg3=pole3 */
  int silent, verbose;
  char *output_file;
  int fSample;
};

/* Parse a single option. */
static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  /* Get the input argument from argp_parse, which we
     know is a pointer to our arguments structure. */
  struct arguments *arguments = state->input;

  switch (key)
    {
    case 'q': case 's':
        arguments->silent = 1;
        break;
    case 'v':
        arguments->verbose = 1;
        break;
    case 'o':
        arguments->output_file = arg;
        break;
    case 'f':
        arguments->fSample = atoi(arg);
        break;

    case ARGP_KEY_ARG:
      if ( (state->arg_num >= 3) )
        /* Too many arguments. */
        argp_usage (state);

      arguments->args[state->arg_num] = arg;

      break;

    case ARGP_KEY_END:
      if (state->arg_num != 3) 
        /* Not enough arguments. */
        argp_usage (state);
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

/* Our argp parser. */
static struct argp argp = { options, parse_opt, args_doc, doc };

int main (int argc, char **argv) {
  struct arguments arguments;

  /* Default values. */
  arguments.silent = 0;
  arguments.verbose = 0;
  arguments.output_file = "-";

  /* Parse our arguments; every option seen by parse_opt will
     be reflected in arguments. */
  argp_parse (&argp, argc, argv, 0, 0, &arguments);

  printf ("ARG1 = %s\nARG2 = %s\nARG3 = %s\nsample Freq = %d\nOUTPUT_FILE = %s\n"
          "VERBOSE = %s\nSILENT = %s\n",
          arguments.args[0], arguments.args[1], arguments.args[2],
          arguments.fSample,
          arguments.output_file,
          arguments.verbose ? "yes" : "no",
          arguments.silent ? "yes" : "no");

  exit (0);
}

