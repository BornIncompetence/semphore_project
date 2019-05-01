#include <algorithm>
#include <iostream>
#include <random>
#include <semaphore.hpp>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>

constexpr auto buffer_size = 10 * sizeof(char);

void produce();
void consume();
void init_chars();
std::mt19937 init_rng();
template <size_t N> void init_chars(std::mt19937 &, int (&)[N]);

struct group {
  unsigned char a[512];
  unsigned char b[512];
  unsigned char c[512];
};

int main() {
  struct {
    std::array<int, 4> ids{};
    std::array<char *, 4> buffers{};
  } mem_spaces;

  group g{};
  
  auto rng = init_rng();
  std::uniform_int_distribution dist{'a', 'z'};
  auto init = [&](auto &&arr) {
    for (auto i = 0; i < 512; ++i) {
      arr[i] = static_cast<char>(dist(rng));
    }
  };

  init(g.a);
  init(g.b);
  init(g.c);

  // for (const auto &i : {0, 1, 2, 3}) {
  //   const auto id = shmget(IPC_PRIVATE, buffer_size, PERMS);
  //   mem_spaces.ids[i] = id;
  //   mem_spaces.buffers[i] = static_cast<char *>(shmat(id, nullptr, SHM_RND));
  // }

  // if (fork()) {
  //   produce();
  // } else {
  //   consume();
  // }
}

std::mt19937 init_rng() {
  std::array<uint32_t, 624> states{};
  std::random_device rd;
  std::generate(states.begin(), states.end(), std::ref(rd));
  std::seed_seq seeds(states.begin(), states.end());
  return std::mt19937{seeds};
}
