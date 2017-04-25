#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>

#ifdef HAVE_POWER8
#include "crc32c_ppc.h"
#elif HAVE_GOOD_YASM_ELF64
#include "crc32c_intel_fast.h"
#endif

#define DEFAULT_BUFLEN 512
#define DEFAULT_PERFTEST_BUFLEN 1073741824
#define DEFAULT_PERFTEST_ITERATIONS 10
#define BILLION 1000000000L /* used to convert sec to nsec */

typedef enum { false=0, true=1 } boolean;

struct crc_turbo_struct 
{
  uint32_t val[32][32];
} crc_turbo_table;

struct global
{
    /* flags and flag related values */
    boolean     b_flag;
    boolean     d_flag;  /* Use Daniel's table driven optimization. */
    boolean     f_flag;
    boolean     l_flag;
    boolean     n_flag;
    boolean     m_flag;  /* Use Mirantis's table driven optimization. */
    boolean     p_flag;
    boolean     t_flag;
    boolean     v_flag;
    boolean     s_flag;
    boolean     z_flag;
    int         b_value; /* length of buffer when -v and -l aren't specified */
    char       *f_value; /* filename */
    int         n_value; /* number of iterations used by perfteset */
    int         l_value; /* length of buffer to fill with -v value */
    uint32_t    s_value; /* nbytes worth of zeros to append */
    int         v_value; /* buffer filled with this value */
    int         z_value; /* nbytes worth of zeros to append */

    /* general */
    char       *prog_name;
    char       *bufP;
    int         buflen;
} g;

#ifdef HAVE_POWER8
extern uint32_t ceph_crc32c_ppc(uint32_t crc, unsigned char const *data, unsigned len);
#elif HAVE_GOOD_YASM_ELF64
extern uint32_t ceph_crc32c_intel_fast(uint32_t crc, unsigned char const *data, unsigned len);
#endif

