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

struct Note
{
  int lineIndex;
  int sampleLocation;
	float midiPitch;
	int padLength;

	bool operator < (const Note& n) const
    {
        return (midiPitch < n.midiPitch);
    }
};

int main() {
  vector<Note> notes;
	vector<float> maxx;
	vector<float> minn;
	ofstream outFile;
	outFile.open("pads.csv");
	std::ifstream minMaxFile("./min.max.csv");
	CSVRow minMaxRow;
	while (minMaxFile >> minMaxRow) {
		minn.push_back(stof(minMaxRow[0]));
		maxx.push_back(stof(minMaxRow[1]));
		// std::cout << "pushed back" << std::endl;
	}
	std::cout << "min/max loaded" << std::endl;

	std::ifstream metaFile("./mega.meta.csv");
	CSVRow row;

	float pitch = 0;
	int padLength = 0;
	bool withinPitch, isPeaky, noOnset, noteSearch = false;
	int rowIndex = 0;
	while (metaFile >> row) {
		// if flag set
		//   if frame has pitch within range and is peaky..
		//     increment count and move on
		//   else
		//     if count is higher than 10
		//       write the interval including
		//       frameindex, ftom(pitch), count
		//       (in future, average of other values)
		//
		//     else
		//       discard the interval (don't write)
		//       remove flag
		//
		// else
		//   if frame is peaky
		//     set flag and remember pitch
		withinPitch = (stof(row[5]) > pitch - 2.0f) &&
			      (stof(row[5]) < pitch + 2.0f);
		isPeaky = stof(row[3]) > 700.0f;
		noOnset = stof(row[4]) < 1000.0f;

		if (noteSearch) {
			if (withinPitch && isPeaky && noOnset) {
				padLength++;
			} else {
				if (padLength > 40) {
								notes.emplace_back();
								notes.back().lineIndex = rowIndex;
								notes.back().sampleLocation = stoi(row[0]);
								notes.back().midiPitch = diy::ftom(stof(row[5]));
								notes.back().padLength = padLength;
					//notes.push_back(
						//{stoi(row[0]), diy::ftom(stof(row[5])), padLength}
					//);
				}
				noteSearch = false;
				padLength = 0;
			}
		} else {
			if (isPeaky) {
				noteSearch = true;
				padLength++;
				pitch = stof(row[5]);
			}
		}

		//outFile << outStream.str();
		rowIndex++;
	}
	
	std::sort(notes.begin(), notes.end());

	for(int i = 0; i < notes.size(); i++) {
		std::ostringstream outStream;
		outStream << notes[i].lineIndex
						  << ","
						  << notes[i].sampleLocation
						  << ","
						  << notes[i].midiPitch
						  << ","
							<< notes[i].padLength
							<< endl;
		cout << outStream.str();
		outFile << outStream.str();
	}

	outFile.close();
	std::cout << "the end" << std::endl;
}

