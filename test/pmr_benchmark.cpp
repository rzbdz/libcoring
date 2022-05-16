// pmr_benchmark.cpp
// Created by PanJunzhong on 5022/5/15.
//
#include <iostream>
#include <chrono>
#include <vector>
#include <array>
#include <list>
#include <coring/detail/skiplist_map.hpp>
#include <map>
using namespace std;
constexpr int SYSTEM_SIZE = 100;
constexpr int MAX = 10000;
volatile int val;
struct StopWatch {
  StopWatch() : clk{std::chrono::system_clock::now()} {}
  ~StopWatch() {
    auto now = std::chrono::system_clock::now();
    auto diff = now - clk;
    cout << chrono::duration_cast<chrono::milliseconds>(diff).count() << "ms" << endl;
  }
  decltype(std::chrono::system_clock::now()) clk;
};
struct ITests {
  virtual void sequential_ops() = 0;
  virtual void random_ops() = 0;
  virtual void access_ops() = 0;
  virtual void pop_ops() = 0;
};
template <typename Container>
struct SystemBase : ITests {
  std::vector<std::unique_ptr<Container>> subs{};
};

template <typename MapType>
struct MapTestsBase : SystemBase<MapType> {
  using Find = SystemBase<MapType>;
  void sequential_ops() override {
    auto &subs = Find::subs;
    for (int i = 0; i < subs.size(); i++) {
      for (int j = 0; j < 1000; j++) {
        subs[i]->emplace(100, -j);  // no fragments
        subs[i]->emplace(500, -j);  // no fragments}
        subs[i]->emplace(200, -j);  // no fragments}
        subs[i]->emplace(300, -j);  // no fragments}
      }
    }
    for (int i = 0; i < subs.size(); i++) {
      for (int j = 0; j < 100000; j++) {
        if (j % 4 == 0) {
          subs[i]->erase(j - 1);
        }
        if (j % 15 == 0) {
          auto first_larget = subs[i]->lower_bound(j - 1);
          [[maybe_unused]] vector<int> res;
          for (auto it = subs[i]->begin(); it != first_larget; it++) {
            res.push_back(it->second);
          }
          subs[i]->erase(subs[i]->begin(), first_larget);
          if (res.size() < 0) {
            std::cout << "oh" << std::endl;
          }
        }
        subs[i]->emplace(j, j - i);  // no fragments
      }
    }
  }
  void random_ops() override {
    auto &subs = Find::subs;
    for (int j = 0; j < 50000; j++) {
      for (int i = 0; i < subs.size(); i++) {
        subs[i]->emplace(j, j - i);  // no fragments
      }
    }
    for (int i = 0; i < subs.size(); i++) {
      for (int j = 0; j < 1000; j++) {
        subs[i]->emplace(100, -j);  // no fragments
        subs[i]->emplace(500, -j);  // no fragments}
        subs[i]->emplace(200, -j);  // no fragments}
        subs[i]->emplace(300, -j);  // no fragments}
      }
    }
    for (int i = 0; i < subs.size(); i++) {
      for (int tof = 0; tof < 50000; tof += 10) {
        // we want a average count
        //        for (int j = 0; j < 100; j++) {
        // now do some search
        //          if (subs[i]->find(tof) != subs[i]->end()) {
        //            auto first_larget = subs[i]->lower_bound(j - 1);
        //            [[maybe_unused]] vector<int> res;
        //            for (auto it = subs[i]->begin(); it != first_larget; it++) {
        //              res.push_back(it->second);
        //            }
        //            subs[i]->erase(subs[i]->begin(), first_larget);
        //            if (res.size() < 0) {
        //              std::cout << "oh" << std::endl;
        //            }
        //          }
        //        }
        subs[i]->erase(tof);
      }
    }
  }
  void access_ops() override {
    auto &subs = Find::subs;
    int count = 0;
    for (int j = 0; j < 5000; j++) {
      for (int i = 0; i < subs.size(); i++) {
        subs[i]->emplace(j, j - i);  // no fragments
        count += 0;
      }
    }
    for (int i = 0; i < subs.size(); i++) {
      for (int j = 0; j < 1000; j++) {
        subs[i]->emplace(100, -j);  // no fragments
        subs[i]->emplace(500, -j);  // no fragments}
        subs[i]->emplace(200, -j);  // no fragments}
        subs[i]->emplace(300, -j);  // no fragments}
        count += 4;
      }
    }
    cout << "emplace " << count << " elements";
    count = 0;
    for (int j = 0; j < 100; j++) {
      for (int i = 0; i < subs.size(); i++) {
        for (auto it = subs[i]->begin(); it != subs[i]->end(); it++) {
          count++;
          if (it->first == INT_MAX) {
            cout << "oops";
          }
        }
      }
    }
    cout << "accessed " << count / 100 << " elements" << endl;
  }
  void pop_ops() override {
    auto &subs = Find::subs;
    for (int j = 0; j < 5000; j++) {
      for (int i = 0; i < subs.size(); i++) {
        subs[i]->emplace(j, j - i);  // no fragments
      }
    }
    for (int i = 0; i < subs.size(); i++) {
      for (int j = 0; j < 1000; j++) {
        subs[i]->emplace(100, -j);  // no fragments
        subs[i]->emplace(500, -j);  // no fragments}
        subs[i]->emplace(200, -j);  // no fragments}
        subs[i]->emplace(300, -j);  // no fragments}
      }
    }
    for (int j = 1000; j < 5000; j += 50) { // here is a simulation of timing ticks
      for (int i = 0; i < subs.size(); i++) { // assume we have serveral timer single thread.
        auto it = subs[i]->find(j);
        for (auto b = subs[i]->begin(); b != it; b++) {
          b->second = -1;  // simulate some thing
        }
        subs[i]->erase(subs[i]->begin(), it);
        for (int k = 0; k < 100; k++) {
          subs[i]->emplace(5000 - k, k + 1);
        }
      }
    }
  }
};

