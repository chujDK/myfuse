#include "test_def.h"
#include "random"

// map blockno to it's expected content
std::map<int, const u_char*> contents;
const int content_sum = 1000;
// this array contains [0, MAX_BLOCK_NO), and then shuffled
std::array<int, MAX_BLOCK_NO> total_blockno;
// this array contains {content_sum} uniq random nums in range [0, MAX_BLOCK_NO)
std::array<int, content_sum> content_blockno;

void generate_test_data() {
  // init the global contents
  for (int i = 0; i < MAX_BLOCK_NO; i++) {
    total_blockno[i] = i;
  }

  static_assert(content_sum < MAX_BLOCK_NO,
                "content_sum must lower then MAX_BLOCK_NO");

  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(total_blockno.begin(), total_blockno.end(), g);
  std::copy(total_blockno.begin(),
            total_blockno.begin() + content_blockno.size(),
            content_blockno.begin());

  for (int& i : content_blockno) {
    auto buf = new u_char[BSIZE];
    for (int i = 0; i < BSIZE; i++) {
      buf[i] = std::rand() % 0x100;
    }
    contents[i] = buf;
  }
}

template <std::size_t MAX_WORKER = MAXOPBLOCKS>
void start_worker(void* (*pthread_worker)(void*)) {
  std::array<pthread_t, MAX_WORKER> workers;
  for (int i = 0; i < MAX_WORKER; i++) {
    auto range   = (struct start_to_end*)malloc(sizeof(struct start_to_end));
    range->start = i * (content_sum / MAX_WORKER);
    range->end   = (i + 1) * (content_sum / MAX_WORKER);
    pthread_create(&workers[i], nullptr, pthread_worker, range);
  }
  for (pthread_t worker : workers) {
    pthread_join(worker, nullptr);
  }
}
