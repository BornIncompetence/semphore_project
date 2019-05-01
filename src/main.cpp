#include <algorithm>
#include <iostream>
#include <random>
#include <semaphore.hpp>
#include <sstream>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

constexpr auto chunk = 512;

struct group {
  unsigned char a[chunk];
  unsigned char b[chunk];
  unsigned char c[chunk];
};

struct shared_memory {
  std::array<int, 4> ids{};
  std::array<char *, 4> buffers{};
};

void child_process(shared_memory &);
void slowdown(std::mt19937 &);
std::pair<uint32_t, uint32_t> random_chunks(std::mt19937 &rng);
std::pair<uint32_t, uint32_t> random_groups(std::mt19937 &rng);

int main() {
  shared_memory mem_spaces{};
  group g{};

  // Initialize shared memory buffers
  std::mt19937 rng{std::random_device{}()};
  std::uniform_int_distribution lower_dist{'a', 'z'};
  std::uniform_int_distribution upper_dist{'A', 'Z'};
  auto init_char = [&](auto &&arr, auto &&dist) {
    for (auto i = 0; i < chunk; ++i) {
      arr[i] = static_cast<char>(dist(rng));
    }
  };

  // Test example of initialization
  init_char(g.a, lower_dist);
  init_char(g.b, upper_dist);
  init_char(g.c, upper_dist);

  for (const auto i : {0, 1, 2, 3}) {
    const auto id = shmget(IPC_PRIVATE, sizeof(group), PERMS);
    mem_spaces.ids[i] = id;
    mem_spaces.buffers[i] = static_cast<char *>(shmat(id, nullptr, SHM_RND));
  }

  // Create 5 children
  std::vector<int> children{};
  for (auto i = 0; i < 5; ++i) {
    const auto pid = fork();
    if (pid > 0) {
      children.push_back(pid);
    } else if (pid == 0) {
      child_process(mem_spaces);
      exit(0);
    } else if (pid < 0) {
      std::cout << "Could not create process.\n";
    }
  }

  // Print parent PID and list of children PIDs
  std::cout << [&]() -> std::string {
    std::stringstream fmt{};
    fmt << '[' << getpid() << "]: Waiting on children ";
    for (auto &&i : children) {
      fmt << i << ' ';
    }
    auto str = fmt.str();
    str.back() = '\n';
    return str;
  }();

  // Wait for all five children to finish
  for (auto &&_ : children) {
    wait(nullptr);
  }

  // Deallocate memory sections
  // (this has to be done by last child in final turn-in)
  for (const auto i : {0, 1, 2, 3}) {
    shmdt(mem_spaces.buffers[i]);
    shmctl(mem_spaces.ids[i], IPC_RMID, nullptr);
  }
}

void child_process(shared_memory &memory) {
  std::mt19937 rng{std::random_device{}()};

  slowdown(rng);
  const auto [chunk1, chunk2] = random_chunks(rng);
  const auto [group1, group2] = random_groups(rng);

  /* Begin atomic swap here */
  std::cout << '[' << getpid() << "]: Chunks: (" << chunk1 << ", " << chunk2
            << ") Groups: (" << group1 << ", " << group2 << ")\n";
}

void slowdown(std::mt19937 &rng) {
  std::uniform_int_distribution<uint32_t> u32dist{};
  while (u32dist(rng) >= 5000)
    ;
}

std::pair<uint32_t, uint32_t> random_chunks(std::mt19937 &rng) {
  std::uniform_int_distribution dist{0, 2};
  return {dist(rng), dist(rng)};
}

std::pair<uint32_t, uint32_t> random_groups(std::mt19937 &rng) {
  std::uniform_int_distribution dist{0, 3};
  return {dist(rng), dist(rng)};
}
