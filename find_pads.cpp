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

struct Note {
	int lineIndex;
	int sampleLocation;
	float midiPitch;
	int lengthInFrames;

	bool operator<(const Note& n) const {
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
		float minValue = stof(minMaxRow[0]);
		float maxValue = stof(minMaxRow[1]);
		minn.push_back(minValue);
		maxx.push_back(maxValue);
	}
	std::cout << "min/max loaded" << std::endl;

	std::ifstream metaFile("./mega.meta.csv");
	CSVRow row;

	float notePitch = 0;
	float notePower = 0;
	int lengthInFrames = 0;
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

		float framePower = stof(row[1]);
		float frameTone = stof(row[2]);
		float framePitch = stof(row[5]);
		float framePeakiness = stof(row[3]);
		float frameOnset = stof(row[4]);
		withinPitch = (framePitch > notePitch - 2.0f) &&
			      (framePitch < notePitch + 2.0f);
		isPeaky = framePeakiness > 750.0f;
		noOnset = (framePower > notePower - 0.075f) &&
				    (framePower < notePower + 0.075f);

		if (noteSearch) {
			if (withinPitch && isPeaky && noOnset) {
				lengthInFrames++;
			} else {
				if (lengthInFrames > 25) {
					notes.emplace_back();
					notes.back().lineIndex =
					    rowIndex - lengthInFrames;
					notes.back().sampleLocation =
					    stoi(row[0]);
					notes.back().midiPitch =
					    diy::ftom(framePitch);
					notes.back().lengthInFrames =
					    lengthInFrames;
					// notes.push_back(
					//{stoi(row[0]),
					//diy::ftom(stof(row[5])),
					//lengthInFrames}
					//);
				}
				noteSearch = false;
				lengthInFrames = 0;
			}
		} else {
			if (isPeaky) {
				noteSearch = true;
				lengthInFrames++;
				notePitch = framePitch;
				notePower = framePower;
			}
		}

		// outFile << outStream.str();
		rowIndex++;
	}

	std::sort(notes.begin(), notes.end());

	for (int i = 0; i < notes.size(); i++) {
		std::ostringstream outStream;
		outStream << notes[i].lineIndex << ","
			  << notes[i].sampleLocation << ","
			  << notes[i].midiPitch << ","
			  << notes[i].lengthInFrames << endl;
		cout << outStream.str();
		outFile << outStream.str();
	}

	outFile.close();
	std::cout << "found :" << notes.size() << std::endl;
	std::cout << "the end" << std::endl;
}

