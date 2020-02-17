#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <iostream>
#include <limits>
#include <set>
#include <valarray>
#include <vector>

#include "Gamma/DFT.h"	// gam::STFT
#include "Gamma/Filter.h"
#include "Gamma/Oscillator.h"
#include "Gamma/SamplePlayer.h"
#include "al/app/al_App.hpp"
#include "al/ui/al_ControlGUI.hpp"  // gui.draw(g)

using namespace al;
using namespace std;

gam::STFT stft(4096, 4096, 0, gam::HAMMING);
gam::SamplePlayer<float, gam::ipl::Cubic, gam::phsInc::Loop> player;

#include "AudioFFT.cpp"
#include "AudioFFT.h"
#include "functions.h"

const int frameSize = 4096;
const size_t fftSize = 4096;  // Needs to be power of 2!
double sample_rate;

vector<float> input(fftSize, 0.0f);
vector<float> re(audiofft::AudioFFT::ComplexSize(fftSize));
vector<float> im(audiofft::AudioFFT::ComplexSize(fftSize));
vector<float> output(fftSize);

audiofft::AudioFFT fft;

string serialize_top_n_peaks(vector<int> peaks, int n) {
	string result = "";
	for (int i = 0; i < n; i++) {
					result += peaks[i] / (fftSize/2) * sample_rate;
					result += ":";
					result += log(abs(re[peaks[i]]));
					result += " ";
	}

	return result;
}

bool peaksComparator2(int a, int b) {
	return log(abs(re[a])) > log(abs(re[b]));
}

bool isPeak(int binIndex, int peakNeighbors, int numBins) {
	if (binIndex < peakNeighbors || binIndex >= numBins - peakNeighbors)
		return false;

	float m = log(abs(re[binIndex]));
	for (int i = 1; i <= peakNeighbors; i++) {
		if (m <= log(abs(re[binIndex - i]))) return false;
		if (m <= log(abs(re[binIndex + i]))) return false;
	}
	return true;
}

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

struct MyApp : App {
	float hz;
	vector<int> gamPeaksVector;
	vector<int> peaksVector;
	int numBins;
	int numPeaks;
	int sampleIndex;
	gam::Sine<> osc;
	gam::OnePole<> frequencyFilter, rateFilter;
	Parameter rate{"Playback Speed", "", 0.5, "", 0.0, 2.0};
	ParameterInt numPeaksParam{"Top N Peaks", "", 8, "", 1, 50};
	ParameterInt peakNeighborsParam{
	    "Compare +/- Neighboring Bins", "", 20, "", 1, 50};
	ParameterBool freezeParam{"Freeze", "", 0.0};
	ParameterBool sourceFileParam{"Sound File Input", "", 1};
	Parameter zoomXParam{"Zoom X", "", 1.0, "", 0.0, 6.0};
	ControlGUI gui;

	Mesh spectrum{Mesh::LINE_STRIP};
	Mesh resynth_spectrum{Mesh::LINE_STRIP};
	Mesh line{Mesh::LINES};
	Mesh line2{Mesh::LINES};

	float minimum{std::numeric_limits<float>::max()};
	float maximum{-std::numeric_limits<float>::max()};

	void onCreate() override {
		gui << rate;
		gui << numPeaksParam;
		gui << peakNeighborsParam;
		gui << zoomXParam;
		gui << sourceFileParam;
		gui << freezeParam;
		gui.init();
		navControl().useMouse(false);

		fft.init(frameSize);

		frequencyFilter.freq(25);
		rateFilter.freq(25);

		sample_rate = audioIO().framesPerSecond();
		gam::sampleRate(sample_rate);

		player.load("../../philip_voice2.wav");

		numBins = stft.numBins();
		cout << "Gamma STFT bins: " << numBins << endl;
		cout << "audioFFT bins: " << re.size() << endl;
		sampleIndex = 0;

		// init meshes
		for (int i = 0; i < numBins; i++) {
			spectrum.vertex(2.0 * i / numBins - 1);
		}

		for (int i = 0; i < re.size(); i++) {
			resynth_spectrum.vertex(2.0 * i / re.size() - 1);
		}

		// init line
		line.vertex(0, 1);
		line.vertex(0, 0);

		line2.vertex(0, 0);
		line2.vertex(0, -1);
	}

	void onSound(AudioIOData& io) override {
		while (io()) {
			player.rate(rateFilter(rate.get()));
			numPeaks = numPeaksParam.get();
			float sample = sourceFileParam ? player() : io.in(0);

			if (stft(sample)) {
				numBins = stft.numBins();
				gamPeaksVector.clear();

				for (int i = 0; i < numBins; i++) {
					// calculate statistics on the magnitude
					// of the bin
					float m = log(stft.bin(i).mag());
					if (m > maximum) maximum = m;
					if (m < minimum) minimum = m;
					spectrum.vertices()[i].y =
					    diy::map(m, minimum, maximum, 0, 1);

					// search for peaks
					if (binGreaterThanNeighbors(
						i, peakNeighborsParam.get(),
						numBins)) {
						gamPeaksVector.push_back(i);
					}
				}

				// sort peaks
				std::sort(gamPeaksVector.begin(),
					  gamPeaksVector.end(),
					  peaksComparator);

				for (int i = 0; i < numBins; i++) {
					bool found = false;
					for (int j = 0; j < numPeaks; j++) {
						if (gamPeaksVector[j] == i)
							found = true;
					}
					if (found == false) {
						stft.bin(i).mag(0.0);
					}
				}
			}

			input[sampleIndex] = sample;
			sampleIndex++;

			// forward fft
			if (sampleIndex >= frameSize - 1) {  // frame is ready
				sampleIndex = 0;
				peaksVector.clear();

				fft.fft(input.data(), re.data(), im.data());

				for (int i = 0; i < re.size(); i++) {
					float m = log(abs(re[i]));
					if (m > maximum) maximum = m;
					if (m < minimum) minimum = m;
					resynth_spectrum.vertices()[i].y =
					    diy::map(m, minimum, maximum, -1,
						     0);

					if (isPeak(i, peakNeighborsParam.get(),
						   re.size())) {
						peaksVector.push_back(i);
					}
				}

				std::sort(peaksVector.begin(),
					  peaksVector.end(), peaksComparator2);
				cout << serialize_top_n_peaks(
					    peaksVector, numPeaksParam.get())
				     << endl;
			}

			io.out(0) = sourceFileParam * player() * 0.2;
			io.out(1) = sourceFileParam * player() * 0.2;
		}
	}

	bool onKeyDown(const Keyboard& k) override {
		minimum = std::numeric_limits<float>::max();
		maximum = -std::numeric_limits<float>::max();
		return true;
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

			if (gamPeaksVector.size() > 0) {
				for (int i = 0; i < numPeaks; i++) {
					hz = audioIO().framesPerSecond() /
					     2.0f * gamPeaksVector[i] / numBins;
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

			if (peaksVector.size() > 0) {
				for (int i = 0; i < numPeaks; i++) {
					hz = audioIO().framesPerSecond() /
					     2.0f * peaksVector[i] / re.size();
					g.pushMatrix();
					g.translate(
					    diy::map(
						hz, 0,
						audioIO().framesPerSecond() / 2,
						-1, 1),
					    0, 0);
					g.draw(line2);
					g.popMatrix();
				}
			}
			g.popMatrix();
		}
		gui.draw(g);
	}
};

int main() {
	MyApp app;
	app.start();
}
