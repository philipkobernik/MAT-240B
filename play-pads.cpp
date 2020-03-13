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
	// samplePlayer player;  // gam::SoundFile soundFile;
};

struct CloudFrame {
	string fileName;
	int sampleLocation;
	int frameLength;
	samplePlayer player;
};

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

struct MyApp : App {
	samplePlayer* mainPlayer;
	samplePlayer* prevPlayer = nullptr;
	MyKNN myknn;
	arma::mat dataset;
	arma::mat datasetPads;
	arma::mat normDatasetNoIndex;
	arma::mat distances;
	arma::Mat<size_t> neighbors;
	vector<CorpusFile> corpus;
	vector<CloudFrame> cloud;
	vector<Note> notes;
	int padIndex = 0;
	bool filesLoaded = false;
	bool grain = true;

	gam::Sine<> osc;
	gam::OnePole<> frequencyFilter, rateFilter;
	Parameter zoomXParam{"Zoom X", "", 1.0, "", 0.0, 6.0};

	Parameter loudnessParam{"RMS", "", 0.5, "", 0.0, 1.0};
	Parameter toneParam{"Tone Color", "", 0.5, "", 0.0, 1.0};
	Parameter kurtosisParam{"Spectral Kurtosis", "", 0.5, "", 0.0, 1.0};
	Parameter differenceParam{"Spectral Difference", "", 0.5, "", 0.0, 1.0};
	Parameter pitchParam{"Pitch", "", 0.5, "", 0.0, 1.0};
	ParameterInt neighborsParam{"Neighbors", "", 30, "", 1, 50};

	ControlGUI gui;

	Mesh mesh;
	Mesh line;

	float minimum{std::numeric_limits<float>::max()};
	float maximum{-std::numeric_limits<float>::max()};

	void onCreate() override {
		std::cout << std::endl << "starting app" << std::endl;
		gui << loudnessParam;
		gui << toneParam;
		gui << kurtosisParam;
		gui << differenceParam;
		gui << pitchParam;
		gui << neighborsParam;
		gui.init();
		navControl().useMouse(false);

		frequencyFilter.freq(25);
		rateFilter.freq(25);

		mesh.primitive(Mesh::POINTS);
		line.primitive(Mesh::LINES);
		line.vertex(0, 0, 0);
		line.vertex(1, 1, 1);

		std::cout << std::endl << "starting load pads" << std::endl;
		std::ifstream padsFile("../pads.csv");
		CSVRow padRow;
		while (padsFile >> padRow) {
			notes.emplace_back();
			notes.back().lineIndex = stoi(padRow[0]);
			notes.back().sampleLocation = stoi(padRow[1]);
			notes.back().midiPitch = stof(padRow[2]);
			notes.back().lengthInFrames = stoi(padRow[3]);
			notes.back().rms = stof(padRow[4]);
			notes.back().tone = stof(padRow[5]);
			notes.back().onset = stof(padRow[6]);
			notes.back().peakiness = stof(padRow[7]);
		}
		std::cout << std::endl << "finished load pads" << std::endl;

		// begin MLPACK
		std::cout << std::endl << "starting mega.metas" << std::endl;
		mlpack::data::Load("../mega.meta.csv", dataset,
				   arma::csv_ascii);
		mlpack::data::Load("../pads.csv", datasetPads, arma::csv_ascii);
		std::cout << datasetPads.n_rows
			  << " rows : " << datasetPads.n_cols << endl;
		std::cout << std::endl << "done mega.metas" << std::endl;

		arma::min(datasetPads, 1).print();
		arma::max(datasetPads, 1).print();

		cout << endl;

		arma::min(dataset, 1).print();
		arma::max(dataset, 1).print();

		std::cout << std::endl << "starting fileEntries" << std::endl;
		std::ifstream file("../fileEntries.csv");
		CSVRow row;
		while (file >> row) {
			corpus.emplace_back();	// init CorpusFile on the end of
						// the vector
			// this implicitly calls new to create new samplePlayer
			// on the heap
			// corpus.back().player.load(row[0].c_str());  // load
			// file
			corpus.back().filePath = row[0];
			corpus.back().startFrame = std::stoi(row[1]);
			corpus.back().lengthInFrames = std::stoi(row[2]);
			// TODO: add id3 tag metadata

			std::cout << '.';
		}

		std::cout << std::endl << "✅ Corpus list loaded" << std::endl;

		// tell our NeighborSearch object (knn) to use
		// the dataset
		// myknn.Train(datasetNoIndex);
		std::cout << "✅ Dataset (norm, no index) loaded" << std::endl;

		gam::sampleRate(audioIO().framesPerSecond());
	}