struct MapSystem : MapTestsBase<std::multimap<int, int>> {
  MapSystem(int scale) {
    std::cout << "non-pmr.map" << std::endl;
    Find::subs.resize(scale);
    for (int i = 0; i < scale; i++) {
      Find::subs[i] = std::make_unique<std::multimap<int, int>>();
    }
  }
};

template <typename SkipType>
struct SkipTestsBase : SystemBase<SkipType> {
  using Find = SystemBase<SkipType>;
  void sequential_ops() override {
    auto &subs = Find::subs;
    for (int i = 0; i < subs.size(); i++) {
      for (int j = 0; j < 1000; j++) {
        subs[i]->emplace(100, -j);  // no fragments
        subs[i]->emplace(500, -j);  // no fragments}
        subs[i]->emplace(200, -j);  // no fragments}
        subs[i]->emplace(300, -j);  // no fragments}
      }
    }
    for (int i = 0; i < subs.size(); i++) {
      for (int j = 0; j < 100000; j++) {
        if (j % 4 == 0) {
          subs[i]->erase_one(j - 1);
        }
        if (j % 15 == 0) {
          [[maybe_unused]] auto p = subs[i]->pop_less_eq(j - 1);
        }
        subs[i]->emplace(j, j - j);  // no fragments
      }
    }
  }
  void random_ops() override {
    auto &subs = Find::subs;
    // re-add it in random order
    for (int j = 0; j < 50000; j++) {
      for (int i = 0; i < subs.size(); i++) {
        subs[i]->emplace(j, j - i);
      }
    }
    for (int i = 0; i < subs.size(); i++) {
      for (int j = 0; j < 1000; j++) {
        subs[i]->emplace(100, -j);  // no fragments
        subs[i]->emplace(500, -j);  // no fragments}
        subs[i]->emplace(200, -j);  // no fragments}
        subs[i]->emplace(300, -j);  // no fragments}
      }
    }
    for (int i = 0; i < subs.size(); i++) {
      for (int tof = 0; tof < 50000; tof += 10) {
        // we want a average count
        //        for (int j = 0; j < 100; j++) {
        //          // now do some search
        //          if (subs[i]->contains(tof)) {
        //            [[maybe_unused]] auto r = subs[i]->pop_less_eq(tof);
        //          }
        //        }
        subs[i]->erase_one(tof);
      }
    }
  }
  void access_ops() override {
    auto &subs = Find::subs;
    int count = 0;
    for (int j = 0; j < 5000; j++) {
      for (int i = 0; i < subs.size(); i++) {
        subs[i]->emplace(j, j - i);  // no fragments
        count++;
      }
    }
    for (int i = 0; i < subs.size(); i++) {
      for (int j = 0; j < 1000; j++) {
        subs[i]->emplace(100, -j);  // no fragments
        subs[i]->emplace(500, -j);  // no fragments}
        subs[i]->emplace(200, -j);  // no fragments}
        subs[i]->emplace(300, -j);  // no fragments}
        count += 4;
      }
    }
    cout << "emplace " << count << " elements";
    count = 0;
    int oops = 0;
    for (int j = 0; j < 100; j++) {
      for (int i = 0; i < subs.size(); i++) {
        for (auto it = subs[i]->begin(); it != subs[i]->end(); it++) {
          count++;
          if (it->first == INT_MAX) {  // prevent oops
            oops++;
          }
        }
      }
    }
    cout << "accessed " << count / 100 << " elements, opps: " << oops << endl;
  }
  void pop_ops() override {
    auto &subs = Find::subs;
    for (int j = 0; j < 5000; j++) {
      for (int i = 0; i < subs.size(); i++) {
        subs[i]->emplace(j, j - i);  // no fragments
      }
    }
    for (int i = 0; i < subs.size(); i++) {
      for (int j = 0; j < 1000; j++) {
        subs[i]->emplace(100, -j);  // no fragments
        subs[i]->emplace(500, -j);  // no fragments}
        subs[i]->emplace(200, -j);  // no fragments}
        subs[i]->emplace(300, -j);  // no fragments}
      }
    }
    for (int j = 1000; j < 5000; j += 50) {
      for (int i = 0; i < subs.size(); i++) {
        subs[i]->do_less_eq_then_pop(j, [](int &a) -> void { a = -1; });
        for (int k = 0; k < 100; k++) {
          subs[i]->emplace(50000 - k, k + 1);
        }
      }
    }
  }
};
struct SkipSystem : SkipTestsBase<coring::experimental::skiplist_map<int, int>> {
  SkipSystem(int scale = SYSTEM_SIZE) {
    std::cout << "non-pmr.skip" << std::endl;
    Find::subs.resize(scale);
    for (int i = 0; i < scale; i++) {
      subs[i] = std::make_unique<coring::experimental::skiplist_map<int, int>>();
    }
  }
};

