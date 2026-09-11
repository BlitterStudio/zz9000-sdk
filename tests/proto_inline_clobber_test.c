/*
 * Source guard for Amiga inline LVO calls. Three invariants:
 *
 * 1. GCC 14+ makes it a hard error for a register-asm operand to also
 *    appear in the clobber list of the same asm statement, so every
 *    clobber list must exclude the registers bound as operands.
 * 2. Library calls may clobber the m68k scratch registers (d1, a0, a1),
 *    so each asm statement must account for every scratch register
 *    either by binding it as an operand or by clobbering it — callers
 *    must never find a stale value left in one.
 * 3. A scratch register that is bound as an operand must be bound
 *    read-write ("+r"): an input-only binding tells GCC the asm
 *    preserves the register, so GCC may keep using its pre-call value
 *    after the jsr — which the library call has just trashed. (Verified
 *    as a real miscompile with GCC 16.2.0b -O2 -mlra.) Registers the
 *    library ABI preserves (d2-d7, a2-a5, a6) stay input-only.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path)
{
  FILE *file;
  long length;
  char *data;

  file = fopen(path, "rb");
  if (!file) {
    return 0;
  }
  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return 0;
  }
  length = ftell(file);
  if (length < 0) {
    fclose(file);
    return 0;
  }
  if (fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return 0;
  }

  data = (char *)malloc((size_t)length + 1U);
  if (!data) {
    fclose(file);
    return 0;
  }
  if (fread(data, 1U, (size_t)length, file) != (size_t)length) {
    free(data);
    fclose(file);
    return 0;
  }

  data[length] = '\0';
  fclose(file);
  return data;
}

/* Collect the registers bound as operands in the statement text, from
 * constraint references like "r"(zz9k_a0) or "+r"(zz9k_a0), recording
 * whether each binding is read-write. Register variables are named
 * <prefix>_<reg>; bare parenthesised text like (a6) inside the asm
 * template has no underscore and is ignored. */
static void collect_operand_regs(const char *text, int bound_rw[16],
                                 int bound_ro[16])
{
  const char *p = text;

  while ((p = strchr(p, '(')) != 0) {
    char varname[64];
    size_t len = 0;
    const char *q;
    int rw = 0;

    /* the constraint string ends immediately before the operand paren:
     * ..."r"(name)... / ..."+r"(name)... — for "+r" the '+' sits three
     * characters before the paren (past 'r' and the closing quote) */
    q = p;
    while (q > text && (q[-1] == ' ' || q[-1] == '\t')) {
      q--;
    }
    if (q > text + 2 && q[-1] == '"' && q[-3] == '+') {
      rw = 1;
    }

    p++;
    q = p;
    while (*q && *q != ')' && *q != ' ' && len < sizeof(varname) - 1U) {
      varname[len++] = *q++;
    }
    varname[len] = '\0';
    q = strrchr(varname, '_');
    if (q != 0 && q[1] != '\0' && q[2] != '\0') {
      char reg = q[1];
      int num = q[2] - '0';
      if ((reg == 'a' || reg == 'd') && num >= 0 && num <= 7) {
        if (rw) {
          bound_rw[num * 2 + (reg == 'd')] = 1;
        } else {
          bound_ro[num * 2 + (reg == 'd')] = 1;
        }
      }
    }
  }
}

/* Mark the registers named in a clobber line like:
 *   : "cc", "memory", "d1", "a1");  */
static void collect_clobber_regs(const char *line, int clobbered[16])
{
  const char *s = line;

  while (*s == ' ' || *s == '\t') {
    s++;
  }
  while ((s = strchr(s, '"')) != 0) {
    if ((s[1] == 'a' || s[1] == 'd') && s[2] >= '0' && s[2] <= '7' &&
        s[3] == '"') {
      clobbered[(s[2] - '0') * 2 + (s[1] == 'd')] = 1;
    }
    s++;
  }
}

