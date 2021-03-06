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

struct CloudFrame {
	string fileName;
	int sampleLocation;
	samplePlayer player;
};

struct MyApp : App {
  samplePlayer* mainPlayer;
  samplePlayer* prevPlayer = nullptr;
	MyKNN myknn;
	arma::mat dataset;
	arma::mat datasetNoIndex;
	arma::mat normDatasetNoIndex;
	arma::mat distances;
	arma::Mat<size_t> neighbors;
	vector<CorpusFile> corpus;
	vector<CloudFrame> cloud;
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
    std::cout << std::endl << "starting mega.metas" << std::endl;
		mlpack::data::Load("../mega.meta.csv", dataset,
				   arma::csv_ascii);
		mlpack::data::Load("../mega.meta.no.index.csv", datasetNoIndex,
				   arma::csv_ascii);
		std::cout << datasetNoIndex.n_rows
			  << " rows : " << datasetNoIndex.n_cols << endl;
    std::cout << std::endl << "done mega.metas" << std::endl;

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
		if (filesLoaded && cloud.size() > 0) {
			if (grain) {
				CloudFrame f1 = cloud[rng.uniform(cloud.size() - 1)];
				CloudFrame f2 = cloud[rng.uniform(cloud.size() - 1)];
				f1.player.pos(f1.sampleLocation);
				f2.player.pos(f2.sampleLocation);
				float prevSample = prevPlayer == nullptr ? 0 : prevPlayer->operator()();
				float gain = 0.75f;

				// 1b
				// 2a 2b
				//    2a
				// quad overlap-and-add sliding window algo
				//
				// 1a 1b 1c  1d                        fO1 .75  o3
				//    2a 2b  2c 2d                     fO2 .5   o2
				//       3a  3b 3c 3d                  mO3 .25  o1
				//           4a 4b 4c 4d               mFull .0 n1
				//              1a 1b 1c  1d           mI3      n2
				//                 2a 2b  2c 2d        fI2      n3
				//                    3a  3b 3c 3d     fI1      n4
				//                        4a 4b 4c 4d
				//
				//1d         
				//2c 2d      
				//3b 3c 3d   
				//4a 4b 4c 4d 
				//   1a 1b 1c  1d         
				//      2a 2b  2c 2d      
				//         3a  3b 3c 3d   
				//             4a 4b 4c 4d 
				//                1a 1b 1c  1d         
				//                   2a 2b  2c 2d      
				//                      3a  3b 3c 3d   
				//                          4a 4b 4c 4d
				//                             1a 1b 1c
				//                                2a 2b
				//                                   3a
				//[shift]:
				//             2d           o1        -> 0-1024
				//             3c 3d        o2        -> 0-2048
				//             4b 4c 4d     o3        -> 0-3072
				//             1a 1b 1c 1d  f         -> 0-4096
				//                2a 2b 2c  i3 -> o1  -> 1024-4096
				//                   3a 3b  i2 -> o2  -> 2048-4096
				//                      4a  i1 -> o3  -> 3072-4096
				//             0                  f   ->
				//                1024            i3  ->
				//                   2048         i2  ->
				//                      3072      i1  ->
				//
				//             on each frame
				//              - 3 will be from prev: i(1-3)->o(1-3)
				//              - 4 will be new
				//              - 1 will be done in the frame
				//             
				//
				//
				//
				//

				// instead of getting one random frame from the
				// cloud, I could get 4 frames and
				// window/overlap them make buffer that is
				// zero'd out, then do += with each
				// windowed/overlapped frames
				for (int i = 0; i < frameSize/2; i++) {
				  float phase = diy::map(i, 0, frameSize/2, 0.0f, 1.0f);
							float fadeIn = 2.0f*phase-(phase*phase);
							float fadeOut = -1.0f * (phase*phase) + 1.0f;
					io.outBuffer(0)[i] =
				    io.outBuffer(1)[i] =
				      // window this first half
							// 2x-x^2
				      f1.player()*fadeIn*gain + // first half f1
				        prevSample*fadeOut*gain; // 2nd half prev sample
								
				}
				for (int i = frameSize/2; i < frameSize; i++) {
				  float phase = diy::map(i, frameSize/2, frameSize, 0.0f, 1.0f);
							float fadeIn = 2.0f*phase-(phase*phase);
							float fadeOut = -1.0f * (phase*phase) + 1.0f;
					io.outBuffer(0)[i] =
				    io.outBuffer(1)[i] =
				      f1.player()*fadeOut*gain + // 2nd half f1
				        f2.player()*fadeIn*gain; // first half f2
				}
				prevPlayer = &f2.player;
				return;

			}
		}

		while (io()) {
			float f = (filesLoaded && cloud.size() > 0) ? mainPlayer->operator()() : 0;
			io.out(0) = io.out(1) = f;
		}
	}

	bool onKeyDown(const Keyboard& k) override {
		float keyboardPitch = (float)(k.key() - 48) / 10.0f;
		std::cout << keyboardPitch << std::endl;

		if(k.key() == 'g') {
				grain = !grain;
				return true;
		}
		// arma::mat query = {
		//{keyboardPitch, keyboardPitch, keyboardPitch}};
		arma::mat query = {{loudnessParam, toneParam, kurtosisParam,
				    differenceParam, keyboardPitch}};
		if (line.vertices().size()) {
			Vec3f pv(toneParam, kurtosisParam, differenceParam);
			line.vertices()[0] = pv;
		}

		// transpose query matrix with .t()
		// (mlpack::data::load does the transpose automagically)
		myknn.Search(query.t(), neighborsParam, neighbors, distances);
		// myknn.Search(10, neighbors, distances);

		filesLoaded = false; // should stop playback
		cloud.clear(); // should release samples
		for (size_t i = 0; i < neighbors.n_elem; ++i) {
			int lineIndex = neighbors[i];
			string fileName;
			int sampleLocation;
			for (size_t j = 0; j < corpus.size(); j++) {
				if (lineIndex >= corpus[j].startFrame &&
				    lineIndex < corpus[j].startFrame +
						    corpus[j].lengthInFrames) {
					// add to list
					fileName = corpus[j].filePath;
					sampleLocation = dataset(0, lineIndex);

					cloud.emplace_back();
					cloud.back().fileName = fileName;
					cloud.back().sampleLocation =
					    sampleLocation;
					cloud.back().player.load(fileName.c_str());

				  mainPlayer = &cloud.back().player;
					mainPlayer->pos(sampleLocation);
					break;
				}
			}
			std::cout << "#" << i << ": " << neighbors[i] << " "
				  << fileName << ", " << sampleLocation
				  << std::endl;
		}

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
