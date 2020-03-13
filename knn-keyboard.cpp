#include "Gamma/Filter.h"
#include "Gamma/Oscillator.h"
#include "Gamma/SamplePlayer.h"
#include "Gamma/SoundFile.h"
#include "al/app/al_App.hpp"
#include "al/math/al_Random.hpp"
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

rnd::Random<> rng;

typedef gam::SamplePlayer<float, gam::ipl::Cubic, gam::phsInc::Loop>
    samplePlayer;

typedef mlpack::neighbor::NeighborSearch<   //
    mlpack::neighbor::NearestNeighborSort,  //
    mlpack::metric::EuclideanDistance,	    //
    arma::mat,				    //
    mlpack::tree::BallTree>		    //
    MyKNN;

struct CorpusFile {
	string filePath;
	int startFrame = 0, lengthInFrames = 0;
	//samplePlayer player;  // gam::SoundFile soundFile;
};

struct CloudNote {
	string fileName;
	int sampleLocation;
	samplePlayer player;
	float midiPitch;
	int lengthInFrames;
};

struct MyApp : App {
  samplePlayer* mainPlayer;
	int mainPlayerLengthInFrames;
	int mainPlayerSampleLocation;

  samplePlayer* prevPlayer = nullptr;
	MyKNN myknn;
	arma::mat dataset;
	arma::mat datasetNoIndex;
	arma::mat normDatasetNoIndex;
	arma::mat distances;
	arma::Mat<size_t> neighbors;
	vector<CorpusFile> corpus;
	vector<CloudNote> cloud;
	bool filesLoaded = false;
	bool grain = true;

	gam::Sine<> osc;
	gam::OnePole<> frequencyFilter, rateFilter;

	Parameter loudnessParam{"RMS", "", 0.5, "", 0.0, 1.0};
	Parameter toneParam{"Tone Color", "", 0.5, "", 0.0, 1.0};
	Parameter onsetParam{"Onset", "", 0.5, "", 0.0, 1.0};
	Parameter peakinessParam{"Peakiness", "", 0.5, "", 0.0, 1.0};

	ControlGUI gui;

	Mesh mesh;
	Mesh line;

	float minimum{std::numeric_limits<float>::max()};
	float maximum{-std::numeric_limits<float>::max()};

	void onCreate() override {
    std::cout << std::endl << "starting app" << std::endl;
		gui << loudnessParam;
		gui << toneParam;
		gui << onsetParam;
		gui << peakinessParam;
		gui.init();
		navControl().useMouse(false);

		frequencyFilter.freq(25);
		rateFilter.freq(25);

		mesh.primitive(Mesh::POINTS);
		line.primitive(Mesh::LINES);
		line.vertex(0, 0, 0);
		line.vertex(1, 1, 1);

    std::cout << std::endl << "starting loadmesh" << std::endl;
		//std::ifstream meshFile("../mega.meta.no.index.csv");
		//CSVRow meshRow;
		//while (meshFile >> meshRow) {
			//Vec3f v(std::stof(meshRow[1]), std::stof(meshRow[2]),
				//std::stof(meshRow[3]));
			//// map higher dimensions to color space!
			//mesh.vertex(v);
		//}
    std::cout << std::endl << "done loadmesh" << std::endl;


		// arma::mat tm = {
		//{1, 2, 3},
		//{4, 5, 6},
		//{7, 8, 9}
		//};
		// tm.print();
		// arma::normalise(tm, 2, 1).print();

		// begin MLPACK
    std::cout << std::endl << "starting pads" << std::endl;
		mlpack::data::Load("../notes.csv", dataset,
				   arma::csv_ascii);
		mlpack::data::Load("../notes.norm.no.index.csv", datasetNoIndex,
				   arma::csv_ascii);
		std::cout << datasetNoIndex.n_rows
			  << " rows : " << datasetNoIndex.n_cols << endl;
    std::cout << std::endl << "done pads" << std::endl;

		// normDatasetNoIndex = arma::normalise(datasetNoIndex, 2, 1);
		// mlpack::data::Save("../normalised.no.index.csv",
		// normDatasetNoIndex);

		// std::cout << std::endl;
		// std::cout << std::endl;

		arma::min(datasetNoIndex, 1).print();
		arma::max(datasetNoIndex, 1).print();

		cout << endl;

		arma::min(dataset, 1).print();
		arma::max(dataset, 1).print();

		// arma::max(normDatasetNoIndex, 1).print();
		// arma::min(normDatasetNoIndex, 1).print();

    std::cout << std::endl << "starting fileEntries" << std::endl;
		std::ifstream file("../fileEntries.csv");
		CSVRow row;
		while (file >> row) {
			corpus.emplace_back();	// init CorpusFile on the end of
						// the vector
			// this implicitly calls new to create new samplePlayer
			// on the heap
			//corpus.back().player.load(row[0].c_str());  // load file
			corpus.back().filePath = row[0];
			corpus.back().startFrame = std::stoi(row[1]);
			corpus.back().lengthInFrames = std::stoi(row[2]);
			// TODO: add id3 tag metadata

			std::cout << '.';
		}

		std::cout << std::endl << "✅ Corpus list loaded" << std::endl;

		// tell our NeighborSearch object (knn) to use
		// the dataset
		myknn.Train(datasetNoIndex);
		std::cout << "✅ Dataset (norm, no index) loaded" << std::endl;

		gam::sampleRate(audioIO().framesPerSecond());
	}

