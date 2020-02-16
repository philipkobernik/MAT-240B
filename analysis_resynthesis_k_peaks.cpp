#include "Gamma/DFT.h"  // gam::STFT
#include "Gamma/Filter.h"
#include "Gamma/Oscillator.h"
#include "Gamma/SamplePlayer.h"
#include "al/app/al_App.hpp"
#include "al/ui/al_ControlGUI.hpp"  // gui.draw(g)
using namespace al;
using namespace std;

#include <limits>
#include <set>
#include <algorithm>
#include <array>
#include <vector>
#include "functions.h"


gam::STFT stft(4096, 4096 / 4, 16384, gam::HAMMING);
gam::STFT re_stft(4096, 4096 / 4, 16384, gam::HAMMING);
gam::SamplePlayer<float, gam::ipl::Cubic, gam::phsInc::Loop> player;

bool peaksComparator(int a, int b) {
  return log(stft.bin(a).mag()) > log(stft.bin(b).mag());
}

bool binGreaterThanNeighbors(int binIndex, int peakNeighbors) {
          float m = log(stft.bin(binIndex).mag());
	  for(int i = 1; i <= peakNeighbors; i++) {
		if(m <= log(stft.bin(binIndex - i).mag())) return false;
		if(m <= log(stft.bin(binIndex + i).mag())) return false;
	  }
	  return true;
}

struct MyApp : App {
  float hz;
  std::vector<int> peaksVector;
  int numBins;
  int numPeaks;
  gam::Sine<> osc;
  gam::OnePole<> frequencyFilter, rateFilter;
  Parameter rate{"Speed", "", 0.5, "", 0.0, 2.0};
  ParameterInt numPeaksParam{"/numPeaksParam", "", 3, "", 1, 20};
  ParameterInt peakNeighborsParam{"/peakNeighborsParam", "", 4, "", 1, 20};
  ParameterBool drawParam{"Pause", "", 1.0};
  ControlGUI gui;
  //std::set<int> first;

  Mesh spectrum{Mesh::LINE_STRIP};
  Mesh resynth_spectrum{Mesh::LINE_STRIP};
  Mesh line{Mesh::LINES};

  float minimum{std::numeric_limits<float>::max()};
  float maximum{-std::numeric_limits<float>::max()};

  void onCreate() override {
    gui << rate;
    gui << numPeaksParam;
    gui << peakNeighborsParam;
    gui << drawParam;
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
    line.vertex(0, -1);
  }

  void onSound(AudioIOData& io) override {
    while (io()) {
      player.rate(rateFilter(rate.get()));
      numPeaks = numPeaksParam.get();
      float f = player();
      //float f = io.in(0);


      std::set<int> peaks;

      //std::array<int, 30> peaks;
      //peaks.fill(0);
      	
      if (stft(f)) {
    	numBins = stft.numBins();
	//cout << "stft frame ready!" << endl;
        //
        // search for the "hottest" bins

        for (int i = 0; i < numBins; i++) {
          //
          // calculate statistics on the magnitude of the bin
          float m = log(stft.bin(i).mag());
          if (m > maximum) maximum = m;
          if (m < minimum) minimum = m;
          spectrum.vertices()[i].y = diy::map(m, minimum, maximum, 0, 1);

          // search for peaks
	  int peakNeighbors = peakNeighborsParam.get();
	  if(i < peakNeighbors || i >= numBins-peakNeighbors) continue;

          if (binGreaterThanNeighbors(i, peakNeighbors)) {
		  //cout << "found peak!" << endl;
		  peaks.insert(i);
	    }
          }
	  // make adjustments!
    // copy peaks to vector
    std::vector<int> newPeaksVector(peaks.begin(), peaks.end());
    peaksVector = newPeaksVector;
    //std::copy(peaks.begin(), peaks.end(), std::back_inserter(peaksVector));
    // sort peaks
    std::sort(peaksVector.begin(), peaksVector.end(), peaksComparator);

    	  
	  for(int i = 0; i < numBins; i++) {
		  bool found = false;
		  for(int j = 0; j < numPeaks; j++) {
			if(peaksVector[j] == i) found = true;
		  }
		  if(found == false) {
			  stft.bin(i).mag(0.0);
		  }
	  }

        }

    float resynth = 0.0f;

    if(false) { //peaksVector.size() > 0) {
      // why doesn't this work?
      // additive synthesis approach
      cout << "resynth: ";
      for(int i = 0; i < numPeaks; i++) {
        hz = audioIO().framesPerSecond() / 2.0f * peaksVector[i] / numBins;
	cout << hz << ", ";
	osc.freq(frequencyFilter(hz));
	resynth += osc();
      }
      cout << endl;
    }


    float resynth_stft = stft();

      // feed resynth to the 2nd stft
      if (re_stft(resynth_stft)) {
        int reNumBins = re_stft.numBins();
        for (int i = 0; i < reNumBins; i++) {
          //
          // calculate statistics on the magnitude of the bin
          float m = log(re_stft.bin(i).mag());
          if (m > maximum) maximum = m;
          if (m < minimum) minimum = m;
          resynth_spectrum.vertices()[i].y = diy::map(m, minimum, maximum, -1, 0);
        }

      }

      io.out(0) = resynth_stft * 1.1;
      //io.out(1) = 0.0f;
      io.out(1) = player() * 0.1;
    }
  }

  bool onKeyDown(const Keyboard& k) override {
    minimum = std::numeric_limits<float>::max();
    maximum = -std::numeric_limits<float>::max();
  }

  void onDraw(Graphics& g) override {
    if (drawParam == 1.0f) {
	    g.clear(0.3);
    g.camera(Viewpoint::IDENTITY);  // Ortho [-1:1] x [-1:1]
    g.color(0, 1, 0);
    g.draw(spectrum);
    g.color(0, 0, 1);
    g.draw(resynth_spectrum);
    g.color(1, 0, 0);

    if(peaksVector.size() > 0) {
      for(int i = 0; i < numPeaks; i++) {
        hz = audioIO().framesPerSecond() / 2.0f * peaksVector[i] / numBins;
	g.pushMatrix();
    	g.translate(diy::map(hz, 0, audioIO().framesPerSecond() / 2, -1, 1), 0, 0);
    	g.draw(line);
	g.popMatrix();
      }
    }

    }
    gui.draw(g);
  }
};

int main() {
  MyApp app;
  app.start();
}
