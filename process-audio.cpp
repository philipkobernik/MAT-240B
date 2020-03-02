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
#include <sstream>
#include <string>
#include <vector>

using std::cout;
using std::endl;
using namespace std;

#include "AudioFile.h"

AudioFile<float> audioFile;

struct Features {
				float rms, centroid, pitch;
};

struct Frame {
				float f1, f2, f3;
};

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
Gist<float> gist(frameSize, sampleRate);

float f1() { return gist.rootMeanSquare(); }
float f2() { return gist.spectralCentroid(); }
float f3() { return gist.pitch(); }

int main() {

	// audioFile.printSummary();
  vector<Frame> frames;
	ofstream megaOutFile, megaOutFileNoIndex, fileEntriesOutFile, minMaxOutFile;
	megaOutFile.open("mega.meta.csv");
	megaOutFileNoIndex.open("mega.meta.no.index.csv");
	fileEntriesOutFile.open("fileEntries.csv");
	minMaxOutFile.open("min.max.csv");
	int frameCount = 0;
	int startFrame;
	Features maxx = { -1e30f, -1e30f, -1e30f };
	Features minn = { 1e30f, 1e30f, 1e30f };

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

			float rms = f1();
			float centroid = f2();
			float pitch = f3();

			if(rms > maxx.rms) maxx.rms = rms;
			if(centroid > maxx.centroid) maxx.centroid = centroid;
			if(pitch > maxx.pitch) maxx.pitch = pitch;

			if(rms < minn.rms) minn.rms = rms;
			if(centroid < minn.centroid) minn.centroid = centroid;
			if(pitch < minn.pitch) minn.pitch = pitch;

			std::ostringstream out;
			out << rms
				    << ','
				    << centroid
				    << ','
				    << pitch << endl;

			megaOutFile << n << ',' << out.str();
			frames.push_back({rms, centroid, pitch});
			//megaOutFileNoIndex  << out.str();
			frameCount++;
							//<< gist.rootMeanSquare()
						//<< ','
						////<< gist.peakEnergy() << ','
						////<< gist.zeroCrossingRate() << ','
						//<< gist.spectralCentroid()
						//<< ','
						////<< gist.spectralCrest() << ','
						////<< gist.spectralFlatness() << ','
						////<< gist.spectralRolloff() << ','
						////<< gist.spectralKurtosis() << ','
						////<< gist.energyDifference() << ','
						////<< gist.spectralDifference() << ','
						////<< gist.highFrequencyContent() << ','
						//<< gist.pitch() << endl;


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
	minMaxOutFile << minn.rms << ',' << maxx.rms << std::endl;
	minMaxOutFile << minn.centroid << ',' << maxx.centroid << std::endl;
	minMaxOutFile << minn.pitch << ',' << maxx.pitch << std::endl;

	// make mega meta file
	megaOutFile.close();
	fileEntriesOutFile.close();
	minMaxOutFile.close();

	for(int i = 0; i<frames.size(); i++) {
					Frame f = frames[i];
					megaOutFileNoIndex <<
									(f.f1-minn.rms) / (maxx.rms-minn.rms)
									<< ","
									<< (f.f2-minn.centroid) / (maxx.centroid-minn.centroid)
									<< ","
									<< (f.f3-20.0f) / (maxx.pitch-20.0)
									<< std::endl;

	}


	megaOutFileNoIndex.close();
	return 0;
}
