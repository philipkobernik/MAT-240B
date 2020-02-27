#include "Gamma/DFT.h"	// gam::STFT
#include "Gamma/Filter.h"
#include "Gamma/Oscillator.h"
#include "Gamma/SamplePlayer.h"
#include "al/app/al_App.hpp"
#include "al/ui/al_ControlGUI.hpp"  // gui.draw(g)
using namespace al;
using namespace std;

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <mlpack/methods/neighbor_search/neighbor_search.hpp>
#include <set>
#include <vector>

#include "functions.h"
#include "parse-csv.h"

gam::STFT stft(4096, 4096 / 4, 16384, gam::HAMMING);
gam::STFT re_stft(4096, 4096 / 4, 16384, gam::HAMMING);
gam::SamplePlayer<float, gam::ipl::Cubic, gam::phsInc::Loop> player;

typedef mlpack::neighbor::NeighborSearch<   //
    mlpack::neighbor::NearestNeighborSort,  //
    mlpack::metric::EuclideanDistance,	    //
    arma::mat,				    //
    mlpack::tree::BallTree>		    //
    MyKNN;

bool peaksComparator(int a, int b) {
	return log(stft.bin(a).mag()) > log(stft.bin(b).mag());
}

bool binGreaterThanNeighbors(int binIndex, int peakNeighbors, int numBins) {
	if (binIndex < peakNeighbors || binIndex >= numBins - peakNeighbors)
		return false;

	float m = log(stft.bin(binIndex).mag());
	for (int i = 1; i <= peakNeighbors; i++) {
		if (m <= log(stft.bin(binIndex - i).mag())) return false;
		if (m <= log(stft.bin(binIndex + i).mag())) return false;
	}
	return true;
}

struct CorpusFile {
	string filePath;
	int startFrame;
	int lengthInFrames;
};

struct MyApp : App {
	MyKNN myknn;
  arma::mat dataset;
  arma::mat datasetNoIndex;
  arma::mat normDatasetNoIndex;
	arma::mat distances;
	arma::Mat<size_t> neighbors;
	vector<CorpusFile> corpus;

	float hz;
	std::vector<int> peaksVector;
	int numBins;
	int numPeaks;
	gam::Sine<> osc;
	gam::OnePole<> frequencyFilter, rateFilter;
	Parameter rate{"Playback Speed", "", 0.5, "", 0.0, 2.0};
	ParameterInt numPeaksParam{"Top N Peaks", "", 8, "", 1, 50};
	ParameterInt peakNeighborsParam{
	    "Compare +/- Neighboring Bins", "", 20, "", 1, 50};
	ParameterBool freezeParam{"Freeze", "", 0.0};
	ParameterBool sourceFileParam{"Sound File Input", "", 1};
	Parameter zoomXParam{"Zoom X", "", 1.0, "", 0.0, 6.0};

	Parameter loudnessParam{"RMS", "", 0.5, "", 0.0, 1.0};
	Parameter toneParam{"Tone Color", "", 0.5, "", 0.0, 1.0};
	Parameter pitchParam{"Pitch", "", 0.5, "", 0.0, 1.0};

	ControlGUI gui;

	Mesh spectrum{Mesh::LINE_STRIP};
	Mesh resynth_spectrum{Mesh::LINE_STRIP};
	Mesh line{Mesh::LINES};

	float minimum{std::numeric_limits<float>::max()};
	float maximum{-std::numeric_limits<float>::max()};

