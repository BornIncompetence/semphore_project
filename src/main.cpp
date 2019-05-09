#include <algorithm>
#include <iostream>
#include <random>
#include <semaphore.hpp>
#include <sstream>
#include <string.h>
#include <string_view>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

constexpr auto CHUNK = 512;
constexpr auto CHILDREN = 5;
constexpr auto GROUPS = 4;
constexpr auto SLOWDOWN = 5000;
constexpr auto VIEW_LIMIT = 64;

struct group {
  std::array<char, CHUNK> a;
  std::array<char, CHUNK> b;
  std::array<char, CHUNK> c;

  char *at(int i) {
    switch (i) {
    case 0:
      return a.data();
    case 1:
      return b.data();
    case 2:
      return c.data();
    default:
      throw std::out_of_range("Attempted to go out of bounds!");
    }
  }
};

struct shared_memory {
  std::array<int, 4> ids{};
  std::array<group *, 4> buffers{};
};

enum { C1, C2, C3, C4, C5 };

void child_process(SEMAPHORE &, shared_memory &, int, int, int, bool);
void slowdown(std::mt19937 &);
std::pair<uint32_t, uint32_t> random_chunks(std::mt19937 &rng);
std::pair<uint32_t, uint32_t> random_groups(std::mt19937 &rng);

int main() {
  shared_memory memory{};

  // Initialize shared memory buffers
  std::mt19937 rng{std::random_device{}()};
  std::uniform_int_distribution lower_dist{'a', 'z'};
  std::uniform_int_distribution upper_dist{'A', 'Z'};

  // Initializes a single array (or pointer) with random numbers constrained
  // within a uniform distribution range
  auto init_char = [&](auto &&arr, auto &&dist) {
    for (auto i = 0; i < CHUNK; ++i) {
      arr[i] = static_cast<char>(dist(rng));
    }
  };

  // Initialize semaphore here. C1 = 1; C2 = C3 = C4 = C5 = 0
  SEMAPHORE s(CHILDREN);
  s.V(C1);

  // Initialize shared memory locations
  for (auto i = 0; i < GROUPS; ++i) {
    const auto id = shmget(IPC_PRIVATE, sizeof(group), PERMS);
    memory.ids[i] = id;
    memory.buffers[i] = static_cast<group *>(shmat(id, nullptr, SHM_RND));
  }

  // Generate random letters at shared memory
  init_char(memory.buffers[0]->a, lower_dist);
  init_char(memory.buffers[0]->b, lower_dist);
  init_char(memory.buffers[0]->c, lower_dist);
  init_char(memory.buffers[1]->a, upper_dist);
  init_char(memory.buffers[1]->b, upper_dist);
  init_char(memory.buffers[1]->c, upper_dist);
  init_char(memory.buffers[2]->a, upper_dist);
  init_char(memory.buffers[2]->b, upper_dist);
  init_char(memory.buffers[2]->c, upper_dist);
  init_char(memory.buffers[3]->a, upper_dist);
  init_char(memory.buffers[3]->b, upper_dist);
  init_char(memory.buffers[3]->c, upper_dist);

  // Print out contents of shared memory
  for (auto i = 0; i < GROUPS; ++i) {
    std::cout << '[' << i << "][" << 0 << "]: "
              << std::string_view(memory.buffers[i]->a.data(), VIEW_LIMIT)
              << "\n[" << i << "][" << 1 << "]: "
              << std::string_view(memory.buffers[i]->b.data(), VIEW_LIMIT)
              << "\n[" << i << "][" << 2 << "]: "
              << std::string_view(memory.buffers[i]->c.data(), VIEW_LIMIT)
              << "\n\n";
  }

  // Prompt user for number of operations to be performed
  std::cout << "Enter the number of swaps you want each process to perform. \n";
  const int swaps = []() {
    std::string line;
    std::getline(std::cin, line);
    try {
      return std::stoi(line);
    } catch (const std::exception &e) {
      std::cout << "Warning: Could not read line, using 3 swaps instead.\n";
      return 3;
    }
  }();

  // Return an enum based on which child is receiving it
  const auto semvar = [](int i) {
    switch (i) {
    case 0:
      return C1;
    case 1:
      return C2;
    case 2:
      return C3;
    case 3:
      return C4;
    case 4:
      return C5;
    default:
      return C1;
    }
  };

  // Attempt to create 5 children (the program will continue if a child fails)
  std::vector<int> children{};
  for (auto i = 0; i < CHILDREN; ++i) {
    const auto pid = fork();
    if (pid > 0) {
      children.push_back(pid);
    } else if (pid == 0) {
      child_process(s, memory, swaps, semvar(i), semvar(i + 1),
                    i + 1 == CHILDREN);
      exit(0);
    } else if (pid < 0) {
      std::cout << "ERROR: Could not create process. The program will likely "
                   "stall indefinitely.\n";
      // Because the last semvar + 1 will not point back to C1
      exit(-1);
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

  // Wait for all children to finish
  for (auto &&_ : children) {
    wait(nullptr);
  }

  // Print out contents of shared memory
  for (auto i = 0; i < GROUPS; ++i) {
    std::cout << '[' << i << "][" << 0 << "]: "
              << std::string_view(memory.buffers[i]->a.data(), VIEW_LIMIT)
              << "\n[" << i << "][" << 1 << "]: "
              << std::string_view(memory.buffers[i]->b.data(), VIEW_LIMIT)
              << "\n[" << i << "][" << 2 << "]: "
              << std::string_view(memory.buffers[i]->c.data(), VIEW_LIMIT)
              << "\n\n";
  }
}

// Swap contents of two arrays each of size 512
void swap(char *arr1, char *arr2) {
  char temp[CHUNK];
  strncpy(temp, arr1, CHUNK);
  strncpy(arr1, arr2, CHUNK);
  strncpy(arr2, temp, CHUNK);
}

void child_process(SEMAPHORE &s, shared_memory &memory, int swaps, int p, int v,
                   bool last_process) {
  std::mt19937 rng{std::random_device{}()};

  const auto [chunk1, chunk2] = random_chunks(rng);
  const auto [group1, group2] = random_groups(rng);
  for (auto i = 0; i < swaps; ++i) {
    s.P(p);
    slowdown(rng);
    auto buf1 = memory.buffers[group1]->at(chunk1);
    auto buf2 = memory.buffers[group2]->at(chunk2);
    std::cout << '[' << getpid() << "]: Swapping memory at location: ["
              << group1 << "][" << chunk1 << "] (" << static_cast<void *>(buf1)
              << ") with [" << group2 << "][" << chunk2 << "] ("
              << static_cast<void *>(buf2) << ")\n";
    swap(memory.buffers[group1]->at(chunk1),
         memory.buffers[group2]->at(chunk2));
    s.V(v);
  }

  // Clean up by last process
  if (last_process) {
    std::cout << '[' << getpid() << "]: Cleaning up...\n";
    for (const auto i : {0, 1, 2, 3}) {
      shmdt(memory.buffers[i]);
      shmctl(memory.ids[i], IPC_RMID, nullptr);
    }
    s.remove();
  }
}

// Slowdown the execution of the child process
void slowdown(std::mt19937 &rng) {
  std::uniform_int_distribution<uint32_t> u32dist{};
  while (u32dist(rng) >= SLOWDOWN)
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
