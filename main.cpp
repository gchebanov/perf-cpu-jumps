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

void test1(uint64_t mem) {
  unsigned char code0[] = {
    0x31, 0xc0,
    0xe9, 0x02, 0x00, 0x00, 0x00,
    0xff, 0xc0,
    0xff, 0xc0,
    0xc3
  };
#ifdef WIN32
  auto const buf = VirtualAlloc(nullptr, mem, MEM_COMMIT, PAGE_READWRITE);
#else
  auto const buf = mmap(nullptr, mem, PROT_READ | PROT_WRITE | PROT_EXEC,
       MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
#endif
  size_t n = mem / sizeof(M);
  M* x = (M*)buf;
  init_rnd(x, n);
  M* y = x->prev;
  for (size_t i = 0; i < n; ++i) {
    int64_t djump = (char*)x[i].next - (char*)(x + i);
    memset(x + i, 0x90, sizeof(M));
    djump -= 5; // op size
    auto ip = (unsigned char*)(x + i);
    *ip++ = 0xe9;
    if ((int32_t)djump != djump) exit(1);
    *(uint32_t*)(ip) = (int32_t)djump;
  }
  memcpy(y, code0, sizeof(code0));
#ifdef WIN32
  DWORD dummy;
  VirtualProtect(buf, mem, PAGE_EXECUTE_READ, &dummy);
#endif
  auto const f = reinterpret_cast<std::int32_t(*)()>(buf);
  auto next_t = rdtsc();
  size_t next_n = 0;
  for (; next_n < 1; next_n += n) {
    auto const result = f();
    if (result != 1) exit(1);
  }
  next_t = rdtsc() - next_t;
  cout << "ticks on code jump: " << next_t / next_n << endl;
#ifdef WIN32
  VirtualFree(buf, 0, MEM_RELEASE);
#else
  munmap(buf, mem);
#endif
}

int main() {
  uint64_t mem = (uint64_t(1e9));
  if (mem & 0xfff) mem = (mem + 0xfff) & ~0xfff;
  test0(mem);
  test1(mem);
  return 0;
}
