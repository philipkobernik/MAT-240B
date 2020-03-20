#include "Gamma/Envelope.h"
#include "Gamma/Filter.h"
#include "Gamma/Oscillator.h"
#include "Gamma/SamplePlayer.h"
#include "Gamma/SoundFile.h"
#include "al/app/al_App.hpp"
#include "al/graphics/al_Shapes.hpp"
#include "al/io/al_AudioIO.hpp"
#include "al/io/al_MIDI.hpp"
#include "al/math/al_Random.hpp"
#include "al/scene/al_PolySynth.hpp"
#include "al/ui/al_ControlGUI.hpp"
using namespace gam;
using namespace al;
using namespace std;

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <mlpack/methods/neighbor_search/neighbor_search.hpp>
#include <mutex>
#include <set>
#include <vector>

#include "functions.h"
#include "parse-csv.h"

rnd::Random<> rng;

typedef gam::SamplePlayer<float, gam::ipl::Cubic, gam::phsInc::Loop>
    samplePlayer;

typedef mlpack::neighbor::NeighborSearch<   //
    mlpack::neighbor::NearestNeighborSort,  //
    mlpack::metric::EuclideanDistance,      //
    arma::mat,                              //
    mlpack::tree::BallTree>                 //
    MyKNN;

struct CorpusFile {
  string filePath;
  int startFrame = 0, lengthInFrames = 0;
};

struct CloudNote {
  string fileName;
  double sampleLocation;
  samplePlayer player;
  double midiPitch;
  double lengthInFrames;
};

struct MidiPitchCluster {
  int midiPitch;
  MyKNN knn;
  vector<CloudNote> notes;
  arma::mat dataset;
  arma::mat datasetNoIndex;
  arma::mat distances;
  arma::Mat<size_t> neighbors;

  void init(int noteNumber) {
    midiPitch = noteNumber;

    char filepath[30];
    char filepathNoIndex[30];
    sprintf(filepath, "../note-%d.csv", noteNumber);
    sprintf(filepathNoIndex, "../note-%d.no.index.csv", noteNumber);

    mlpack::data::Load(filepath, dataset, arma::csv_ascii);
    mlpack::data::Load(filepathNoIndex, datasetNoIndex, arma::csv_ascii);

    std::cout << "midiCluster " << noteNumber << ": " << datasetNoIndex.n_rows
              << " x " << datasetNoIndex.n_cols << endl;
    train();
  }

  void train() {
    knn.Train(datasetNoIndex);
    std::cout << "midiCluster " << midiPitch << ": training done." << endl;
  }
  void query(arma::mat queryMatrix) {
    knn.Search(queryMatrix.t(), 1, neighbors, distances);
  }
  void loadNote(vector<CorpusFile> corpus) {  // should this be a pointer?
    notes.clear();
    // neighbors will only be 1 element in this design
    for (size_t i = 0; i < neighbors.n_elem; ++i) {
      int padsIndexRow = neighbors[i];
      string fileName;
      int corpusFrameIndex = dataset(0, padsIndexRow);
      double sampleLocation = dataset(1, padsIndexRow);
      double midiPitch = dataset(2, padsIndexRow);
      double lengthInFrames = dataset(3, padsIndexRow);

      // loading the selection of notes into memory
      for (size_t j = 0; j < corpus.size(); j++) {
        if (corpusFrameIndex >= corpus[j].startFrame &&
            corpusFrameIndex <
                corpus[j].startFrame + corpus[j].lengthInFrames) {
          // add to list
          fileName = corpus[j].filePath;

          notes.emplace_back();
          notes.back().fileName = fileName;
          notes.back().sampleLocation = sampleLocation;
          notes.back().player.load(fileName.c_str());
          notes.back().midiPitch = midiPitch;
          notes.back().lengthInFrames = lengthInFrames;

          break;
        }
      }

      std::cout << "#" << i << ": midi: " << midiPitch << " " << fileName
                << ", " << lengthInFrames << std::endl;
    }

    neighbors.clear();
    distances.clear();
  }
};

