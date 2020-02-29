#include "Gamma/Filter.h"
#include "Gamma/Oscillator.h"
#include "Gamma/SamplePlayer.h"
#include "Gamma/SoundFile.h"
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

typedef gam::SamplePlayer<float, gam::ipl::Cubic, gam::phsInc::Loop>
    samplePlayer;

samplePlayer * mainPlayer;

typedef mlpack::neighbor::NeighborSearch<   //
    mlpack::neighbor::NearestNeighborSort,  //
    mlpack::metric::EuclideanDistance,	    //
    arma::mat,				    //
    mlpack::tree::BallTree>		    //
    MyKNN;

struct CorpusFile {
	string filePath;
	int startFrame = 0, lengthInFrames = 0;
	samplePlayer player; // gam::SoundFile soundFile;
};

struct MyApp : App {
	MyKNN myknn;
	arma::mat dataset;
	arma::mat datasetNoIndex;
	arma::mat normDatasetNoIndex;
	arma::mat distances;
	arma::Mat<size_t> neighbors;
	vector<CorpusFile> corpus;
	bool fileLoaded = false;

	float hz;
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

		// begin MLPACK
		mlpack::data::Load("../mega.meta.csv", dataset,
				   arma::csv_ascii);
		mlpack::data::Load("../mega.meta.no.index.csv", datasetNoIndex,
				   arma::csv_ascii);
		std::cout << datasetNoIndex.n_rows
			  << " rows : " << datasetNoIndex.n_cols << endl;

		normDatasetNoIndex = arma::normalise(datasetNoIndex);

		std::ifstream file("../fileEntries.csv");
		CSVRow row;
		while (file >> row) {
			corpus.emplace_back(); // init CorpusFile on the end of the vector
			// this implicitly calls new to create new samplePlayer on the heap
			corpus.back().player.load(row[0].c_str());
			corpus.back().filePath = row[0];
			corpus.back().startFrame = std::stoi(row[1]);
			corpus.back().lengthInFrames = std::stoi(row[2]);

			std::cout << '.';
			mainPlayer = &corpus.back().player;
		} // but here......
		// once you leave the scope, the sp (sample player) gets deleted...
		// 
		fileLoaded = true;
		std::cout << std::endl << "✅ Corpus file list loaded" << std::endl;

		// tell our NeighborSearch object (knn) to use
		// the dataset
		myknn.Train(normDatasetNoIndex);
		std::cout << "✅ Dataset (norm, no index) loaded" << std::endl;
	}

	void onSound(AudioIOData& io) override {
		while (io()) {
			float out = fileLoaded ? mainPlayer->operator()() : 0.0;
			io.out(0) = io.out(1) = out;
		}
	}

	bool onKeyDown(const Keyboard& k) override {
		minimum = std::numeric_limits<float>::max();
		maximum = -std::numeric_limits<float>::max();
		float keyboardPitch = (float)(k.key()-48)/10.0f;
		std::cout << keyboardPitch << std::endl;
		arma::mat query = {{keyboardPitch, keyboardPitch, keyboardPitch}};
		//arma::mat query = {{loudnessParam, toneParam, keyboardPitch}};

		// transpose query matrix with .t()
		// (mlpack::data::load does the transpose automagically)
		myknn.Search(query.t(), 1, neighbors, distances);
		// myknn.Search(10, neighbors, distances);

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
					mainPlayer = &corpus[j].player;
				}
			}
			std::cout << "#" << i << ": " << neighbors[i] << " "
				  << fileName << ", " << sampleLocation
				  << std::endl;
			const char* fileNameCharStar = fileName.c_str();

			mainPlayer->pos((double)sampleLocation);

		}

		neighbors.clear();
		distances.clear();
	}

	void onDraw(Graphics& g) override {
		g.clear(0.0);
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
