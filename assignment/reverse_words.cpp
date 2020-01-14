#include <iostream>
#include <string>
#include <vector>
#include <sstream>

using namespace std;
int main() {
  while (true) {
    printf("Type a sentence (then hit return): ");

    string line;
    string word;
    // vector of strings
    vector <string> tokens;
    getline(cin, line);
    stringstream stream_line(line);

    while ( getline(stream_line, word, ' ')) {
      tokens.push_back(word);
    }

    if (!cin.good()) {
      printf("Done\n");
      return 0;
    }

    for(int i = 0; i < tokens.size(); i++) {
      // reverse iterator
      for (std::string::reverse_iterator rit=tokens[i].rbegin(); rit!=tokens[i].rend(); ++rit) {
        cout << *rit;
      }
      // my for loop
      //for(int j = tokens[i].size()-1; j > -1; j--) {
        //cout << tokens[i][j];
      //}
      cout << ' ';
    }
    cout << "\n\ngood jorb!\n\n";

  }
}
