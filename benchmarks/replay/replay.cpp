/*
 * Copyright (c) 2020, NVIDIA CORPORATION.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either ex  ess or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cxxopts.hpp"
#include "rapidcsv.h"

#include <benchmark/benchmark.h>
#include <string>

static void BM_Replay(benchmark::State& state) {
}
BENCHMARK(BM_Replay);


enum class action : bool { ALLOCATE, FREE };

/**
 * @brief Stores the contents of a parsed log
 *
 * Holds 3 vectors of length `n`, where `n` is the number of actions in the log
 * - actions: Indicates if action `i` is an allocation or a deallocation
 * - sizes: Indicates the size of the action `i`
 * - pointers: For an allocation, the pointer returned, for a free, the pointer
 *   freed
 */
struct parsed_log {
  parsed_log(std::vector<action>&& a, std::vector<std::size_t>&& s,
             std::vector<uintptr_t>&& p)
      : actions{std::move(a)}, sizes{std::move(s)}, pointers{std::move(p)} {}
  std::vector<action> actions{};
  std::vector<std::size_t> sizes{};
  std::vector<uintptr_t> pointers{};
};

/**
 * @brief Parses the RMM log file specifed by `filename` for consumption by the
 * replay benchmark.
 *
 * @param filename Name of the RMM log file
 * @return parsed_log The logfile parsed into a set of actions that can be
 * consumed by the replay benchmark.
 */
parsed_log parse_csv(std::string const& filename) {
  rapidcsv::Document csv(filename);

  std::vector<std::size_t> sizes = csv.GetColumn<std::size_t>("Size");

  // Convert action strings to enum to reduce overhead of processing actions in
  // benchmark
  std::vector<std::string> actions_as_string =
      csv.GetColumn<std::string>("Action");
  std::vector<action> actions(actions_as_string.size());
  std::transform(actions_as_string.begin(), actions_as_string.end(),
                 actions.begin(), [](std::string const& s) {
                   return (s == "allocate") ? action::ALLOCATE : action::FREE;
                 });

  // Convert address string to uintptr_t
  // E.g., 0x7fb3c446f000 -> 140410068856832
  std::vector<std::string> pointers_as_string =
      csv.GetColumn<std::string>("Pointer");
  std::vector<uintptr_t> pointers(pointers_as_string.size());
  std::transform(
      pointers_as_string.begin(), pointers_as_string.end(), pointers.begin(),
      [](std::string const& s) { return std::stoll(s, nullptr, 16); });

  return parsed_log{std::move(actions), std::move(sizes), std::move(pointers)};
}

int main(int argc, char** argv) {
  // benchmark::Initialize will remove GBench command line arguments it
  // recognizes and leave any remaining arguments
  ::benchmark::Initialize(&argc, argv);

  // Parse for replay arguments:
  cxxopts::Options options(
      "RMM Replay Benchmark",
      "Replays and benchmarks allocation activity captured from RMM logging.");

  options.add_options()("f,file", "Name of RMM log file.",
                        cxxopts::value<std::string>());

  auto result = options.parse(argc, argv);

  // Parse the log file
  if (result.count("file")) {
    auto filename = result["file"].as<std::string>();
    auto parsed_log = parse_csv(filename);
  } else {
    throw std::runtime_error{"No log filename specified."};
  }

  ::benchmark::RunSpecifiedBenchmarks();
}