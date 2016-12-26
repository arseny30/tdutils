#pragma once
#include "td/utils/common.h"

#include <map>
#include <unordered_map>

namespace td {

// TODO template KeyT
class Hints {
  typedef int64 KeyT;
  typedef int64 RatingT;

 public:
  void add(KeyT key, const string &name);

  void remove(KeyT key) {
    add(key, "");
  }

  void set_rating(KeyT key, RatingT rating);

  vector<KeyT> search(const string &query, int32 limit) const;  // TODO sort by name instead of sort by rating

  string key_to_string(KeyT) const;

  vector<KeyT> search_empty(int32 limit) const;

 private:
  std::map<string, vector<KeyT>> word_to_keys_;
  std::unordered_map<KeyT, string> key_to_name_;
  std::unordered_map<KeyT, RatingT> key_to_rating_;

  static vector<string> get_words(const string &name);

  vector<KeyT> search_word(const string &name) const;

  class CompareByRating {
    const std::unordered_map<KeyT, RatingT> &key_to_rating_;

    RatingT get_rating(const KeyT &key) const {
      auto it = key_to_rating_.find(key);
      if (it == key_to_rating_.end()) {
        return RatingT();
      }
      return it->second;
    }

   public:
    explicit CompareByRating(const std::unordered_map<KeyT, RatingT> &key_to_rating) : key_to_rating_(key_to_rating) {
    }

    bool operator()(const KeyT &lhs, const KeyT &rhs) const {
      auto lhs_rating = get_rating(lhs);
      auto rhs_rating = get_rating(rhs);
      return lhs_rating < rhs_rating || (lhs_rating == rhs_rating && lhs < rhs);
    }
  };
};

}  // end of namespace td