	void onCreate() override {
		gui << loudnessParam;
		gui << toneParam;
		gui << pitchParam;
		gui.init();
		navControl().useMouse(false);

		frequencyFilter.freq(25);
		rateFilter.freq(25);

		gam::sampleRate(audioIO().framesPerSecond());
		player.load("../philip_voice2.wav");

		numBins = stft.numBins();

		for (int i = 0; i < numBins; i++) {
			spectrum.vertex(2.0 * i / numBins - 1);
			resynth_spectrum.vertex(2.0 * i / numBins - 1);
		}

		line.vertex(0, 1);
		line.vertex(0, 0);

		// begin MLPACK
		mlpack::data::Load("../mega.meta.csv", dataset, arma::csv_ascii);
		mlpack::data::Load("../mega.meta.no.index.csv", datasetNoIndex, arma::csv_ascii);
    std::cout << datasetNoIndex.n_rows << " rows : " << datasetNoIndex.n_cols << endl;

		std::cout << "✅ Corpus file list loaded" << std::endl;

		normDatasetNoIndex = arma::normalise(datasetNoIndex);

		std::ifstream file("../fileEntries.csv");
		CSVRow row;
		while (file >> row) {
			CorpusFile f{row[0], std::stoi(row[1]),
				     std::stoi(row[2])};
			corpus.push_back(f);
			std::cout << "row[0](" << row[0] << ")\n";
		}
		std::cout << "✅ Corpus file list loaded" << std::endl;

		// tell our NeighborSearch object (knn) to use
		// the dataset
		myknn.Train(normDatasetNoIndex);
		std::cout << "✅ Dataset (norm, no index) loaded" << std::endl;
	}

	void onSound(AudioIOData& io) override {
		while (io()) {

      

			io.out(0) = io.out(1) = player();
		}
	}

	bool onKeyDown(const Keyboard& k) override {
		// std::cout << "on keyboard! " << k.key() << std::endl;
		minimum = std::numeric_limits<float>::max();
		maximum = -std::numeric_limits<float>::max();
		//arma::mat query(3, 1, arma::fill::randu);
		arma::mat query = { { loudnessParam, toneParam, pitchParam } };

		// transpose query matrix with .t()
		// mlpack::data::load does the transpose automagically
		 myknn.Search(query.t(), 1, neighbors, distances);
		//myknn.Search(10, neighbors, distances);

		for (size_t i = 0; i < neighbors.n_elem; ++i) {
			int lineIndex = neighbors[i];
			string fileName;
			int sampleLocation;
			for (size_t j = 0; j < corpus.size(); j++) {
				if (lineIndex >= corpus[j].startFrame &&
				    lineIndex <
					lineIndex + corpus[j].lengthInFrames) {
				  fileName = corpus[j].filePath;
				  sampleLocation = dataset(0, lineIndex);
				}
			}
			std::cout << "#" << i << ": " << neighbors[i] << " "
				  << fileName << ", " << sampleLocation << std::endl;
			// open audio file, play samples
			const char * fileNameCharStar = fileName.c_str();
			std::cout << "loading...." << std::endl;
      player.load(fileNameCharStar);
			std::cout << player.frames() << "<-- how many samples this file has" << std::endl;
			std::cout << sampleLocation << "<-- sample-index we're shooting for" << std::endl;

			player.pos((double)sampleLocation);
			// tell the audio thread what to play
		}

		neighbors.clear();
		distances.clear();
	}

	void onDraw(Graphics& g) override {
		if (freezeParam == 0.0f) {
			g.clear(0.0);
			g.camera(Viewpoint::IDENTITY);	// Ortho [-1:1] x [-1:1]
			g.pushMatrix();
			g.color(1, 1, 1);
			// g.translate(-1, 0, 0);
			g.scale(zoomXParam.get(), 1, 1);
			g.translate((zoomXParam.get() * 2.0 - 2.0) /
					(zoomXParam.get() * 2.0),
				    0, 0);
			g.draw(spectrum);
			g.color(0.1, 1, 0.1);
			g.draw(resynth_spectrum);
			g.color(1, 1, 1);

			if (peaksVector.size() > 0) {
				for (int i = 0; i < numPeaks; i++) {
					hz = audioIO().framesPerSecond() /
					     2.0f * peaksVector[i] / numBins;
					g.pushMatrix();
					g.translate(
					    diy::map(
						hz, 0,
						audioIO().framesPerSecond() / 2,
						-1, 1),
					    0, 0);
					g.draw(line);
					g.popMatrix();
				}
			}
			g.popMatrix();
		}
		gui.draw(g);
	}
};

int main() {
	AudioDevice dev = AudioDevice::defaultOutput();
	MyApp app;
	app.configureAudio(dev, dev.defaultSampleRate(), 512,
			   dev.channelsOutMax(), dev.channelsInMax());

	app.start();
}
