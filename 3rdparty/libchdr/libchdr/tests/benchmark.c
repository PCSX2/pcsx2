#include <libchdr/chd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

int main(int argc, char** argv)
{
  chd_error err;
  chd_file* file;
  const chd_header* header;
  void* buffer;
  int i;
  unsigned int totalbytes;
  clock_t start, end;
  double time_taken;

  printf("\nlibchdr benchmark tool....");

  /* Recording the starting clock tick.*/
  start = clock(); 
  
  /* Sequential read all hunks */
  err = chd_open(argv[1], CHD_OPEN_READ, NULL, &file);
  if (err)
	printf("\nchd_open() error: %s", chd_error_string(err));
  header = chd_get_header(file);
  totalbytes = header->hunkbytes * header->totalhunks;
  buffer = malloc(header->hunkbytes);
  for (i = 0 ; i < header->totalhunks ; i++)
  {
    err = chd_read(file, i, buffer);
    if (err)
      printf("\nchd_read() error: %s", chd_error_string(err));
  }
  free(buffer);
  chd_close(file);

  /* Recording the end clock tick. */
  end = clock();

  /* Calculating total time taken by the program. */
  time_taken = ((double)(end - start)) / ((double)CLOCKS_PER_SEC);

  /* Print results */
  printf("\nRead %d bytes in %lf seconds", totalbytes, time_taken);
  printf("\nRate is %lf MB/s", (((double)totalbytes)/(1024*1024)) / time_taken);
  printf("\n\n");
  return 0;
}
