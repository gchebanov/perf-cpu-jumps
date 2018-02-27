#include <iostream>
#include "x86intrin.h"

#ifdef WIN32
#include <memoryapi.h>
#else
#include <sys/mman.h>
#endif

using namespace std;

uint64_t rdtsc() {
  return __builtin_ia32_rdtsc();
}

struct M {
  M *next, *prev;
  char pad[64 - sizeof(next) - sizeof(prev)];
};

struct tlog {
  const char* comm;
  uint64_t cnt;
  tlog(const char* comm) : comm(comm), cnt(rdtsc()) {}
  ~tlog() { if (comm) cout << comm << ' '; cout << (rdtsc() - cnt) << endl; }
};

void init_rnd(M* x, size_t n) {
  tlog("init rnd");
  x[0].next = x[0].prev = x;
  srand(0);
  for (size_t i = 1; i < n; ++i) {
    size_t j = rand() % i;
    x[i].next = x[j].next;
    x[i].prev = x + j;
    x[i].next->prev = x + i;
    x[i].prev->next = x + i;
  }
}

void test0 (uint64_t mem) {
  size_t n = mem / sizeof(M);
  M* x;
  { tlog("alloc");
    x = new M[n];
  }
  init_rnd(x, n);
  auto next_t = rdtsc();
  size_t next_n = 0;
  for (; next_n < 1; next_n += n) {
    M* y = x;
    do {
      y = y->next;
    } while (y != x);
  }
  next_t = rdtsc() - next_t;
  cout << "ticks on data jump: " << next_t / next_n << endl;
}

void test1(uint64_t mem, bool conditional = false) {
  int code0_off = 32;
  unsigned char code0[] = {
    0x53, 0x51,
    0x31, 0xc0,
    0x31, 0xdb,
    0x31, 0xc9,
    0xff, 0xc0,
    0xe9, (unsigned char)(0xff - 5 - 10 - code0_off + 1), 0xff, 0xff, 0xff,
  };
  unsigned char code1[] = {
    0x59, 0x5b,
    0xc3
  };
#ifdef WIN32
  auto const buf = VirtualAlloc(nullptr, mem, MEM_COMMIT, PAGE_READWRITE);
#else
  auto const buf = mmap(nullptr, mem, PROT_READ | PROT_WRITE | PROT_EXEC,
       MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
#endif
  size_t n = mem / sizeof(M);
  M *x = (M*)buf;
  init_rnd(x, n);
  M *y = x->prev;
  uint32_t eax = 1;
  { size_t i = 0; do {
    size_t ni = x[i].next - x;
    int32_t djump = (char*)x[i].next - (char*)(x + i);
    memset(x + i, 0x90, sizeof(M));
    auto ip = (unsigned char*)(x + i);
    if (!conditional) {
      djump -= 5; // op size
      *ip++ = 0xe9;
      if ((int32_t) djump != djump) exit(1);
      *(int32_t *) (ip) = (int32_t) djump;
    } else {
      *ip++ = 0x67;
      *ip++ = 0x8d;
      *ip++ = 0x1c;
      *ip++ = 0x80; //lea
      *ip++ = 0x89;
      *ip++ = 0xd8; // mov
      *ip++ = 0xc1;
      *ip++ = 0xeb;
      *ip++ = 0x11; // shr
      *ip++ = 0x0f;
      *ip++ = 0x82; //jb
      eax *= 5;
      bool cf = (eax >> (0x11 - 1)) & 1;
      int32_t rjump = rand() % (2 * n) - n;
      if (!cf) swap(djump, rjump);
      *(uint32_t *) ip = (uint32_t) (djump + (unsigned char *) (x + i) - ip - 4);
      ip += 4;
      *ip++ = 0xe9;
      *(uint32_t *) ip = (uint32_t) (rjump + (unsigned char *) (x + i) - ip - 4);
    }
    i = ni;
  } while (i != 0); }
  memcpy((char*)x + code0_off, code0, sizeof(code0));
  memset(y, 0x90, sizeof(M));
  memcpy(y, code1, sizeof(code1));
#ifdef WIN32
  DWORD dummy;
  VirtualProtect(buf, mem, PAGE_EXECUTE_READ, &dummy);
#endif
  auto const f = reinterpret_cast<std::int32_t(*)()>((char*)buf + code0_off);
  auto next_t = rdtsc();
  size_t next_n = 0;
  for (; next_n < 1; next_n += n) {
    auto const result = f();
    if (conditional) {
      if ((5 * result) != eax) exit(1);
    } else {
      if (result != 1) exit(1);
    }
  }
  next_t = rdtsc() - next_t;
  cout << "ticks on code " << (conditional ? "" : "un") << "conditional jump: " << next_t / next_n << endl;
#ifdef WIN32
  VirtualFree(buf, 0, MEM_RELEASE);
#else
  munmap(buf, mem);
#endif
}

int main() {
  uint64_t mem = (uint64_t(1e9));
  // mem = 1;
  if (mem & 0xfff) mem = (mem + 0xfff) & ~0xfff;
  test0(mem);
  test1(mem, false);
  test1(mem, true);
  return 0;
}
