#include <unistd.h>

void simprint_int(long aVal) {
  long g1back;
  __asm__ __volatile__ (
     "mov  %%g1, %0     \n"
     "mov  %1, %%g1     \n"
     "sethi 0x666, %%g0 \n"
     "mov  %0, %%g1     \n"
   : "=r"  (g1back)
   : "r"  (aVal)
   );
}

void simprint_str(char const * aStr) {
  long g1back;
  __asm__ __volatile__ (
     "mov  %%g1, %0     \n"
     "mov  %1, %%g1     \n"
     "sethi 0x667, %%g0 \n"
     "mov  %0, %%g1     \n"
   : "=r"  (g1back)
   : "r"  (aStr)
   );
}

enum simprint_xact_marker_type {
  kStart = 1,
  kEndSuccess = 2,
  kEndFailed = 3
};

static struct simprint_xact_t {
  unsigned long long struct_version;
  unsigned long long pid;
  unsigned long long xact_num;
  unsigned long long xact_type;
  unsigned long long marker_type;
  unsigned long long canary;
} simprint_xact_;

void simprint_xact(long long aTransactionNumber, long long aTransactionType, xact_marker_type aMarkerType) {
  static long long pid = 0;
  if (pid == 0) {
    pid = getpid();
  }
  simprint_xact_.struct_version = 1;
  simprint_xact_.pid = pid;
  simprint_xact_.xact_num = aTransactionNumber;
  simprint_xact_.xact_type = aTransactionType;
  simprint_xact_.marker_type = aMarkerType;
  simprint_xact_.canary = 0xDEAD;

  long g1back;
  __asm__ __volatile__ (
     "mov  %%g1, %0     \n"
     "mov  %1, %%g1     \n"
     "sethi 0x668, %%g0 \n"
     "mov  %0, %%g1     \n"
   : "=r"  (g1back)
   : "r"  (&xact_)
   );
}

/* For testing:
int main() {

  int pid  = getpid();

  __asm__ __volatile__ ("sethi 0x40000, %g0");
  simprint_int(0xDEADBEEF);
  simprint_int(pid);
  simprint_str("Hello World!");
  simprint_xact(1, 1, kStart);
  simprint_xact(1, 1, kEndSuccess);
  printf("pid: %d", pid);

  __asm__ __volatile__ ("sethi 0x1, %g0");

}
*/