	void onSound(AudioIOData& io) override {
    int frameSize = 4096;

		while (io()) {
			float f = 0;
			if (filesLoaded && cloud.size() > 0) {
				int noteEnd = (mainPlayerLengthInFrames - 1) *
						  (frameSize / 4) +
					      mainPlayerSampleLocation;
				if (mainPlayer->pos() > noteEnd) {
					mainPlayer->pos(mainPlayerSampleLocation);
				}

				f = mainPlayer->operator()();
			}
			io.out(0) = io.out(1) = f;
		}

	}
	bool generateKeyboard() {
    // get the params:
		//float loudness = 0;
		//float tone = 0;
		//float onset = 0;
		//float peakiness = 0;

		// execute search on params
		// this is searching pads.norm.no.index.csv
		arma::mat query = {{loudnessParam, toneParam, onsetParam, peakinessParam}};
		myknn.Search(query.t(), 150, neighbors, distances);

		// map pitches to the keyboard
		filesLoaded = false; // should stop playback
		cloud.clear(); // should release samples
		for (size_t i = 0; i < neighbors.n_elem; ++i) {
			int padsIndexRow = neighbors[i];
			string fileName;
			int corpusFrameIndex = dataset(0, padsIndexRow);
      int sampleLocation = dataset(1, padsIndexRow);
			float midiPitch = dataset(2, padsIndexRow);
      int lengthInFrames = dataset(3, padsIndexRow);

			// from the 50, assign the midipitches to keys

			// loading the selection of notes into memory
			for (size_t j = 0; j < corpus.size(); j++) {
				if (corpusFrameIndex >= corpus[j].startFrame &&
				    corpusFrameIndex < corpus[j].startFrame +
						    corpus[j].lengthInFrames) {
					// add to list
					fileName = corpus[j].filePath;

					cloud.emplace_back();
					cloud.back().fileName = fileName;
					cloud.back().sampleLocation = sampleLocation;
					cloud.back().player.load(fileName.c_str());
					cloud.back().midiPitch = midiPitch;
					cloud.back().lengthInFrames = lengthInFrames;

				  mainPlayer = &cloud.back().player;
					mainPlayer->pos(sampleLocation);
				  mainPlayerLengthInFrames = lengthInFrames;
				  mainPlayerSampleLocation = sampleLocation;
					break;
				}
			}
			std::cout << "#" << i << ": midi: " << midiPitch << " "
				  << fileName << ", " << lengthInFrames
				  << std::endl;
		}

		filesLoaded = true;
		neighbors.clear();
		distances.clear();
	}

	bool onKeyDown(const Keyboard& k) override {
		float keyboardPitch = (float)(k.key() - 48 + 60-34);
		std::cout << keyboardPitch << std::endl;
		std::cout << cloud.size() << std::endl;

		if(k.key() == 'g') {
				generateKeyboard();
				return true;
		}

		// iterate the cloud, find note that is closest to keyboardPitch
		for(int i=0; i< cloud.size(); i++) {
				  CloudNote c = cloud[i];
					if(c.midiPitch > keyboardPitch - 0.2 &&
													c.midiPitch < keyboardPitch + 0.2) {
									std::cout << "found the note!" << endl;
									filesLoaded = false;
									mainPlayer = &c.player;
									mainPlayer->pos(c.sampleLocation);
					mainPlayerLengthInFrames = c.lengthInFrames;
					mainPlayerSampleLocation = c.sampleLocation;
									filesLoaded = true;
					return true;
					}
					//mainPlayer = &cloud.back().player;
					//mainPlayer->pos(sampleLocation);
					//mainPlayerLengthInFrames = lengthInFrames;
					//mainPlayerSampleLocation = sampleLocation;
		}
		std::cout << "no note found!" << endl;
		return false;
		// set that to mainPlayer?

	}

	void onDraw(Graphics& g) override {
		g.clear(0.0);
		g.draw(mesh);
		g.draw(line);
		gui.draw(g);
	}
};

int main() {
	AudioDevice dev = AudioDevice::defaultOutput();
	MyApp app;
	app.configureAudio(dev, dev.defaultSampleRate(), 4096,
			   dev.channelsOutMax(), dev.channelsInMax());

	app.start();
}
