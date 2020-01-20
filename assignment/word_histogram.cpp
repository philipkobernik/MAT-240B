#include <algorithm>      // sort
#include <iostream>       // cin, cout, printf
#include <unordered_map>  // unordered_map
#include <vector>         // vector
#include <regex>          // regex
using namespace std;

bool pairComparator (std::pair<std::string, unsigned> a,std::pair<std::string, unsigned> b) {
  return (a.second > b.second);
}

int main() {
  unordered_map<string, unsigned> dictionary;

  string word;
  while (cin >> word) {
    // first, lets downcase the string
    std::transform(word.begin(), word.end(), word.begin(), ::tolower);

    // next, lets trim off any non-letter chars
    regex not_word_chars("[^\\w]");
    word = std::regex_replace(word, not_word_chars, "");

    // create-or-increment that pair, use the word as the hash-key
    std::unordered_map<std::string,unsigned>::const_iterator found = dictionary.find(word);
    if(found == dictionary.end()) {
      //cout << "not found, time to create" << "\n";
      if(word != "") { // string could be empty after pre-processing
        dictionary.insert(
          std::make_pair<std::string, unsigned>(word, 1)
        );
      }

    } else {
      // found it, increment it
      // why doesn't below line work:
      // found->second += 1; // cannot assign to return value because function "operator->" returns a const value

      dictionary[found->first] = found->second + 1;

    }
  }

  vector<pair<string, unsigned> > wordList;
  for (auto& t : dictionary) wordList.push_back(t);

  // put your sorting code here
  //// wordList.sort(pairComparator); // how I would imagine using sort
  std::sort(wordList.begin(), wordList.end(), pairComparator); // how c++ sort actually be

  for (auto& t : wordList) printf("%u:%s\n", t.second, t.first.c_str());
}
