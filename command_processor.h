#pragma once

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <numeric>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

static const std::string BULK = "bulk: ";
static const std::string START_BLOCK = "{";
static const std::string END_BLOCK = "}";

struct Command {
  std::string text;
  std::chrono::system_clock::time_point timeStamp;
};

/**
 * @brief базовый класс для вывода
 */
class Output {
public:
  virtual void update(const std::vector<Command>& command) = 0;
  virtual void ProcessCommand() = 0;
  virtual ~Output() = default;

protected:
  std::string Join(const std::vector<Command>& v) const {
    return std::accumulate(v.begin(), v.end(), std::string(),
                           [](std::string &s, const Command &com) {
      return s.empty() ? s.append(com.text)
                       : s.append(", ").append(com.text);
    });
  }
};

/**
 * @brief класс обработчика команд
 */
class BatchCommandProcessor { // publisher
public:
  BatchCommandProcessor(size_t bulkSize)
    : m_bulkSize(bulkSize) {}

  ~BatchCommandProcessor() {
    if (!m_blockForced) {
      DumpBatch();
    }
    for (auto subscruber : m_subscribers) {
      if (auto s = subscruber.lock()) {
        unSubscribe(s);
      }
    }
  }

  void StartBlock() {
    m_blockForced = true;
    DumpBatch();
  }

  void FinishBlock() {
    m_blockForced = false;
    DumpBatch();
  }

  void ProcessCommand(const Command& command) {
    m_commands.push_back(command);
    notify();

    if (!m_blockForced &&
        (m_commands.size() >= static_cast<size_t>(m_bulkSize))) {
      DumpBatch();
    }
  }

  void subscribe(std::shared_ptr<Output> o) noexcept {
    if (o) {
      m_subscribers.push_back(o);
    }
  }

  void unSubscribe(std::shared_ptr<Output> o) noexcept {
    if (o) {
      m_subscribers.erase(
            std::remove_if(
              m_subscribers.begin(), m_subscribers.end(),
              [&o](std::weak_ptr<Output> subscriber) {
        return subscriber.lock() == o;
      }), m_subscribers.end());
    }
  }

  void notify() noexcept {
    for (auto subscriber : m_subscribers) {
      if (auto s = subscriber.lock()) {
        s->update(m_commands);
      }
    }
  }

private:
  void ClearBatch() {
    m_commands.clear();
    notify();
  }

  void DumpBatch() {
    if (!m_commands.empty()) {
      std::vector<std::thread> threads;
      for (auto subscriber : m_subscribers) {        
        if (auto s = subscriber.lock()) {
          threads.push_back(std::thread([s] {
            s->ProcessCommand();
          }));
        }
      }
      for (auto &thread : threads) {
        if (thread.joinable()) {
          thread.join();
        }
      }
    }
    ClearBatch();
  }

  size_t m_bulkSize;
  bool m_blockForced = false;
  std::vector<Command> m_commands;
  std::vector<std::weak_ptr<Output>> m_subscribers;
};

/**
 * @brief класс вывода команд в консоль
 */
class ConsoleOutput : public Output { // subscriber
public:
  ConsoleOutput() = default;

  void update(const std::vector<Command>& commands) override {
    m_commands = commands;
  }

  void ProcessCommand() override {
    auto output = BULK + Join(m_commands);
    std::cout << output << std::endl;
  }

private:
  std::vector<Command> m_commands;
};

/**
 * @brief класс записи команд в файл
 */
class ReportWriter : public Output { // subscriber
public:
  ReportWriter() = default;

  void update(const std::vector<Command>& commands) override {
    m_commands = commands;
  }

  void ProcessCommand() override {
    auto output = BULK + Join(m_commands);
    std::ofstream file(GetFilename(), std::ofstream::out);
    file << output;
    // wait
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(1ms);
  }

private:
  std::vector<Command> m_commands;

  std::string GetFilename() {
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
          m_commands[0].timeStamp.time_since_epoch()).count();
    auto id = std::this_thread::get_id();
    std::stringstream filename;
    filename << "bulk" << seconds << "_" << id << ".log";

    return filename.str();
  }
};

/**
 * @brief класс работы с командами из консоли
 */
class BatchConsoleInput {
public:
  BatchConsoleInput(size_t bulkSize) {
    m_commandProcessor = std::make_unique<BatchCommandProcessor>(bulkSize);
    m_output.push_back(std::make_shared<ReportWriter>());
    m_output.push_back(std::make_shared<ConsoleOutput>());
    for (auto o : m_output) {
      m_commandProcessor->subscribe(o);
    }
  }

  void ProcessCommand(const Command& command) {
    if (m_commandProcessor) {
      if (command.text == START_BLOCK) {
        if (m_blockDepth++ == 0)
          m_commandProcessor->StartBlock();
      }
      else if (command.text == END_BLOCK) {
        if (--m_blockDepth == 0)
          m_commandProcessor->FinishBlock();
      }
      else
        m_commandProcessor->ProcessCommand(command);
    }
  }
private:
  int m_blockDepth = 0;
  std::unique_ptr<BatchCommandProcessor> m_commandProcessor;
  std::vector<std::shared_ptr<Output>> m_output;
};