class VMVoice : public SynthVoice {
 public:
  VMVoice() {
    mAmpEnv.curve(0);  // make segments lines
    mAmpEnv.sustainPoint(2);

    addSphere(mMesh, 0.5, 30, 30);
    sineScaleFilter.freq(25);
  }

  // Note parameters
  VMVoice& setCloudNote(CloudNote* n, bool noteLoop) {
    player = &n->player;
    player->pos(n->sampleLocation);
    mOsc.freq(diy::mtof(n->midiPitch + 24));

    if (noteLoop) {
      player->min(n->sampleLocation);

      float noteEndFrame =
          (n->lengthInFrames - 1.0) * (4096.0 / 4.0) + n->sampleLocation;
      player->max(noteEndFrame);
    } else {
      player->min(0);
      player->max(player->frames());
    }

    return *this;
  }

  VMVoice& setSineScale(float scale) {
    sineScale = scale;
    return *this;
  }
  // VMVoice& midiNote(int noteToPlay) {
  //// get cloudNote?
  // MidiPitchCluster* c = &midiPitchClusters[noteToPlay - 24];  // hacky
  // CloudNote* n = &c->notes[0];
  // mOsc.freq(diy::mtof(n->midiPitch + 12));
  // cout << "Chosen note: " << n->midiPitch << endl;
  // cout << "file: " << n->fileName << endl;

  // player = &n->player;
  // player->pos(n->sampleLocation);

  // if (noteLoop) {
  // player->min(n->sampleLocation);

  // float noteEndFrame =
  //(n->lengthInFrames - 1.0) * (4096.0 / 4.0) + n->sampleLocation;
  // player->max(noteEndFrame);
  //} else {
  // player->min(0);
  // player->max(player->frames());
  //}

  //// midiPitch = noteToPlay;
  //// mOsc.freq(diy::mtof(cloudNote->midiPitch + 12));
  // return *this;
  //}

  // Audio processing function
  void onProcess(AudioIOData& io) override {
    while (io()) {
      float f = player == nullptr ? 0.0f : player->operator()();

      float out
        = (f * mAmpEnv() * 0.5f) + (mOsc() * mAmpEnv() * sineScaleFilter(sineScale));

      io.out(0) += out;
      io.out(1) += out;
    }

    if (mAmpEnv.done()) free();
  }

  // Graphics processing function
  void onProcess(Graphics& g) override {
    float spatialEnv = mAmpEnv.value();
    g.pushMatrix();
    gl::blending(true);
    gl::blendTrans();
    g.translate(mOsc.freq() / 250 - 3, spatialEnv * 2 - 1, -8);
    g.color(spatialEnv, mOsc.freq() / 1000, spatialEnv, spatialEnv);
    g.draw(mMesh);
    g.popMatrix();
  }

  void onTriggerOn() override { mAmpEnv.reset(); }

  void onTriggerOff() override { mAmpEnv.release(); }

 protected:
  float mAmp{0.4f};
  float mDur{1.5f};
  float sineScale{0.0f};
  Sine<> mOsc;
  Env<3> mAmpEnv{0.f, 0.5f, 1.f, 1.0f, 1.f, 2.0f, 0.f};
  Mesh mMesh;
  samplePlayer* player;
  gam::OnePole<> sineScaleFilter;
};

struct VoiceMementos : App, MIDIMessageHandler {
  PolySynth pSynth;

  RtMidiIn midiIn;
  mutex m;
  bool update{false};
  int noteToPlay{25};
  bool noteLoop = true;

  samplePlayer* mainPlayer;

  samplePlayer* player = nullptr;

  double mainPlayerLengthInFrames;
  double mainPlayerSampleLocation;
  int noteIndex = 0;

  samplePlayer* prevPlayer = nullptr;

  vector<MidiPitchCluster> midiPitchClusters;
  vector<CorpusFile> corpus;
  vector<CloudNote> cloud;

  gam::Sine<> osc;
  gam::OnePole<> rateFilter;

