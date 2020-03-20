#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <string>
#include <vector>

#include "functions.h"
#include "parse-csv.h"

using namespace std;

int frameFeaturesSize = 5;

int main() {
  vector<float> pitches;
  // vector<float> minn;
  ofstream outFile;
  outFile.open("mega.meta.filter.pitch.csv");
  std::cout << "min/max loaded" << std::endl;

  std::ifstream metaFile("./mega.meta.csv");
  CSVRow row;
  while (metaFile >> row) {
    pitches.push_back(std::stof(row[5]));

    float median = 0;
    if (pitches.size() > 4) {
      // get the last 5
      vector<float>::const_iterator start = pitches.end() - 5;
      vector<float>::const_iterator end = pitches.end() - 0;

      vector<float> lastN(start, end);
      // cout << "size: " << lastN.size() << endl;

      // sort them
      std::sort(lastN.begin(), lastN.end());

      // get the median
      // cout << lastN[0]
      //<< " , "
      //<< lastN[1]
      //<< " , "
      //<< lastN[2]
      //<< " , "
      //<< lastN[3]
      //<< " , "
      //<< lastN[4]
      //<< endl;

      // cout << "median: " << lastN[2] << endl;
      median = lastN[2];
    }
    std::ostringstream outStream;

    outStream << row[0] << "," << row[1] << "," << row[2] << "," << row[3]
              << "," << row[4] << "," << row[5] << "," << median << endl;

    // cout << outStream.str();
    outFile << outStream.str();
  }
  outFile.close();
  std::cout << "the end" << std::endl;
}

