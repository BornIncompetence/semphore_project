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

enum { PUT_ITEM, TAKE_ITEM };

void child_process(SEMAPHORE &, shared_memory &, int);
void slowdown(std::mt19937 &);
std::pair<uint32_t, uint32_t> random_chunks(std::mt19937 &rng);
std::pair<uint32_t, uint32_t> random_groups(std::mt19937 &rng);

int main() {
  shared_memory mem_spaces{};

  // Initialize shared memory buffers
  std::mt19937 rng{std::random_device{}()};
  std::uniform_int_distribution lower_dist{'a', 'z'};
  std::uniform_int_distribution upper_dist{'A', 'Z'};
  auto init_char = [&](auto &&arr, auto &&dist) {
    for (auto i = 0; i < CHUNK; ++i) {
      arr[i] = static_cast<char>(dist(rng));
    }
  };

  // Initialize semaphore here
  SEMAPHORE s(CHILDREN);
  for (auto _ = 0; _ < CHILDREN; ++_) {
    s.V(PUT_ITEM);
  }

  // Initialize shared memory locations
  for (auto i = 0; i < GROUPS; ++i) {
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

  // Print out contents of shared memory
  for (auto i = 0; i < GROUPS; ++i) {
    std::cout << '[' << i << "][" << 0 << "]: "
              << std::string_view(mem_spaces.buffers[i]->a.data(), CHUNK)
              << "\n[" << i << "][" << 1 << "]: "
              << std::string_view(mem_spaces.buffers[i]->b.data(), CHUNK)
              << "\n[" << i << "][" << 2 << "]: "
              << std::string_view(mem_spaces.buffers[i]->c.data(), CHUNK)
              << "\n\n";
  }

  // Prompt user for number of operations to be performed
  std::cout << "Enter the number of swaps you want each process to perform. \n";
  int swaps = 1;
  /*[]() {
    std::string line;
    std::getline(std::cin, line);
    return std::stoi(line);
  }()*/

  // Attempt to create 5 children (the program will continue if a child fails)
  std::vector<int> children{};
  for (auto i = 0; i < CHILDREN; ++i) {
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

  // Wait for all children to finish
  for (auto &&_ : children) {
    wait(nullptr);
  }

  // Print out contents of shared memory
  for (auto i = 0; i < GROUPS; ++i) {
    std::cout << '[' << i << "][" << 0 << "]: "
              << std::string_view(mem_spaces.buffers[i]->a.data(), CHUNK)
              << "\n[" << i << "][" << 1 << "]: "
              << std::string_view(mem_spaces.buffers[i]->b.data(), CHUNK)
              << "\n[" << i << "][" << 2 << "]: "
              << std::string_view(mem_spaces.buffers[i]->c.data(), CHUNK)
              << "\n\n";
  }

  // Deallocate memory sections
  // (this has to be done by last child in final turn-in)
  for (const auto i : {0, 1, 2, 3}) {
    shmdt(mem_spaces.buffers[i]);
    shmctl(mem_spaces.ids[i], IPC_RMID, nullptr);
  }
}

void swap(char *arr1, char *arr2) {
  char temp[512];
  strncpy(temp, arr1, CHUNK);
  strncpy(arr1, arr2, CHUNK);
  strncpy(arr2, temp, CHUNK);
}

void child_process(SEMAPHORE &s, shared_memory &memory, int swaps) {
  std::mt19937 rng{std::random_device{}()};

  slowdown(rng);
  const auto [chunk1, chunk2] = random_chunks(rng);
  const auto [group1, group2] = random_groups(rng);
  for (auto i = 0; i < swaps; ++i) {
    s.P(PUT_ITEM);
    auto buf1 = memory.buffers[group1]->at(chunk1);
    auto buf2 = memory.buffers[group2]->at(chunk2);
    std::cout << '[' << getpid() << "]: Swapping memory at location: ["
              << group1 << "][" << chunk1 << "] (" << static_cast<void *>(buf1)
              << ") with [" << group2 << "][" << chunk2 << "] ("
              << static_cast<void *>(buf2) << ")\n";
    swap(memory.buffers[group1]->at(chunk1),
         memory.buffers[group2]->at(chunk2));
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
