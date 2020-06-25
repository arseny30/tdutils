#include "td/utils/TsFileLog.h"

#include "td/utils/common.h"
#include "td/utils/FileLog.h"
#include "td/utils/logging.h"
#include "td/utils/port/thread_local.h"
#include "td/utils/Slice.h"

#include <array>
#include <atomic>
#include <limits>
#include <mutex>

namespace td {

namespace detail {
class TsFileLog : public LogInterface {
 public:
  Status init(string path, int64 rotate_threshold, bool redirect_stderr) {
    path_ = std::move(path);
    rotate_threshold_ = rotate_threshold;
    redirect_stderr_ = redirect_stderr;
    for (size_t i = 0; i < logs_.size(); i++) {
      logs_[i].id = i;
    }
    return init_info(&logs_[0]);
  }

  vector<string> get_file_paths() override {
    vector<string> res;
    for (auto &log : logs_) {
      res.push_back(get_path(&log));
    }
    return res;
  }

  void append(CSlice cslice) override {
    return append(cslice, -1);
  }
  void append(CSlice cslice, int log_level) override {
    get_current_logger()->append(cslice, log_level);
  }

 private:
  struct Info {
    FileLog log;
    std::atomic<bool> is_inited{false};
    size_t id;
  };

  static constexpr size_t MAX_THREAD_ID = 128;
  int64 rotate_threshold_;
  bool redirect_stderr_;
  std::string path_;
  std::array<Info, MAX_THREAD_ID> logs_;
  std::mutex init_mutex_;

  LogInterface *get_current_logger() {
    auto *info = get_current_info();
    if (!info->is_inited.load(std::memory_order_relaxed)) {
      std::unique_lock<std::mutex> lock(init_mutex_);
      if (!info->is_inited.load(std::memory_order_relaxed)) {
        init_info(info).ensure();
      }
    }
    return &info->log;
  }

  Info *get_current_info() {
    return &logs_[get_thread_id()];
  }

  Status init_info(Info *info) {
    TRY_STATUS(info->log.init(get_path(info), std::numeric_limits<int64>::max(), info->id == 0 && redirect_stderr_));
    info->is_inited = true;
    return Status::OK();
  }

  string get_path(const Info *info) const {
    if (info->id == 0) {
      return path_;
    }
    return PSTRING() << path_ << ".thread" << info->id << ".log";
  }

  void rotate() override {
    for (auto &info : logs_) {
      if (info.is_inited.load(std::memory_order_acquire)) {
        info.log.lazy_rotate();
      }
    }
  }
};
}  // namespace detail

Result<unique_ptr<LogInterface>> TsFileLog::create(string path, int64 rotate_threshold, bool redirect_stderr) {
  auto res = make_unique<detail::TsFileLog>();
  TRY_STATUS(res->init(path, rotate_threshold, redirect_stderr));
  return std::move(res);
}

}  // namespace td
