#include "Gist/Gist.h"
#include "Gist/Gist.cpp"
#include "Gist/fft/WindowFunctions.h"
#include "Gist/fft/WindowFunctions.cpp"
#include "Gist/core/CoreTimeDomainFeatures.h"
#include "Gist/core/CoreTimeDomainFeatures.cpp"
#include "Gist/core/CoreFrequencyDomainFeatures.h"
#include "Gist/core/CoreFrequencyDomainFeatures.cpp"
#include "Gist/onset-detection-functions/OnsetDetectionFunction.h"
#include "Gist/onset-detection-functions/OnsetDetectionFunction.cpp"
#include "Gist/pitch/Yin.h"
#include "Gist/pitch/Yin.cpp"
#include "Gist/mfcc/MFCC.h"
#include "Gist/mfcc/MFCC.cpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using std::cout;
using std::endl;
using namespace std;

#include "AudioFile.h"

AudioFile<float> audioFile;

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
const int frameSize = 1024;
const int hopSize = frameSize / 4;

int main() {
	Gist<float> gist(frameSize, sampleRate);

	// audioFile.printSummary();

	ofstream megaOutFile, megaOutFileNoIndex, fileEntriesOutFile;
	megaOutFile.open("mega.meta.csv");
	megaOutFileNoIndex.open("mega.meta.no.index.csv");
	fileEntriesOutFile.open("fileEntries.csv");
	int frameCount = 0;
	int startFrame;

	for (const auto& name : get_filenames("/Users/ptk/src/corpus_audio")) {
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

		// process audio file
		// use Gist to analyze and print a bunch of frames
		//
		// cout << "📝 n, RMS, peak, ZCR, centroid" << endl;
		// outFile << "sample, rms, s.centroid, pitch" << endl;
		for (int n = 0; n + frameSize < numSamples; n += hopSize) {
			gist.processAudioFrame(&audioFile.samples[channel][n],
					       frameSize);
			// outFile << n << ','
			megaOutFile << n << ',' << gist.rootMeanSquare()
				    << ','
				    //<< gist.peakEnergy() << ','
				    //<< gist.zeroCrossingRate() << ','
				    << gist.spectralCentroid()
				    << ','
				    //<< gist.spectralCrest() << ','
				    //<< gist.spectralFlatness() << ','
				    //<< gist.spectralRolloff() << ','
				    //<< gist.spectralKurtosis() << ','
				    //<< gist.energyDifference() << ','
				    //<< gist.spectralDifference() << ','
				    //<< gist.highFrequencyContent() << ','
				    << gist.pitch() << endl;

			megaOutFileNoIndex << gist.rootMeanSquare()
				    << ','
				    << gist.spectralCentroid()
				    << ','
				    << gist.pitch() << endl;

			frameCount++;

			float progress = (float)n/(float)numSamples;

				int barWidth = 70;

				std::cout << "[";
				int pos = barWidth * progress;
				for (int i = 0; i < barWidth; ++i) {
					if (i < pos)
						std::cout << "=";
					else if (i == pos)
						std::cout << "🕺";
					else
						std::cout << " ";
				}
				std::cout << "] " << int(progress * 100.0)+1 // use Math.ceil instead of this hax
					  << " %\r";
				std::cout.flush();
		}  // done with file
    std::cout << std::endl << std::endl;

		fileEntriesOutFile << (frameCount - 1 - startFrame) << endl;

		// outFile.close();
	}  // end of files loop

	// make mega meta file
	megaOutFile.close();
	megaOutFileNoIndex.close();
	fileEntriesOutFile.close();

	return 0;
}
