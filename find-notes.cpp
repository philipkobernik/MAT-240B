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
	float rms;
	float tone;
	float onset;
	float peakiness;

	bool operator<(const Note& n) const {
		return (midiPitch < n.midiPitch);
	}
};

struct NoteFile {
	int midiPitch;
	ofstream out;
	ofstream outNormNoIndex;
};

int main() {
	vector<Note> notes;
	vector<NoteFile> files;
	vector<float> maxx;
	vector<float> minn;
	ofstream outFile, outFileNormNoIndex;
	outFile.open("notes.csv");
	outFileNormNoIndex.open("notes.norm.no.index.csv");

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
	float noteOnset = 0;

	float noteToneSum = 0;
	float notePeakinessSum = 0;

	int lengthInFrames = 0;
	bool withinPitch, isPeaky, noOnset, noteSearch = false;
	int rowIndex = 0;
	while (metaFile >> row) {
		int frameSampleLocation = stof(row[0]);
		float framePower = stof(row[1]);
		float frameTone = stof(row[2]);
		float framePitch = stof(row[5]);
		float framePeakiness = stof(row[3]);
		float frameOnset = stof(row[4]);

		withinPitch = (framePitch > notePitch - 2.0f) &&
			      (framePitch < notePitch + 2.0f);
		isPeaky = framePeakiness > 400.0f;  // 750 best results so far
		noOnset = (framePower > notePower - 0.075f) &&
			  (framePower < notePower + 0.075f);

		if (noteSearch) {
			if (withinPitch && isPeaky && noOnset) {
				// increment counter
				lengthInFrames++;

				// increment sums to calc avg
				noteToneSum += frameTone;
				notePeakinessSum += framePeakiness;

			} else {
				if (lengthInFrames > 20) {
					notes.emplace_back();
					notes.back().lineIndex =
					    rowIndex - lengthInFrames;
					notes.back().sampleLocation =
					    frameSampleLocation;
					notes.back().midiPitch =
					    diy::ftom(framePitch);
					notes.back().lengthInFrames =
					    lengthInFrames;
					notes.back().rms =
					    notePower;	// varies little
					notes.back().tone =
					    noteToneSum /
					    lengthInFrames;  // avg tone
					notes.back().onset =
					    noteOnset;	// attack onset
					notes.back().peakiness =
					    notePeakinessSum / lengthInFrames;
				}
				noteSearch = false;
				lengthInFrames = 0;
				noteToneSum = 0;
				notePeakinessSum = 0;
			}
		} else {
			if (isPeaky) {
				noteSearch = true;
				lengthInFrames++;
				notePitch = framePitch;
				notePower = framePower;
				noteOnset = frameOnset;
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
			  << notes[i].lengthInFrames << "," << notes[i].rms
			  << "," << notes[i].tone << "," << notes[i].onset
			  << "," << notes[i].peakiness << endl;
		// cout << outStream.str();
		outFile << outStream.str();
	}

	// end pads.csv
	outFile.close();

	vector<int> noteCounts(40, 0);

	// write pads.norm.no.index.csv
	for (int i = 0; i < notes.size(); i++) {
		Note n = notes[i];
		outFileNormNoIndex
		    //<< (n.rms - minn[0]) / (maxx[0] - minn[0])
		    //<< ","
		    //<< (n.tone - minn[1]) / (maxx[1] - minn[1])
		    //<< ","
		    //<< (n.onset - minn[3]) / (maxx[3] - minn[3])
		    //<< ","
		    //<< (n.peakiness - minn[2]) / (maxx[2] - minn[2])
		    //<< ","
		    << (n.midiPitch - minn[4]) / (maxx[4] - minn[4]) << endl;

		int p = n.midiPitch;
		float ramp = n.midiPitch - p;

		for (int j = 24; j < noteCounts.size(); j++) {
			if ((n.midiPitch > float(j) - 0.33f) &&
			    (n.midiPitch < float(j) + 0.33f)) {
				noteCounts[j] = noteCounts[j] + 1;
				if (files.size() < j - 23) {
          char outName[30];
          char outNameNoIndex[30];
					sprintf(outName, "note-%d.csv", j);
					sprintf(outNameNoIndex, "note-%d.no.index.csv", j);
					files.emplace_back();
					files.back().out.open(outName);
					files.back().outNormNoIndex.open(outNameNoIndex);
					files.back().midiPitch = j;
					// files should be primed for writing
				}
				files.back().out << notes[i].lineIndex << ","
					     << notes[i].sampleLocation << ","
					     << notes[i].midiPitch << ","
					     << notes[i].lengthInFrames << ","
					     << notes[i].rms << ","
					     << notes[i].tone << ","
					     << notes[i].onset << ","
					     << notes[i].peakiness << endl;

				files.back().outNormNoIndex 
				<< (n.rms - minn[0]) / (maxx[0] - minn[0])
				<< ","
				<< (n.tone - minn[1]) / (maxx[1] - minn[1])
				<< ","
				<< (n.onset - minn[3]) / (maxx[3] - minn[3])
				<< ","
				<< (n.peakiness - minn[2]) / (maxx[2] - minn[2])
		    << endl;
			}
		}
	}
	for (int j = 24; j < noteCounts.size(); j++) {
		std::cout << "noteCounts: " << j << ": " << noteCounts[j]
			  << std::endl;
	}
	for (int j = 0; j < files.size(); j++) {
    files[j].out.close();
    files[j].outNormNoIndex.close();
	}

	outFileNormNoIndex.close();

	std::cout << "found :" << notes.size() << std::endl;
	std::cout << "the end" << std::endl;
}