/* Constraint lines start (after indentation) with ': "'. The clobber
 * line is the only one without a parenthesised operand. */
static int is_constraint_line(const char *line)
{
  const char *s = line;

  while (*s == ' ' || *s == '\t') {
    s++;
  }
  return s[0] == ':' && s[1] == ' ' && s[2] == '"';
}

static int is_clobber_line(const char *line)
{
  const char *s = line;

  while (*s == ' ' || *s == '\t') {
    s++;
  }
  return is_constraint_line(line) && strchr(s, '(') == 0;
}

static const char *reg_name(int idx)
{
  static const char *names[16] = {
    "a0", "d0", "a1", "d1", "a2", "d2", "a3", "d3",
    "a4", "d4", "a5", "d5", "a6", "d6", "a7", "d7"
  };
  return names[idx];
}

int main(int argc, char **argv)
{
  static const int scratch_d1 = 1 * 2 + 1;
  static const int scratch_a0 = 0 * 2 + 0;
  static const int scratch_a1 = 1 * 2 + 0;
  static const int scratch[3] = {0, 0, 0}; /* unused size marker */
  const int scratch_idx[3] = {scratch_a0, scratch_a1, scratch_d1};
  char *source;
  char *line;
  int ok = 1;
  int checked = 0;

  (void)scratch;
  if (argc != 2) {
    printf("usage: %s <proto/zz9k.h>\n", argv[0]);
    return 2;
  }

  source = read_file(argv[1]);
  if (!source) {
    printf("failed to read %s\n", argv[1]);
    return 2;
  }

  line = strtok(source, "\n");
  while (line != 0) {
    char *stmt[256];
    int n = 0;
    int bound_rw[16] = {0};
    int bound_ro[16] = {0};
    int clobbered[16] = {0};
    int i;

    if (strstr(line, "__asm volatile(") == 0) {
      line = strtok(0, "\n");
      continue;
    }
    stmt[n++] = line;
    line = strtok(0, "\n");
    while (line != 0 && n < 256 && !is_clobber_line(line)) {
      stmt[n++] = line;
      line = strtok(0, "\n");
    }
    if (line == 0 || n == 256) {
      printf("unterminated asm statement\n");
      ok = 0;
      break;
    }
    stmt[n++] = line;
    line = strtok(0, "\n");

    {
      char *joined;
      size_t len = 0;

      for (i = 0; i < n - 1; i++) {
        len += strlen(stmt[i]) + 1U;
      }
      joined = (char *)malloc(len + 1U);
      if (!joined) {
        printf("out of memory\n");
        ok = 0;
        break;
      }
      joined[0] = '\0';
      for (i = 0; i < n - 1; i++) {
        strcat(joined, stmt[i]);
        strcat(joined, "\n");
      }
      collect_operand_regs(joined, bound_rw, bound_ro);
      free(joined);
    }
    collect_clobber_regs(stmt[n - 1], clobbered);

    for (i = 0; i < 3; i++) {
      int idx = scratch_idx[i];
      int bound = bound_rw[idx] || bound_ro[idx];

      if (bound && clobbered[idx]) {
        printf("%s bound as operand AND clobbered in one asm\n",
               reg_name(idx));
        ok = 0;
      }
      if (bound_ro[idx] && !bound_rw[idx]) {
        printf("%s bound input-only: the library call trashes it, but GCC "
               "would treat it as preserved — bind it \"+r\"\n",
               reg_name(idx));
        ok = 0;
      }
      if (!(bound || clobbered[idx])) {
        printf("asm statement leaves scratch register %s "
               "neither bound nor clobbered\n",
               reg_name(idx));
        ok = 0;
      }
    }
    checked++;
  }

  if (checked == 0) {
    printf("no inline asm statements found\n");
    ok = 0;
  }

  printf("%d inline asm statements checked\n", checked);
  free(source);
  return ok ? 0 : 1;
}
