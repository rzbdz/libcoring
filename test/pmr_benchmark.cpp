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
    for (int j = 0; j < 5000; j++) {
      for (int i = 0; i < subs.size(); i++) {
        subs[i]->emplace(j, j - i);  // no fragments
      }
    }
    for (int j = 0; j < 100; j++) {
      for (int i = 0; i < subs.size(); i++) {
        for (auto it = subs[i]->begin(); it != subs[i]->end(); it++) {
          if (it->first == INT_MAX) {
            cout << "oops";
          }
        }
      }
    }
  }
};

struct MapSystem : MapTestsBase<std::map<int, int>> {
  MapSystem(int scale) {
    std::cout << "non-pmr.map" << std::endl;
    Find::subs.resize(scale);
    for (int i = 0; i < scale; i++) {
      Find::subs[i] = std::make_unique<std::map<int, int>>();
    }
  }
};

template <typename SkipType>
struct SkipTestsBase : SystemBase<SkipType> {
  using Find = SystemBase<SkipType>;
  void sequential_ops() override {
    auto &subs = Find::subs;
    for (int i = 0; i < subs.size(); i++) {
      for (int j = 0; j < 100000; j++) {
        if (j % 4 == 0) {
          subs[i]->erase_one(j - 1);
        }
        if (j % 15 == 0) {
          [[maybe_unused]] auto p = subs[i]->pop_less_eq(j - 1);
        }
        subs[i]->add(j, j - j);  // no fragments
      }
    }
  }
  void random_ops() override {
    auto &subs = Find::subs;
    // re-add it in random order
    for (int j = 0; j < 50000; j++) {
      for (int i = 0; i < subs.size(); i++) {
        subs[i]->add(j, j - i);
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
    for (int j = 0; j < 5000; j++) {
      for (int i = 0; i < subs.size(); i++) {
        subs[i]->add(j, j - i);  // no fragments
      }
    }
    for (int j = 0; j < 100; j++) {
      for (int i = 0; i < subs.size(); i++) {
        for (auto it = subs[i]->begin(); it != subs[i]->end(); it++) {
          if (it->first == INT_MAX - 5) {  // prevent oops
            cout << "oops";
          }
        }
      }
    }
  }
};
struct SkipSystem : SkipTestsBase<coring::skiplist_map<int, int, INT_MIN, INT_MAX>> {
  SkipSystem(int scale = SYSTEM_SIZE) {
    std::cout << "non-pmr.skip" << std::endl;
    Find::subs.resize(scale);
    for (int i = 0; i < scale; i++) {
      subs[i] = std::make_unique<coring::skiplist_map<int, int, INT_MIN, INT_MAX>>();
    }
  }
};

struct PmrModule {
  std::vector<std::unique_ptr<std::pmr::monotonic_buffer_resource>> spaces;
  std::vector<std::unique_ptr<std::pmr::unsynchronized_pool_resource>> ress;
};

struct PmrMapSystem : PmrModule, MapTestsBase<std::pmr::map<int, int>> {
  PmrMapSystem(int scale, int buffer_size = 6'400'000) {
    std::cout << "pmr.map" << std::endl;
    Find::subs.resize(scale);
    PmrModule::ress.resize(scale);
    PmrModule::spaces.resize(scale);
    for (int i = 0; i < scale; i++) {
      spaces[i] = std::make_unique<std::pmr::monotonic_buffer_resource>(buffer_size);
      ress[i] = std::make_unique<std::pmr::unsynchronized_pool_resource>(spaces[i].get());
      subs[i] = std::make_unique<std::pmr::map<int, int>>(ress[i].get());
    }
  }
};

struct PmrSkipSystem : PmrModule, SkipTestsBase<coring::experiment::skiplist_map<int, int, INT_MIN, INT_MAX>> {
  PmrSkipSystem(int scale, int buffer_size = 6'400'000) {
    std::cout << "pmr.skip" << std::endl;
    Find::subs.resize(scale);
    PmrModule::ress.resize(scale);
    PmrModule::spaces.resize(scale);
    for (int i = 0; i < scale; i++) {
      spaces[i] = std::make_unique<std::pmr::monotonic_buffer_resource>(buffer_size);
      ress[i] = std::make_unique<std::pmr::unsynchronized_pool_resource>(spaces[i].get());
      subs[i] = std::make_unique<coring::experiment::skiplist_map<int, int, INT_MIN, INT_MAX>>(ress[i].get());
    }
  }
};

char get(char **argv, int nd) { return argv[nd][0]; }
bool match(const char *c, char ch) { return c[0] == ch; }
constexpr int DS = 1;
constexpr int PMR = 2;
constexpr int OP = 3;
int main(int argc, char **argv) {
  std::cout << coring::experiment::skiplist_map<int, int, INT_MIN, INT_MAX>::node_size << std::endl;
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
  }
  return 0;
}