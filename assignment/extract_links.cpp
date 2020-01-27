#include <regex>
#include <iostream>
using namespace std;
int main() {

  string word;

  while (true) {
    string line;
    getline(cin, line);

    std::smatch match;
    std::regex expr ("<\\s*a\\s+href=\"(.*?)\".*?>(.*?)<\\s*/\\s*a\\s*>");

    while (std::regex_search (line, match, expr)) {
      cout << match[2] << " --> " << match[1] << endl;
      line = match.suffix().str();
    }

    if(cin.eof()) { break; }

  }
}

