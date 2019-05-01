// This software may be used in your CECS326 programs

// simple implementation of SEMAPHORE class with some error
// and signal handling

#include <semaphore.hpp>

SEMAPHORE::SEMAPHORE(int size) {
  this->size = size;
  sem_id = semget(IPC_PRIVATE, size, PERMS);
  init();
}

int SEMAPHORE::remove() {
  return semctl(sem_id, 0 /*not used*/, IPC_RMID, semun{});
}

int SEMAPHORE::P(int id) {
  int ret;
  struct sembuf *p = &p_set[id];
  while (((ret = semop(sem_id, p, 1)) == -1) && (errno == EINTR))
    ;
  return ret;
}

int SEMAPHORE::V(int id) {
  int ret;
  struct sembuf *v = &v_set[id];
  while (((ret = semop(sem_id, v, 1)) == -1) && (errno == EINTR))
    ;
  return ret;
}

void SEMAPHORE::set_semaphore_buffer_p(int k, int op, int flg) {
  p_set[k].sem_num = (short)k;
  p_set[k].sem_op = op;
  p_set[k].sem_flg = flg;
}

void SEMAPHORE::set_semaphore_buffer_v(int k, int op, int flg) {
  v_set[k].sem_num = (short)k;
  v_set[k].sem_op = op;
  v_set[k].sem_flg = flg;
}

int SEMAPHORE::init() {
  p_set = std::make_unique<sembuf[]>(size);
  v_set = std::make_unique<sembuf[]>(size);
  for (int k = 0; k < size; k++) {
    set_semaphore_buffer_p(k, -1, 0 /*suspend*/);
    set_semaphore_buffer_v(k, 1, 0 /*suspend*/);
  }

  // initialize all to zero
  semun arg{};
  ushort init_v[size];
  for (int k = 0; k < size; k++) {
    init_v[k] = 0;
  }
  arg.array = init_v;
  return semctl(sem_id, size, SETALL, arg);
}