void
Usage(void)
{
    fprintf(stderr, "Usage: %s\n", g.prog_name);
    fprintf(stderr, "    { -f <filename> | -v <value> -l <len> |\n");
    fprintf(stderr, "      -v <value> -l <len> |\n");
    fprintf(stderr, "      -s <startcrc> | \n");
    fprintf(stderr, "      -p }\n");
    fprintf(stderr, "    [ -z <zerolen> ] [-b <buflen> ] [-d] [-m] [-n] }\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "    -f <filename> : Read the file and calculate crc32c.\n");
    fprintf(stderr, "    -v <value>    : Fill in memory buffer with this\n");
    fprintf(stderr, "                  : value and calculate crc32c.\n");
    fprintf(stderr, "    -l <len>      : Length of in memory buffer to\n");
    fprintf(stderr, "                    fill with -v value.\n");
    fprintf(stderr, "    -s <startcrc> : Starting crc in hex.\n");
    fprintf(stderr, "    -p            : Perftest for fastzero.\n");
    fprintf(stderr, "    -n            : Number of iterations used by perftest.\n");
    fprintf(stderr, "    -z <zerolen>  : Length of zero'ed buffer to append.\n");
    fprintf(stderr, "    -b <buflen>   : Length of in memory buffer when\n");
    fprintf(stderr, "                    -v and -l are not specified.\n");
    fprintf(stderr, "    -d            : Use Daniel's optimized zero buffer code.\n");
    fprintf(stderr, "    -m            : Use Mirantis's optimized zero buffer code.\n");
    exit(1);
}

void
get_flags(int argc, char **argv)
{
    int rc;
    int nInputs = 0;

    /* Remember program name for Usage. */
    g.prog_name = argv[0];

    while((rc = getopt(argc, argv, "b:df:l:mn:ptv:s:z:")) != EOF) {
        switch(rc) {
        case 'b':
            g.b_flag = true;
            g.b_value = atoi(optarg);
            break;
        case 'd':
            g.d_flag = true;
            break;
        case 'f':
            g.f_flag = true;
            g.f_value = optarg;
            break;
        case 'l':
            g.l_flag = true;
            g.l_value = atoi(optarg);
            break;
        case 'm':
            g.m_flag = true;
            break;
        case 'n':
            g.n_flag = true;
            g.n_value = atoi(optarg);
            break;
        case 'p':
            g.p_flag = true;
            break;
        case 't':
            g.t_flag = true;
            break;
        case 's':
            g.s_flag = true;
            g.s_value = strtoul(optarg, NULL, 16);
            break;
        case 'v':
            g.v_flag = true;
            g.v_value = atoi(optarg);
            break;
        case 'z':
            g.z_flag = true;
            g.z_value = atoi(optarg);
            break;
        default:
            Usage();
            break;
        }
    }

    if (g.f_flag) nInputs++;
    if (g.l_flag || g.v_flag) nInputs++;
    if (g.p_flag) nInputs++;
    if (g.s_flag) nInputs++;

    if (nInputs > 1)
    {
        fprintf(stderr, "Error: Conflicting input methods specified.\n");
        fprintf(stderr, "  Either -p, -f, -s, or -v and -l.\n");
        Usage();
    }

    if ((g.l_flag && !g.v_flag) || (g.v_flag && !g.l_flag))
    {
        fprintf(stderr, "The -l and -v flags require each other.\n");
        Usage();
    }

    if ((g.d_flag || g.m_flag) && (!g.z_flag) && (!g.p_flag))
    {
        fprintf(stderr, "The -d and -m flags require the -z flag or -p flag.\n");
        Usage();
    }

    if (g.n_flag && !g.p_flag)
    {
        fprintf(stderr, "The -n flag requires the -p flag.\n");
        Usage();
    }
}

void check_architecture(void)
{
    int for_ppc = 0;
    int for_x86 = 0;

#ifdef HAVE_POWER8
    for_ppc = 1;
#elif HAVE_GOOD_YASM_ELF64
    for_x86 = 1;
#else
    fprintf(stderr, "Unable to detect hw architecture.\n");
    exit(1); 
#endif
}

static uint64_t
carryless_multiply(uint64_t xval, uint64_t yval)
{
    /* uint64_t crc_poly = 0x1edc6f41; */
    uint64_t crc_poly_plus = 0x11edc6f41;
    uint64_t prim = crc_poly_plus;
    uint64_t field_charac_full = (uint64_t) 1 << 32;
    uint32_t result = 0;

    while (yval > 0)
    {
        if (yval & 1)
            result = result ^ xval;

        yval = yval >> 1; /* equivalent to y / 2. */
        xval = xval << 1; /* equivalent to x*2. */

        if (xval & field_charac_full)
            xval = xval ^ prim;                                                          
    }                                                                                    
                                                                                         
    return result;                                                                       
}    

/*
 * Look: http://crcutil.googlecode.com/files/crc-doc.1.0.pdf
 * Here is implementation that goes 1 logical step further,
 * it splits calculating CRC into jumps of length 1, 2, 4, 8, ....
 * Each jump is performed on single input bit separately, xor-ed after that.
 */
void create_turbo_table(void)
{
  int b;
  int bit;
  int range;
  uint32_t crc_x;
  uint32_t crc_y;

  for (bit = 0 ; bit < 32 ; bit++) {
#ifdef HAVE_POWER8
    crc_turbo_table.val[0][bit] = ceph_crc32c_ppc(1UL << bit, NULL, 1);
#elif HAVE_GOOD_YASM_ELF64
    crc_turbo_table.val[0][bit] = ceph_crc32c_intel_fast(1UL << bit, NULL, 1);
#endif
  }
  for (range = 1; range <32 ; range++) {
    for (bit = 0 ; bit < 32 ; bit++) {
      crc_x = crc_turbo_table.val[range-1][bit];
      crc_y = 0;
      for (b = 0 ; b < 32 ; b++) {
        if ( (crc_x & (1UL << b)) != 0 ) {
          crc_y = crc_y ^ crc_turbo_table.val[range-1][b];
        }
      }
      crc_turbo_table.val[range][bit] = crc_y;
    }
  }
  return;
}

uint32_t ceph_crc32c_zeros(uint32_t crc, unsigned len)
{
  int b;
  int range = 0;
  uint32_t crc1;
  uint32_t mask;
  uint32_t *ptr;
  unsigned remainder = len & 15;

  len = len & ~15;

  while (len != 0) {
    crc1 = 0;
    if ((len & 1) == 1) {
      ptr = crc_turbo_table.val[range];

      while (crc != 0) {
        mask = ~((crc & 1) - 1);
        crc1 = crc1 ^ (mask & *ptr);
        crc = crc >> 1;
        ptr++;
      }
      crc = crc1;
    }
    len = len >> 1;
    range++;
  }
  if (remainder > 0)
  {
#ifdef HAVE_POWER8
    crc = ceph_crc32c_ppc(crc, NULL, remainder);
#elif HAVE_GOOD_YASM_ELF64
    crc = ceph_crc32c_intel_fast(crc, NULL, remainder);
#endif
  }
  return crc;
}

int perftest(void)
{
    struct timespec  start;
    struct timespec  end;
    uint64_t         diff_nsec = 0;
    uint64_t         total_diff_nsec = 0;
    uint64_t         avg_diff_nsec = 0;
    int              max_iter;
    int              iter;
    size_t           off = 0;
    size_t           nbytes = 0;
    size_t           nzerobytes = 0;
    uint32_t         crc;

    /* When checking zero buffer we want to start with a non-zero crc, 
     * otherwise all the results are going to be zero. 
     */
    crc = 1;

    /* Default to 1 GB buffer full of zero's. */
    if (g.z_flag) 
        nzerobytes = g.z_value;
    else
        nzerobytes = DEFAULT_PERFTEST_BUFLEN;

    /* Default to DEFAULT_PERFTEST_ITERATIONS. */
    if (g.n_flag) 
        max_iter = g.n_value;
    else
        max_iter = DEFAULT_PERFTEST_ITERATIONS;

    for (iter=0; iter < max_iter; iter++)
    {
	if (g.d_flag)
	{
	    clock_gettime(CLOCK_MONOTONIC, &start);
#ifdef HAVE_POWER8
	    crc = ceph_crc32c_ppc(crc, NULL, nzerobytes);
#elif HAVE_GOOD_YASM_ELF64
	    crc = ceph_crc32c_intel_fast(crc, NULL, nzerobytes);
#endif
	    clock_gettime(CLOCK_MONOTONIC, &end);
	}
	else if (g.m_flag)
	{
	    clock_gettime(CLOCK_MONOTONIC, &start);
	    crc = ceph_crc32c_zeros(crc, nzerobytes);
	    clock_gettime(CLOCK_MONOTONIC, &end);
	}
	else
	{
	    clock_gettime(CLOCK_MONOTONIC, &start);

            /* In this case we are timing how long it would take to
             * crc an actual buffer filled in with zeroes.  It will be done
             * in a single call if the buffer is large enough to fit all the
             * zeroes.  If not it will require multiple calls.
             */
            off = 0;

            do
            {
                /* Calc less than a buffer, a buffer at a time, or the remainder. */
                if (nzerobytes < g.buflen)
                    nbytes = nzerobytes;
                else if (off + g.buflen < nzerobytes)
                    nbytes = g.buflen;
                else
                    nbytes = nzerobytes - off;

#ifdef HAVE_POWER8
	        crc = ceph_crc32c_ppc(crc, (unsigned char const *) g.bufP, nbytes);
#elif HAVE_GOOD_YASM_ELF64
	        crc = ceph_crc32c_intel_fast(crc, (unsigned char const *) g.bufP, nbytes);
#endif

                off += nbytes;
	    } while (off < nzerobytes);

	    clock_gettime(CLOCK_MONOTONIC, &end);
	}

        /* Convert high resolution timers to nsec units.
         *
         * The end time must be greater than or equal to the start time, 
         * but the duration might cross a 1 sec boundary.  In that case 
         * the env.tv_sec - start.tv_sec would be > 0, but the 
         * end.tv_nsec - start.tv_nsec may be positive, negative, or 
         * even 0.  This means the tv_nsec value must be signed 
         * in order for this conversion calculation to work properly.
         */
        diff_nsec  = (BILLION * (end.tv_sec - start.tv_sec));
        diff_nsec += end.tv_nsec - start.tv_nsec;

	printf("%d crc=0x%08X\n", iter, crc);
	printf("  duration %lu nsec\n", diff_nsec);

        total_diff_nsec += diff_nsec;

	/* printf("  duration %lu sec %lu nsec\n", diff.tv_sec, diff.tv_nsec); */
	/* printf("  start %lu sec %lu nsec\n", start.tv_sec, start.tv_nsec); */
	/* printf("  end   %lu sec %lu nsec\n", end.tv_sec, end.tv_nsec); */
    }

    if (iter) avg_diff_nsec = total_diff_nsec / iter;
    printf("---\n");
    printf("avg duration %lu nsec across %d iterations\n", avg_diff_nsec, iter);
}

int main(int argc, char **argv)
{
    int rc=0;
    int iter=0;
    int fill_value=0;
    int fd=0;
    size_t off=0;
    size_t nbytes=0;
    size_t nzerobytes=0;
    size_t nread=0;
    uint32_t crc=0;

    memset(&g, 0, sizeof(struct global));

    get_flags(argc, argv);

    if (g.m_flag) create_turbo_table();

    /* Allocate a general use memory buffer.  The -l flag overrides 
     * others.  If that is not specified then -b flag overrides 
     * the default value.
     */
    if (g.l_flag)
        g.buflen = g.l_value;
    else if (g.b_flag)
        g.buflen = g.b_value;
    else
        g.buflen = DEFAULT_BUFLEN;

    g.bufP = malloc(g.buflen);
    if (!g.bufP)
    {
        fprintf(stderr, "malloc error\n");
        exit(1);
    }
    memset(g.bufP, 0, g.buflen);

    if (g.p_flag)
    {
        rc = perftest();
        goto xerror;
    }

    if (g.s_flag)
    {
        crc = g.s_value;
    }
    else if (g.v_flag && g.l_flag)
    {
        fill_value   = g.v_value;

	for(iter=0; iter<g.buflen; iter++)
	{
            g.bufP[iter] = fill_value;
	}

#ifdef HAVE_POWER8
	crc = ceph_crc32c_ppc(crc, (unsigned char const *) g.bufP, g.buflen);
#elif HAVE_GOOD_YASM_ELF64
	crc = ceph_crc32c_intel_fast(crc, (unsigned char const *) g.bufP, g.buflen);
#endif
    }
    else
    {
        fd = open(g.f_value, O_RDONLY);
        if (fd < 0)
        {
            fprintf(stderr, "Open error (errno=%d).\n", errno);
            exit(1);
        }

        while ((nread = read(fd, g.bufP, g.buflen)) > 0)
        {
            off = 0;
            do
            {
                nbytes = (size_t) nread - off;

                /* For now the size of what we're reading equals the size 
                 * of the chunk we're calculating the crc32c against, but 
                 * we'll leave this in just in case we ever want to calculate 
                 * smaller crc32c chunks of a larger buffer.
                 */
                if (nbytes > g.buflen) nbytes = g.buflen;

#ifdef HAVE_POWER8
	        crc = ceph_crc32c_ppc(crc, (unsigned char const *) g.bufP + off, nbytes);
#elif HAVE_GOOD_YASM_ELF64
	        crc = ceph_crc32c_intel_fast(crc, (unsigned char const *) g.bufP + off, nbytes);
#endif

                off += nbytes;
            } while (off < (size_t) nread);
        }

        close(fd);
    }

    if (g.z_flag && g.d_flag)
    {
        /* Calculate crc32c of zero'ed buffer with Daniel's fastzero optimization. */
        nzerobytes = g.z_value;
#ifdef HAVE_POWER8
	crc = ceph_crc32c_ppc(crc, (unsigned char const *) NULL, nzerobytes);
#elif HAVE_GOOD_YASM_ELF64
	crc = ceph_crc32c_intel_fast(crc, (unsigned char const *) NULL, nzerobytes);
#endif
    }
    else if (g.z_flag && g.m_flag)
    {
        /* Calculate crc32c of zero'ed buffer with Mirantis's fastzero optimization. */
        nzerobytes = g.z_value;
	crc = ceph_crc32c_zeros(crc, nzerobytes);
    }
    else if (g.z_flag)
    {
        /* Calculate crc32c of zero'ed buffer with no optimizations. */

        /* Make sure the buffer is now zero filled. */
        memset(g.bufP, 0, g.buflen);
        off = 0;
        nzerobytes = g.z_value;

        do
        {
            /* Calc less than a buffer, a buffer at a time, or the remainder. */
            if (nzerobytes < g.buflen)
                nbytes = nzerobytes;
            else if (off + g.buflen < nzerobytes)
                nbytes = g.buflen;
            else
                nbytes = nzerobytes - off;

#ifdef HAVE_POWER8
	    crc = ceph_crc32c_ppc(crc, (unsigned char const *) g.bufP, nbytes);
#elif HAVE_GOOD_YASM_ELF64
	    crc = ceph_crc32c_intel_fast(crc, (unsigned char const *) g.bufP, nbytes);
#endif

            off += nbytes;
	} while (off < nzerobytes);
    }

    printf("crc=0x%08X\n", crc);

xerror:
    if (g.bufP) 
    {
        free(g.bufP);
        g.bufP = NULL;
    }
    
    return rc;
}
