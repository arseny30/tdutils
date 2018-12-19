#include "td/utils/filesystem.h"

#include "td/utils/buffer.h"
#include "td/utils/logging.h"
#include "td/utils/PathView.h"
#include "td/utils/port/FileFd.h"
#include "td/utils/Slice.h"
#include "td/utils/Status.h"
#include "td/utils/unicode.h"
#include "td/utils/utf8.h"

namespace td {

Result<BufferSlice> read_file(CSlice path, int64 size, int64 offset) {
  TRY_RESULT(from_file, FileFd::open(path, FileFd::Read));
  if (size == -1) {
    size = from_file.get_size() - offset;
  }
  if (size < 0) {
    return Status::Error("Failed to read file: invalid size or offset");
  }
  BufferWriter content{static_cast<size_t>(size), 0, 0};
  TRY_RESULT(got_size, from_file.pread(content.as_slice(), offset));
  if (got_size != static_cast<size_t>(size)) {
    return Status::Error("Failed to read file");
  }
  from_file.close();
  return content.as_buffer_slice();
}
Result<std::string> read_file_str(CSlice path, int64 size, int64 offset) {
  TRY_RESULT(from_file, FileFd::open(path, FileFd::Read));
  if (size == -1) {
    size = from_file.get_size() - offset;
  }
  if (size < 0) {
    return Status::Error("Failed to read file: invalid size or offset");
  }
  std::string content(static_cast<size_t>(size), '\0');
  TRY_RESULT(got_size, from_file.pread(content, offset));
  if (got_size != static_cast<size_t>(size)) {
    return Status::Error("Failed to read file");
  }
  from_file.close();
  return std::move(content);
}

// Very straightforward function. Don't expect much of it.
Status copy_file(CSlice from, CSlice to, int64 size) {
  TRY_RESULT(content, read_file(from, size));
  return write_file(to, content.as_slice());
}

Status write_file(CSlice to, Slice data) {
  auto size = data.size();
  TRY_RESULT(to_file, FileFd::open(to, FileFd::Truncate | FileFd::Create | FileFd::Write));
  TRY_RESULT(written, to_file.write(data));
  if (written != static_cast<size_t>(size)) {
    return Status::Error(PSLICE() << "Failed to write file: written " << written << " bytes instead of " << size);
  }
  to_file.close();
  return Status::OK();
}

static std::string clean_filename_part(Slice name, int max_length) {
  auto is_ok = [](uint32 code) {
    if (code < 32) {
      return false;
    }
    if (code < 127) {
      switch (code) {
        case '<':
        case '>':
        case ':':
        case '"':
        case '/':
        case '\\':
        case '|':
        case '?':
        case '*':
        case '&':
        case '`':
        case '\'':
          return false;
        default:
          return true;
      }
    }
    auto category = get_unicode_simple_category(code);

    return category == UnicodeSimpleCategory::Letter || category == UnicodeSimpleCategory::DecimalNumber ||
           category == UnicodeSimpleCategory::Number;
  };

  std::string new_name;
  int size = 0;
  for (auto *it = name.ubegin(); it != name.uend() && size < max_length;) {
    uint32 code;
    it = next_utf8_unsafe(it, &code, "clean_filename_part");
    if (!is_ok(code)) {
      if (prepare_search_character(code) == 0) {
        continue;
      }
      code = ' ';
    }
    if (new_name.empty() && (code == ' ' || code == '.')) {
      continue;
    }
    append_utf8_character(new_name, code);
    size++;
  }

  while (!new_name.empty() && (new_name.back() == ' ' || new_name.back() == '.')) {
    new_name.pop_back();
  }
  return new_name;
}

std::string clean_filename(CSlice name) {
  if (!check_utf8(name)) {
    return {};
  }

  PathView path_view(name);
  auto filename = clean_filename_part(path_view.file_stem(), 60);
  auto extension = clean_filename_part(path_view.extension(), 20);
  if (!extension.empty()) {
    if (filename.empty()) {
      filename = std::move(extension);
    } else {
      filename.reserve(filename.size() + 1 + extension.size());
      filename += '.';
      filename += extension;
    }
  }

  return filename;
}

}  // namespace td