	void onSound(AudioIOData& io) override {
		int frameSize = 4096;
		Note note = notes[padIndex];
		// if (filesLoaded && cloud.size() > 0) {
		// if (grain) {
		// CloudFrame f1 =
		// cloud[rng.uniform(cloud.size() - 1)];
		// CloudFrame f2 =
		// cloud[rng.uniform(cloud.size() - 1)];
		// f1.player.pos(f1.sampleLocation);
		// f2.player.pos(f2.sampleLocation);
		// float prevSample =
		// prevPlayer == nullptr
		//? 0
		//: prevPlayer->operator()();
		// float gain = 0.75f;

		// for (int i = 0; i < frameSize; i++) {
		// io.outBuffer(0)[i] =
		// io.outBuffer(1)[i] = f1.player();
		//}
		// prevPlayer = &f2.player;
		// return;
		//}
		//}

		while (io()) {
			float f = 0;
			if (filesLoaded && cloud.size() > 0) {
				int noteEnd = (cloud[0].frameLength - 1) *
						  (frameSize / 4) +
					      cloud[0].sampleLocation;
				if (mainPlayer->pos() > noteEnd) {
					mainPlayer->pos(
					    cloud[0].sampleLocation);
				}

				f = mainPlayer->operator()();
			}
			io.out(0) = io.out(1) = f;
		}
	}

	bool onKeyDown(const Keyboard& k) override {
		float keyboardPitch = (float)(k.key() - 48) / 10.0f;
		std::cout << keyboardPitch << std::endl;

		if (k.key() == 'g') {
			grain = !grain;
			return true;
		}
		padIndex++;
		filesLoaded = false;  // should stop playback
		cloud.clear();	      // should release samples
		Note note = notes[padIndex];
    cout << "rms " << note.rms << endl;
		cout << "tone " << note.tone << endl;
		cout << "onset " << note.onset << endl;
		cout << "peakiness " << note.peakiness << endl;

		int lineIndex = note.lineIndex;
		string fileName;
		int sampleLocation = note.sampleLocation;
		for (size_t j = 0; j < corpus.size(); j++) {
			// cout << "lineIndex " << lineIndex << endl;
			// cout << "corpus startframe " << corpus[j].startFrame
			// << endl; cout << "corpus startframe+lengthInFrames "
			// << corpus[j].startFrame+corpus[j].lengthInFrames <<
			// endl;
			if (lineIndex >= corpus[j].startFrame &&
			    lineIndex < corpus[j].startFrame +
					    corpus[j].lengthInFrames) {
				// add to list
				fileName = corpus[j].filePath;
				// sampleLocation = dataset(0, lineIndex);

				cloud.emplace_back();
				cloud.back().fileName = fileName;
				cloud.back().sampleLocation = sampleLocation;
				cloud.back().frameLength = note.lengthInFrames;
				cloud.back().player.load(fileName.c_str());

				mainPlayer = &cloud.back().player;
				mainPlayer->pos(sampleLocation);
				break;
			}
		}
		// cloud of 1
		std::cout << "#" << padIndex << ": " << fileName << ", "
			  << sampleLocation << std::endl;

		filesLoaded = true;
		neighbors.clear();
		distances.clear();
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
