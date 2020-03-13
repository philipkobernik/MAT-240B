#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <random>

#include "Gist/Gist.cpp"
#include "Gist/Gist.h"
#include "Gist/core/CoreFrequencyDomainFeatures.cpp"
#include "Gist/core/CoreFrequencyDomainFeatures.h"
#include "Gist/core/CoreTimeDomainFeatures.cpp"
#include "Gist/core/CoreTimeDomainFeatures.h"
#include "Gist/fft/WindowFunctions.cpp"
#include "Gist/fft/WindowFunctions.h"
#include "Gist/mfcc/MFCC.cpp"
#include "Gist/mfcc/MFCC.h"
#include "Gist/onset-detection-functions/OnsetDetectionFunction.cpp"
#include "Gist/onset-detection-functions/OnsetDetectionFunction.h"
#include "Gist/pitch/Yin.cpp"
#include "Gist/pitch/Yin.h"

using std::cout;
using std::endl;
using namespace std;

#include "AudioFile.h"

#include "functions.h"

std::mt19937 seeded_eng() {
  std::random_device r;
  std::seed_seq seed{r(), r(), r(), r(), r(), r(), r(), r()};
  return std::mt19937(seed);
}

class Random {
  std::mt19937 eng = seeded_eng();
public:
  auto operator()(int a, int b) {
    std::uniform_int_distribution<int> dist(a, b);
    return dist(eng);
  }
};

Random rng;

AudioFile<float> audioFile;

// struct Features {
// float rms, centroid, pitch;
//};

// struct Frame {
// float f1, f2, f3;
//};

std::vector<std::string> get_filenames(std::filesystem::path path) {
	std::vector<std::string> filenames;

	// http://en.cppreference.com/w/cpp/experimental/fs/directory_iterator
	const std::filesystem::directory_iterator end{};

	for (std::filesystem::directory_iterator iter{path}; iter != end;
	     ++iter) {
		// http://en.cppreference.com/w/cpp/experimental/fs/is_regular_file
		if (std::filesystem::is_regular_file(*iter)) {
			// comment out if all names (names of
			// directories tc.) are required
			string filename = iter->path().string();
			if (filename.substr(filename.length() - 3) == "wav") {
				filenames.push_back(filename);
			}
		}
	}

	return filenames;
}

float mtof(float m) { return 8.175799f * powf(2.0f, m / 12.0f); }

const int sampleRate = 44100;
const int frameSize = 4096;
const int hopSize = frameSize / 4;
Gist<float> gist(frameSize, sampleRate);

int main() {
	// audioFile.printSummary();
	vector<vector<float>> frames;
	ofstream megaOutFile, megaOutFileNoIndex, fileEntriesOutFile,
	    minMaxOutFile;
	megaOutFile.open("mega.meta.csv");
	megaOutFileNoIndex.open("mega.meta.no.index.csv");
	fileEntriesOutFile.open("fileEntries.csv");
	minMaxOutFile.open("min.max.csv");
	int frameCount = 0;
	int startFrame;
	vector<float> maxx = {-1e30f, -1e30f, -1e30f, -1e30f, -1e30f};
	vector<float> minn = {1e30f, 1e30f, 1e30f, 1e30f, 1e30f};
	int frameFeaturesSize = 5;
			//vector<float> chosenFeatures = {
					//gist.rootMeanSquare(),    // 1
					//gist.spectralCentroid(),  // 2
					//gist.spectralCrest(),     // 3
					//gist.spectralRolloff(),   // 4
					//gist.pitch()	      // 5
			//};

	for (const auto& name : get_filenames("/Users/ptk/src/corpus_755")) {
		std::cout << "👾 " << name << std::endl;
		// create meta file
		// ofstream outFile;
		// std::string metaName = name.substr(0);
		// metaName.append(".csv");
		// outFile.open(metaName);

		//
		// load audio file
		audioFile.load(name);
		int channel = 0;
		int numSamples = audioFile.getNumSamplesPerChannel();

		startFrame = frameCount;
		fileEntriesOutFile << name << "," << frameCount << ",";

		vector<string> emojiList = {"🌊","🤠","🚀","⭐️","🐌","🦥","🧚","💆","🥏","🛰"};
    string progressEmoji = emojiList[rng(0, emojiList.size() - 1)];

		// process audio file
		// use Gist to analyze and print a bunch of frames
		//
		// cout << "📝 n, RMS, peak, ZCR, centroid" << endl;
		// outFile << "sample, rms, s.centroid, pitch" << endl;
		for (int n = 0; n + frameSize < numSamples; n += hopSize) {
			gist.processAudioFrame(&audioFile.samples[channel][n],
					       frameSize);

			vector<float> frameFeatures = {
			    gist.rootMeanSquare(),    // 0
					gist.spectralCentroid(),  // 1
					gist.spectralCrest(),     // 2
					gist.complexSpectralDifference(), // 3
			    diy::ftom(gist.pitch())	      // 4
			};

			for (int f = 0; f < frameFeatures.size(); f++) {
				float feat = frameFeatures[f];
				if (feat > maxx[f]) maxx[f] = feat;
				if (feat < minn[f]) minn[f] = feat;
			}

			std::ostringstream out;

			for (int i = 0; i < frameFeatures.size(); i++) {
				out << frameFeatures[i];
				if (i != frameFeatures.size() - 1) {
					out << ',';
				} else {
					out << endl;
				}
			}

			megaOutFile << n << ',' << out.str();
			frames.push_back(frameFeatures);
			frameCount++;

			float progress = (float)n / (float)numSamples;

			int barWidth = 70;

			std::cout << "[";
			int pos = barWidth * progress;
			for (int i = 0; i < barWidth; ++i) {
				if (i < pos)
					std::cout << "=";
				else if (i == pos)
					std::cout << progressEmoji;
				else
					std::cout << " ";
			}
			std::cout << "] "
				  << int(progress * 100.0) +
					 1  // use Math.ceil instead of this hax
				  << " %\r";
			std::cout.flush();
		}  // done with file
		std::cout << std::endl << std::endl;

		fileEntriesOutFile << (frameCount - 1 - startFrame) << endl;

		// outFile.close();
	}  // end of files loop
	for(int i=0; i<frameFeaturesSize; i++) {
    minMaxOutFile << minn[i] << ',' << maxx[i] << std::endl;
	}

	// make mega meta file
	megaOutFile.close();
	fileEntriesOutFile.close();
	minMaxOutFile.close();

	for (int i = 0; i < frames.size(); i++) {
		vector<float> f = frames[i];
		for (int j = 0; j < frameFeaturesSize; j++) {
			megaOutFileNoIndex
				<< (f[j] - minn[j]) / (maxx[j] - minn[j]);
      if(j != frameFeaturesSize-1) {
				megaOutFileNoIndex << ",";
			} else {
				megaOutFileNoIndex << endl;
			}
		}
	}

	megaOutFileNoIndex.close();
	return 0;
}