  Parameter loudnessParam{"RMS", "", 0.5, "", 0.0, 1.0};
  Parameter toneParam{"Tone Color", "", 0.5, "", 0.0, 1.0};
  Parameter onsetParam{"Onset", "", 0.5, "", 0.0, 1.0};
  Parameter peakinessParam{"Peakiness", "", 0.5, "", 0.0, 1.0};
  Parameter sineScaleParam{"Pitch Reference Level", "", 0.0, "", 0.0, 1.0};

  ControlGUI gui;

  Mesh mesh;
  Mesh line;

  float minimum{std::numeric_limits<float>::max()};
  float maximum{-std::numeric_limits<float>::max()};

  void onInit() {
    // Check for connected MIDI devices
    if (midiIn.getPortCount() > 0) {
      // Bind ourself to the RtMidiIn object, to have the onMidiMessage()
      // callback called whenever a MIDI message is received
      MIDIMessageHandler::bindTo(midiIn);

      // Open the last device found
      unsigned int port = midiIn.getPortCount() - 1;
      midiIn.openPort(port);
      printf("Opened port to %s\n", midiIn.getPortName(port).c_str());
    } else {
      printf("Error: No MIDI devices found.\n");
    }
  }

  // This gets called whenever a MIDI message is received on the port
  void onMIDIMessage(const MIDIMessage& m) {
    // Here we demonstrate how to parse common channel messages
    int noteNumber = m.noteNumber() - 36;
    switch (m.type()) {
      case MIDIByte::NOTE_ON:

        if (noteNumber < 40 && noteNumber > 23) {
          // float frequency = ::pow(2., (midiNote - 69.) / 12.) * 440.;
          // if (line.vertices().size()) {
          // Vec3f pv(loudnessParam, toneParam, peakinessParam);
          // line.vertices()[0] = pv;
          //}

          MidiPitchCluster* c = &midiPitchClusters[noteNumber - 24];  // hacky
          CloudNote* n = &c->notes[0];
          cout << "Chosen note: " << n->midiPitch << endl;
          cout << "file: " << n->fileName << endl;

          VMVoice* voice = pSynth.getVoice<VMVoice>();
          voice->setCloudNote(n, noteLoop);
          voice->setSineScale(sineScaleParam);
          pSynth.triggerOn(voice, 0, noteNumber);
        }
        break;

      case MIDIByte::NOTE_OFF:
        if (noteNumber < 40 && noteNumber > 23) {
          pSynth.triggerOff(noteNumber);
        }
        // player = nullptr;
        break;

      case MIDIByte::PITCH_BEND:
        break;

      // Control messages need to be parsed again...
      case MIDIByte::CONTROL_CHANGE:
        switch (m.controlNumber()) {
          case MIDIByte::MODULATION:
            loudnessParam =
                loudnessParam + ((int)m.controlValue() == 1 ? -0.04 : 0.04);
            break;
          case 2:
            toneParam = toneParam + ((int)m.controlValue() == 1 ? -0.04 : 0.04);
            break;
          case 3:
            onsetParam =
                onsetParam + ((int)m.controlValue() == 1 ? -0.04 : 0.04);
            break;
          case 4:
            peakinessParam =
                peakinessParam + ((int)m.controlValue() == 1 ? -0.04 : 0.04);
            break;

          case MIDIByte::EXPRESSION:
            break;

          default:
            generateKeyboard();
            break;
        }
        break;
      default:;
    }
  }