struct PmrModule {
  std::vector<std::unique_ptr<std::pmr::monotonic_buffer_resource>> spaces;
  std::vector<std::unique_ptr<std::pmr::unsynchronized_pool_resource>> ress;
};

struct PmrMapSystem : PmrModule, MapTestsBase<std::pmr::multimap<int, int>> {
  PmrMapSystem(int scale, int buffer_size = 6'400'00) {
    std::cout << "pmr.map" << std::endl;
    Find::subs.resize(scale);
    PmrModule::ress.resize(scale);
    PmrModule::spaces.resize(scale);
    for (int i = 0; i < scale; i++) {
      spaces[i] = std::make_unique<std::pmr::monotonic_buffer_resource>(buffer_size);
      ress[i] = std::make_unique<std::pmr::unsynchronized_pool_resource>(spaces[i].get());
      subs[i] = std::make_unique<std::pmr::multimap<int, int>>(ress[i].get());
    }
  }
};

struct PmrSkipSystem
    : PmrModule,
      SkipTestsBase<coring::experimental::skiplist_map<int, int, std::pmr::polymorphic_allocator<std::byte>>> {
  PmrSkipSystem(int scale, int buffer_size = 6'400'00) {
    std::cout << "pmr.skip" << std::endl;
    Find::subs.resize(scale);
    PmrModule::ress.resize(scale);
    PmrModule::spaces.resize(scale);
    for (int i = 0; i < scale; i++) {
      spaces[i] = std::make_unique<std::pmr::monotonic_buffer_resource>(buffer_size);
      ress[i] = std::make_unique<std::pmr::unsynchronized_pool_resource>(spaces[i].get());
      subs[i] =
          std::make_unique<coring::experimental::skiplist_map<int, int, std::pmr::polymorphic_allocator<std::byte>>>(
              ress[i].get());
    }
  }
};

char get(char **argv, int nd) { return argv[nd][0]; }
bool match(const char *c, char ch) { return c[0] == ch; }
constexpr int DS = 1;
constexpr int PMR = 2;
constexpr int OP = 3;
int main(int argc, char **argv) {
  if (argc < 4) {
    std::cout << "`./pmr map non-pmr sequential` to run map + non-pmr + sequential_ops" << std::endl
              << "`./pmr skip pmr random` to run skiplist_map + pmr + random_ops" << std::endl;
    exit(0);
  }
  ITests *global_itest = nullptr;
  if (match("map", get(argv, DS))) {
    if (match("pmr", get(argv, PMR))) {
      global_itest = new PmrMapSystem(128);
    } else {
      global_itest = new MapSystem(128);
    }
  } else {
    if (match("pmr", get(argv, PMR))) {
      global_itest = new PmrSkipSystem(128);
    } else {
      global_itest = new SkipSystem(128);
    }
  }
  StopWatch sw{};
  if (match("seq", get(argv, OP))) {
    std::cout << ".seq" << std::endl;
    global_itest->sequential_ops();
  } else if (match("rand", get(argv, OP))) {
    std::cout << ".rand" << std::endl;
    global_itest->random_ops();
  } else if (match("access", get(argv, OP))) {
    std::cout << ".access" << std::endl;
    global_itest->access_ops();
  } else if (match("pop", get(argv, OP))) {
    std::cout << ".pop" << std::endl;
    global_itest->pop_ops();
  }
  return 0;
}