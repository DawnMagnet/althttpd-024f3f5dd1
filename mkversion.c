/*
** 这个C程序从“manifest”、“manifest.uuid”和“VERSION”文件中提取信息，
** 生成“VERSION.h”头文件。使用三个参数调用此程序：
**
**     ./a.out manifest.uuid manifest VERSION
**
** 注意，manifest.uuid和manifest文件是由Fossil生成的。
**
** 输出将成为“VERSION.h”文件。输出是一个C语言的头文件，
** 包含各种构建属性的#defines：
**
**   MANIFEST_UUID              这些值是文本字符串，
**   MANIFEST_VERSION           用于标识源代码树所属的Fossil check-in。
**                              它们不考虑任何未提交的编辑。
**
**   MANIFEST_DATE              源代码check-in的日期/时间
**   MANIFEST_YEAR              以各种格式表示。
**   MANIFEST_NUMERIC_DATE
**   MANIFEST_NUMERIC_TIME
**
**   RELEASE_VERSION            版本号（来自VERSION源文件）
**   RELEASE_VERSION_NUMBER     以各种格式表示。
**   RELEASE_RESOURCE_VERSION
**
** 将来可能会添加新的#defines。
*/
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* These macros are provided to \"stringify\" the value of the define
** for those options in which the value is meaningful. */
#define CTIMEOPT_VAL_(opt) #opt
#define CTIMEOPT_VAL(opt)  CTIMEOPT_VAL_(opt)

static FILE *open_for_reading(const char *zFilename) {
  FILE *f = fopen(zFilename, "r");
  if (f == 0) {
    fprintf(stderr, "cannot open \"%s\" for reading\n", zFilename);
    exit(1);
  }
  return f;
}

/*
** Given an arbitrary-length input string key zIn, generate
** an N-byte hexadecimal hash of that string into zOut.
*/
static void hash(const char *zIn, int N, char *zOut) {
  unsigned char i, j, t;
  int m, n;
  unsigned char s[256];
  // s[]为0-255的数组
  for (m = 0; m < 256; m++) {
    s[m] = m;
  }
  // 使用zIn[]的内容对s[]进行混淆(随机打乱)
  for (j = 0, m = n = 0; m < 256; m++, n++) {
    j += s[m] + zIn[n];
    if (zIn[n] == 0) {
      n = -1;
    }
    // 交换s[m]和s[s[m]+?]
    t = s[j];
    s[j] = s[m];
    s[m] = t;
  }
  i = j = 0;
  for (n = 0; n < N - 2; n += 2) {
    i++;
    t = s[i];
    j += t;
    s[i] = s[j];
    s[j] = t;
    t += s[i];
    zOut[n] = "0123456789abcdef"[(t >> 4) & 0xf];
    zOut[n + 1] = "0123456789abcdef"[t & 0xf];
  }
  zOut[n] = 0;
}

int main(int argc, char *argv[]) {
  FILE *m, *u, *v;
  char *z;
#if defined(__DMC__) /* e.g. 0x857 */
  int i = 0;
#endif
  int j = 0, x = 0, d = 0;
  size_t n;
  int vn[3];
  char b[1000];
  char vx[1000];
  if (argc != 4) {
    fprintf(stderr, "Usage: %s manifest.uuid manifest VERSION\n", argv[0]);
    exit(1);
  }
  memset(b, 0, sizeof(b));
  memset(vx, 0, sizeof(vx));
  u = open_for_reading(argv[1]);
  if (fgets(b, sizeof(b) - 1, u) == 0) {
    fprintf(stderr, "malformed manifest.uuid file: %s\n", argv[1]);
    exit(1);
  }
  fclose(u);
  for (z = b; z[0] && z[0] != '\r' && z[0] != '\n'; z++) {
  }
  *z = 0;
  printf("#define MANIFEST_UUID \"%s\"\n", b);
  printf("#define MANIFEST_VERSION \"[%10.10s]\"\n", b);
  m = open_for_reading(argv[2]);
  while (b == fgets(b, sizeof(b) - 1, m)) {
    if (0 == strncmp("D ", b, 2)) {
      int k, n;
      char zDateNum[30];
      printf("#define MANIFEST_DATE \"%.10s %.8s\"\n", b + 2, b + 13);
      printf("#define MANIFEST_YEAR \"%.4s\"\n", b + 2);
      n = 0;
      for (k = 0; k < 10; k++) {
        if (isdigit(b[k + 2]))
          zDateNum[n++] = b[k + 2];
      }
      zDateNum[n] = 0;
      printf("#define MANIFEST_NUMERIC_DATE %s\n", zDateNum);
      n = 0;
      for (k = 0; k < 8; k++) {
        if (isdigit(b[k + 13]))
          zDateNum[n++] = b[k + 13];
      }
      zDateNum[n] = 0;
      for (k = 0; zDateNum[k] == '0'; k++) {
      }
      printf("#define MANIFEST_NUMERIC_TIME %s\n", zDateNum + k);
    }
  }
  fclose(m);
  v = open_for_reading(argv[3]);
  if (fgets(b, sizeof(b) - 1, v) == 0) {
    fprintf(stderr, "malformed VERSION file: %s\n", argv[3]);
    exit(1);
  }
  fclose(v);
  for (z = b; z[0] && z[0] != '\r' && z[0] != '\n'; z++) {
  }
  *z = 0;
  printf("#define RELEASE_VERSION \"%s\"\n", b);
  z = b;
  vn[0] = vn[1] = vn[2] = 0;
  while (1) {
    if (z[0] >= '0' && z[0] <= '9') {
      x = x * 10 + z[0] - '0';
    } else {
      if (j < 3)
        vn[j++] = x;
      x = 0;
      if (z[0] == 0)
        break;
    }
    z++;
  }
  for (z = vx; z[0] == '0'; z++) {
  }
  printf("#define RELEASE_VERSION_NUMBER %d%02d%02d\n", vn[0], vn[1], vn[2]);
  memset(vx, 0, sizeof(vx));
  strcpy(vx, b);
  for (z = vx; z[0]; z++) {
    if (z[0] == '-') {
      z[0] = 0;
      break;
    }
    if (z[0] != '.')
      continue;
    if (d < 3) {
      z[0] = ',';
      d++;
    } else {
      z[0] = '\0';
      break;
    }
  }
  printf("#define RELEASE_RESOURCE_VERSION %s", vx);
  while (d < 3) {
    printf(",0");
    d++;
  }
  printf("\n");
#if defined(__clang__) && defined(__clang_major__)
  printf(
      "#define COMPILER \"clang-" CTIMEOPT_VAL(__clang_major__) "." CTIMEOPT_VAL(
          __clang_minor__) "." CTIMEOPT_VAL(__clang_patchlevel__) "\"\n");
#elif defined(__GNUC__) && defined(__VERSION__)
  printf("#define COMPILER \"gcc-" __VERSION__ "\"\n");
#else
  printf("#define COMPILER \"unknown\"\n");
#endif
  return 0;
}
