// This software may be used in your CECS326 programs

#include <errno.h>
#include <memory>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define PERMS S_IWUSR | S_IRUSR

// need to be checking for existing definition
union semun {    // truncated definition
  int val;       // Value for SETVAL
  ushort *array; // Array for GETALL, SETALL
};

class SEMAPHORE {
private:
  int size;
  pid_t sem_id;
  std::unique_ptr<sembuf[]> p_set;
  std::unique_ptr<sembuf[]> v_set;

  int init();
  void set_semaphore_buffer_p(int, int, int);
  void set_semaphore_buffer_v(int, int, int);

public:
  // create a number of semaphores denoted by size
  // precondition: size is larger than zero
  // postcondition: all semaphores are initialized to zero
  explicit SEMAPHORE(int size);

  // deallocates all semaphores created by constructor
  // precondition: existence of SEMAPHORE object
  // postcondition: all semaphores are removed
  int remove();

  // semaphore P operation on semaphore semname
  // precondition: existence of SEMAPHORE object
  // postcondition: semaphore decrements and process may be blocked
  // return value: -1 denotes an error
  int P(int semname);

  // semaphore V operation on semaphore semname
  // precondition: existence of SEMAPHORE object
  // postcondition: semaphore increments and process may be resumed
  // return value: -1 denotes an error
  int V(int semname);
};
