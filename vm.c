#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#define DEBUG
#include "vm.h"

void
print_traceback (vm_state_t state)
{
  fprintf (stderr, "fatal error: failed %s(), reason: %s\n",
	   state.traceback.function, state.traceback.reason);
}

__attribute__((noreturn))
     void halt_with_traceback (vm_state_t state, bool do_free)
{
  print_traceback (state);
  if (do_free)
    state.free ();
  exit (EXIT_FAILURE);
}

int
main (int argc, char **argv)
{
  if (argc < 2)
    {
      fprintf (stderr, "%s <path-to-binary>\n", argv[0]);
      return EXIT_FAILURE;
    }

  FILE *binary = fopen (argv[1], "r");
  struct stat binary_fstat;

  if (binary == NULL)
    {
      fprintf (stderr, "open() failed: %s\n", strerror (errno));
      return EXIT_FAILURE;
    }

  if (fstat (fileno (binary), &binary_fstat) < 0)
    {
      fprintf (stderr, "fstat() failed: %s\n", strerror (errno));
      fclose (binary);
      return EXIT_FAILURE;
    }

  char program_buffer[binary_fstat.st_size];
  size_t nread = fread (program_buffer,
			sizeof (*program_buffer),
			sizeof (program_buffer),
			binary);
  fclose (binary);

  if (nread != (sizeof (program_buffer) / sizeof (*program_buffer)))
    {
      fprintf (stderr, "fread() failed: expected %zu, read %zu\n",
	       binary_fstat.st_size, nread);
      return EXIT_FAILURE;
    }

  vm_state_t vm_state;
  if (!VM_initialize (&vm_state))
    halt_with_traceback (vm_state, true);
  if (!vm_state.load (program_buffer, nread))
    halt_with_traceback (vm_state, true);
  if (!vm_state.execute ())
    print_traceback (vm_state);

  vm_state.free ();

  return EXIT_SUCCESS;
}
