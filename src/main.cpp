#include <algorithm>
#include <charconv>
#include <iostream>
#include <random>
#include <semaphore.hpp>
#include <sstream>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

constexpr auto CHUNK = 512;

struct group {
  unsigned char a[CHUNK];
  unsigned char b[CHUNK];
  unsigned char c[CHUNK];
};

struct shared_memory {
  std::array<int, 4> ids{};
  std::array<group *, 4> buffers{};
};

enum { PUT_ITEM, TAKE_ITEM };

void swap(unsigned char *, unsigned char *, int);
void child_process(SEMAPHORE &, shared_memory &, int);
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
    for (auto i = 0; i < CHUNK; ++i) {
      arr[i] = static_cast<unsigned char>(dist(rng));
    }
  };

  // Initialize semaphore here
  SEMAPHORE s(4);
  s.V(PUT_ITEM);
  s.V(PUT_ITEM);
  s.V(PUT_ITEM);
  s.V(PUT_ITEM);
  s.V(PUT_ITEM);

  // Initialize shared memory locations
  for (const auto i : {0, 1, 2, 3}) {
    const auto id = shmget(IPC_PRIVATE, sizeof(group), PERMS);
    mem_spaces.ids[i] = id;
    mem_spaces.buffers[i] = static_cast<group *>(shmat(id, nullptr, SHM_RND));
  }

  // Initialize shared memory locations with random letters
  init_char(mem_spaces.buffers[0]->a, lower_dist);
  init_char(mem_spaces.buffers[0]->b, lower_dist);
  init_char(mem_spaces.buffers[0]->c, lower_dist);
  init_char(mem_spaces.buffers[1]->a, upper_dist);
  init_char(mem_spaces.buffers[1]->b, upper_dist);
  init_char(mem_spaces.buffers[1]->c, upper_dist);
  init_char(mem_spaces.buffers[2]->a, upper_dist);
  init_char(mem_spaces.buffers[2]->b, upper_dist);
  init_char(mem_spaces.buffers[2]->c, upper_dist);
  init_char(mem_spaces.buffers[3]->a, upper_dist);
  init_char(mem_spaces.buffers[3]->b, upper_dist);
  init_char(mem_spaces.buffers[3]->c, upper_dist);

  // Prompt user for number of operations to be performed
  std::cout << "Enter the number of swaps you want each process to perform. ";
  int swaps = 1;
  /*[]() {
    std::string line;
    std::getline(std::cin, line);
    return std::stoi(line);
  }()*/

  // Create 5 children
  std::vector<int> children{};
  for (auto i = 0; i < 5; ++i) {
    const auto pid = fork();
    if (pid > 0) {
      children.push_back(pid);
    } else if (pid == 0) {
      child_process(s, mem_spaces, swaps);
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

void swap(unsigned char *a, unsigned char *b, int n) {
  for (auto i = 0; i < n; ++i) {
    const auto temp = a[i];
    b[i] = a[i];
    a[i] = temp;
  }
}

void child_process(SEMAPHORE &s, shared_memory &memory, int swaps) {
  std::mt19937 rng{std::random_device{}()};

  slowdown(rng);
  const auto [chunk1, chunk2] = random_chunks(rng);
  const auto [group1, group2] = random_groups(rng);
  for (auto i = 0; i < swaps; ++i) {
    s.P(PUT_ITEM);
    const auto f = [&](uint32_t g, uint32_t c) -> unsigned char * {
      switch (c) {
      case 0:
        return memory.buffers[g]->a;
      case 1:
        return memory.buffers[g]->b;
      default:
        return memory.buffers[g]->c;
      }
    };
    swap(f(chunk1, group1), f(chunk2, group2), CHUNK);
    s.V(TAKE_ITEM);
  }
}

// Slowdown the execution of the child process
void slowdown(std::mt19937 &rng) {
  std::uniform_int_distribution<uint32_t> u32dist{};
  while (u32dist(rng) >= 5000)
    ;
}

// Get two random numbers between 0 and 2
std::pair<uint32_t, uint32_t> random_chunks(std::mt19937 &rng) {
  std::uniform_int_distribution dist{0, 2};
  return {dist(rng), dist(rng)};
}

// Get two random numbers between 0 and 3
std::pair<uint32_t, uint32_t> random_groups(std::mt19937 &rng) {
  std::uniform_int_distribution dist{0, 3};
  return {dist(rng), dist(rng)};
}