  void onCreate() override {
    pSynth.allocatePolyphony<VMVoice>(16);

    std::cout << std::endl << "starting app" << std::endl;
    gui << loudnessParam;
    gui << toneParam;
    //gui << onsetParam;
    gui << peakinessParam;
    gui << sineScaleParam;
    gui.init();
    navControl().useMouse(false);


    mesh.primitive(Mesh::POINTS);
    line.primitive(Mesh::LINES);
    line.vertex(0, 0, 0);
    line.vertex(1, 1, 1);

    std::cout << std::endl << "starting loadmesh" << std::endl;
    std::ifstream meshFile("../notes.csv");
    CSVRow meshRow;
    while (meshFile >> meshRow) {
      Vec3f v(std::stof(meshRow[0]), std::stof(meshRow[1]),
              std::stof(meshRow[3]));
      // map higher dimensions to color space!
      mesh.vertex(v);
    }
    std::cout << std::endl
              << "finished loadmesh: " << mesh.vertices().size() << " points"
              << std::endl;

    std::cout << std::endl << "starting notes" << std::endl;

    for (int i = 24; i < 39; i++) {
      midiPitchClusters.emplace_back();
      midiPitchClusters.back().init(i);
    }

    std::cout << std::endl << "done notes" << std::endl;

    std::cout << std::endl << "starting fileEntries" << std::endl;
    std::ifstream file("../fileEntries.csv");
    CSVRow row;
    while (file >> row) {
      corpus.emplace_back();
      corpus.back().filePath = row[0];
      corpus.back().startFrame = std::stoi(row[1]);
      corpus.back().lengthInFrames = std::stoi(row[2]);
      // TODO: add id3 tag metadata

      std::cout << '.';
    }

    std::cout << std::endl << "✅ Corpus list loaded" << std::endl;

    gam::sampleRate(audioIO().framesPerSecond());
  }

  void onSound(AudioIOData& io) override {
    pSynth.render(io);  // Render audio
    // use a "try lock" in the audio thread because we won't wait
    // when we don't get the lock; we just do nothing and move on.
    // we'll do better next time.
    //
  }

  bool generateKeyboard() {
    arma::mat query = {{loudnessParam, toneParam, peakinessParam}};

    player = nullptr;  // protect memory access in onSound

    for (int i = 0; i < midiPitchClusters.size(); i++) {
      midiPitchClusters[i].query(query);
      midiPitchClusters[i].loadNote(corpus);
    }

    cout << "finished querying all notes" << endl;
  }

  bool onKeyDown(const Keyboard& k) override {
    if (k.key() == 'g') {
      generateKeyboard();
      return true;
    }
    if (k.key() == 'r') {
      noteLoop = !noteLoop;
      update = true;
      return true;
    }

    int noteNumber = k.key() - 48 + 23;
    std::cout << "note number: " << noteNumber << std::endl;

    if (noteNumber < 40 && noteNumber > 23) {
      // float frequency = ::pow(2., (midiNote - 69.) / 12.) * 440.;
      // if (line.vertices().size()) {
      // Vec3f pv(loudnessParam, toneParam, peakinessParam);
      // line.vertices()[0] = pv;
      //}

      MidiPitchCluster* c = &midiPitchClusters[noteNumber - 24];  // hacky
      CloudNote* n = &c->notes[0];
      cout << "Chosen note: " << n->midiPitch << endl;
      cout << "file: " << n->fileName << endl;

      VMVoice* voice = pSynth.getVoice<VMVoice>();
      voice->setCloudNote(n, noteLoop);
      pSynth.triggerOn(voice, 0, noteNumber);
    }

    return true;
  }

  bool onKeyUp(Keyboard const& k) override {
    int noteNumber = k.key() - 48 + 23;
    if (noteNumber < 40 && noteNumber > 23) {
      pSynth.triggerOff(noteNumber);
    }
    return true;
  }

  void onDraw(Graphics& g) override {
    g.clear();
    //pSynth.render(g);
    g.draw(mesh);
    gui.draw(g);
  }
};

int main() {
  // AudioDevice aggregate = AudioDevice("Apple Inc.: ptk-media-call");
  AudioDevice aggregate = AudioDevice("Apple Inc.: speaks-bh");
  aggregate.print();

  VoiceMementos app;

  app.audioDomain()->configure(aggregate, 44100, 1024,
                               aggregate.channelsOutMax(),
                               aggregate.channelsInMax());
  app.audioDomain()->audioIO().print();
  app.start();
}

