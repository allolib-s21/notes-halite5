#include <cstdio> // for printing to stdout

#include "Gamma/Analysis.h"
#include "Gamma/Effects.h"
#include "Gamma/Envelope.h"
#include "Gamma/Oscillator.h"

#include "al/app/al_App.hpp"
#include "al/graphics/al_Shapes.hpp"
#include "al/scene/al_PolySynth.hpp"
#include "al/scene/al_SynthSequencer.hpp"
#include "al/ui/al_ControlGUI.hpp"
#include "al/ui/al_Parameter.hpp"

#include <cassert>
#include <vector>
#include <cmath>
#include "notes.h"

// using namespace gam;
using namespace al;

enum Instrument
{
    INSTR_MSCHORDS,
    INSTR_KPS,
    INSTR_MSBASS,
    INSTR_FM,
    NUM_INSTRUMENTS
};

static const float SEMITONE_RATIO = 1.0594630943592952646;
static const float CENT_RATIO = 1.0005777895065548;

// https://en.wikipedia.org/wiki/Equal_temperament#General_formulas_for_the_equal-tempered_interval
float note_freq(uint16_t note) { return 440 * std::pow(SEMITONE_RATIO, note - 0x45); }

float detune(float freq, int cents) { return freq * std::pow(CENT_RATIO, cents); }

class TimeSignature
{
private:
    int upper;
    int lower;

public:
    TimeSignature()
    {
        this->upper = 7;
        this->lower = 4;
    }
};

class Note
{
private:
    float freq;
    float time;
    float duration;
    float amp;
    float attack;
    float decay;

public:
    Note()
    {
        this->freq = 440.0;
        this->time = 0;
        this->duration = 0.5;
        this->amp = 0.2;
        this->attack = 0.05;
        this->decay = 0.05;
    }
    Note(float freq,
         float time = 0.0f,
         float duration = 0.5f,
         float amp = 0.2f,
         float attack = 0.05f,
         float decay = 0.05f)
    {
        this->freq = freq;
        this->time = time;
        this->duration = duration;
        this->amp = amp;
        this->attack = attack;
        this->decay = decay;
    }
    // Return an identical note, but offset by the
    // number of beats indicated by beatOffset,
    // and with amplitude multiplied by ampMult
    Note(const Note &n, float beatOffset, float ampMult = 1.0f)
    {
        this->freq = n.freq;
        this->time = n.time + beatOffset;
        this->duration = n.duration;
        this->amp = n.amp * ampMult;
        this->attack = n.attack;
        this->decay = n.decay;
    }
    Note(const Note &n)
    {
        this->freq = n.freq;
        this->time = n.time;
        this->duration = n.duration;
        this->amp = n.amp;
        this->attack = n.attack;
        this->decay = n.decay;
    }
    float getFreq() { return this->freq; }
    float getTime() { return this->time; }
    float getDuration() { return this->duration; }
    float getAmp() { return this->amp; }
    float getAttack() { return this->attack; }
    float getDecay() { return this->decay; }
};

class Sequence
{
private:
    TimeSignature ts;
    std::vector<Note> notes;

public:
    Sequence(TimeSignature ts)
    {
        this->ts = ts;
    }

    void add(Note n)
    {
        notes.push_back(n);
    }

    // Add notes from the source sequence s,
    // but starting on the beat indicated by startBeat

    void addSequence(Sequence *s, float startBeat, float ampMult = 1.0)
    {
        for (auto &note : *(s->getNotes()))
            add(Note(note, startBeat, ampMult));
    }

    std::vector<Note> *getNotes()
    {
        return &notes;
    }
};

class KPSWaves : public SynthVoice
{
public:
    // Unit generators
    float mNoiseMix;
    gam::Pan<> mPan;
    gam::ADSR<> mAmpEnv;
    gam::ADSR<> mFiltEnv;
    gam::EnvFollow<> mEnvFollow; // envelope follower to connect audio output to graphics
    gam::Saw<> mOsc0;
    gam::DWO<> mOsc1;
    gam::NoiseWhite<> mNoise;
    gam::Biquad<> mFilter;
    gam::Comb<> mComb;

    // Additional members
    Mesh mMesh;

    // Initialize voice. This function will nly be called once per voice
    void init() override
    {
        mAmpEnv.curve(0);               // linear segments
        mAmpEnv.levels(0, 1.0, 1.0, 0); // These tables are not normalized, so scale to 0.3
        mAmpEnv.sustainPoint(2);        // Make point 2 sustain until a release is issued

        mFiltEnv.curve(0);
        mFiltEnv.levels(0, 1.0, 1.0, 0);
        mFiltEnv.sustainPoint(2);

        // mComb.ipolType(ipl::ROUND);

        // We have the mesh be a sphere
        addDisc(mMesh, 1.0, 30);

        createInternalTriggerParameter("amplitude", 0.3, 0.0, 1.0);
        createInternalTriggerParameter("frequency", 60, 20, 5000);
        createInternalTriggerParameter("oscMix", 0.5, 0.0, 1.0);
        createInternalTriggerParameter("noise", 0.0, 0.0, 1.0);
        createInternalTriggerParameter("ampEnvAtk", 0.1, 0.01, 2.0);
        createInternalTriggerParameter("ampEnvDec", 0.01, 0.01, 2.0);
        createInternalTriggerParameter("ampEnvSus", 0.8, 0.0, 1.0);
        createInternalTriggerParameter("ampEnvRel", 0.4, 0.05, 2.0);
        createInternalTriggerParameter("ampEnvCve", 4.0, -10.0, 10.0);
        createInternalTriggerParameter("filtEnvAtk", 0.1, 0.01, 2.0);
        createInternalTriggerParameter("filtEnvDec", 0.01, 0.01, 2.0);
        createInternalTriggerParameter("filtEnvSus", 0.8, 0.0, 1.0);
        createInternalTriggerParameter("filtEnvRel", 0.4, 0.05, 2.0);
        createInternalTriggerParameter("filtEnvCve", 4.0, -10.0, 10.0);
        createInternalTriggerParameter("filtEnvDpth", 0.0, -400, 4800);
        createInternalTriggerParameter("filtFreq", 2400.0, 10.0, 5000);
        createInternalTriggerParameter("filtRes", 0.1, 0.01, 10);
        createInternalTriggerParameter("combDel", 0.002268, 0.001, 1.0);
        createInternalTriggerParameter("combFbk", 0.5, -1.0, 1.0);
        createInternalTriggerParameter("combFfw", 0.0, -1.0, 1.0);
        createInternalTriggerParameter("combDec", 0.0, 0.001, 1.0);
        createInternalTriggerParameter("pan", 0.0, -1.0, 1.0);
    }

    //

    virtual void onProcess(AudioIOData &io) override
    {
        updateFromParameters();
        float amp = getInternalParameterValue("amplitude");
        float filtFreq = getInternalParameterValue("filtFreq");
        float filtEnvDepth = getInternalParameterValue("filtEnvDpth");
        float oscMix = getInternalParameterValue("oscMix");
        float noiseMix = getInternalParameterValue("noise");
        float noteFreq = getInternalParameterValue("frequency");
        
        while (io())
        {
            // mix oscillator with noise
            float osc0 = mOsc0();
            float osc1 = mOsc1.sqr();
            float mainOscMix = osc0 * (1 - oscMix) + osc1 * (oscMix);
            float noiseSamp = mNoise() * noiseMix;
            float s1 = mainOscMix * (1 - noiseMix) + noiseSamp;

            // apply main filter
            mFilter.freq(filtFreq + (mFiltEnv() * filtEnvDepth));
            s1 = mFilter(s1);
            mComb.delay((44100.0 / noteFreq) / 44100.0);
            s1 = mComb(s1);

            // apply amplitude envelope
            s1 *= mAmpEnv() * amp;

            float s2;
            mPan(s1, s1, s2);
            io.out(0) += s1;
            io.out(1) += s2;
        }

        if (mAmpEnv.done() && (mEnvFollow.value() < 0.001f))
            free();
    }

    virtual void onProcess(Graphics &g)
    {
        float frequency = getInternalParameterValue("frequency");
        float amplitude = getInternalParameterValue("amplitude");
        g.pushMatrix();
        g.translate(amplitude, amplitude, -4);
        //g.scale(frequency/2000, frequency/4000, 1);
        float scaling = 0.1;
        g.scale(scaling * frequency / 200, scaling * frequency / 400, scaling * 1);
        g.color(mEnvFollow.value(), frequency / 1000, mEnvFollow.value() * 10, 0.4);
        g.draw(mMesh);
        g.popMatrix();
    }
    virtual void onTriggerOn() override
    {
        updateFromParameters();
        mAmpEnv.reset();
        mFiltEnv.reset();
    }

    virtual void onTriggerOff() override
    {
        mAmpEnv.triggerRelease();
        mFiltEnv.triggerRelease();
    }

    void updateFromParameters()
    {
        mOsc0.freq(getInternalParameterValue("frequency"));
        mOsc1.freq(getInternalParameterValue("frequency"));

        mAmpEnv.attack(getInternalParameterValue("ampEnvAtk"));
        mAmpEnv.decay(getInternalParameterValue("ampEnvDec"));
        mAmpEnv.sustain(getInternalParameterValue("ampEnvSus"));
        mAmpEnv.release(getInternalParameterValue("ampEnvRel"));
        mAmpEnv.curve(getInternalParameterValue("ampEnvCve"));

        mPan.pos(getInternalParameterValue("pan"));

        mFilter.freq(getInternalParameterValue("filtFreq"));
        mFilter.res(getInternalParameterValue("filtRes"));

        mFiltEnv.attack(getInternalParameterValue("filtEnvAtk"));
        mFiltEnv.decay(getInternalParameterValue("filtEnvDec"));
        mFiltEnv.sustain(getInternalParameterValue("filtEnvSus"));
        mFiltEnv.release(getInternalParameterValue("filtEnvRel"));
        mFiltEnv.curve(getInternalParameterValue("filtEnvCve"));

        mComb.maxDelay(getInternalParameterValue("combDel") * 1.1);
        mComb.delay(getInternalParameterValue("combDel"));
        mComb.ffd(getInternalParameterValue("combFfw"));
        mComb.fbk(getInternalParameterValue("combFbk"));
        mComb.decay(getInternalParameterValue("combDec"));
    }
};


class FM : public SynthVoice
{
public:
    // Unit generators
    gam::Pan<> mPan;
    gam::ADSR<> mAmpEnv;
    gam::ADSR<> mModEnv;
    gam::EnvFollow<> mEnvFollow;

    gam::Sine<> car, mod; // carrier, modulator sine oscillators

    // Additional members
    Mesh mMesh;

    void init() override
    {
        //      mAmpEnv.curve(0); // linear segments
        mAmpEnv.levels(0, 1, 1, 0);

        // We have the mesh be a sphere
        addDisc(mMesh, 1.0, 30);

        createInternalTriggerParameter("amplitude", 0.5, 0.0, 1.0);
        createInternalTriggerParameter("freq", 440, 10, 4000.0);
        createInternalTriggerParameter("attackTime", 0.1, 0.01, 3.0);
        createInternalTriggerParameter("releaseTime", 0.1, 0.1, 10.0);
        createInternalTriggerParameter("pan", 0.0, -1.0, 1.0);

        // FM index
        createInternalTriggerParameter("idx1", 0.01, 0.0, 10.0);
        createInternalTriggerParameter("idx2", 7, 0.0, 10.0);
        createInternalTriggerParameter("idx3", 5, 0.0, 10.0);

        createInternalTriggerParameter("carMul", 1, 0.0, 20.0);
        createInternalTriggerParameter("modMul", 1.0007, 0.0, 20.0);
        createInternalTriggerParameter("sustain", 0.75, 0.1, 1.0); // Unused
    }

    //
    void onProcess(AudioIOData &io) override
    {
        float modFreq =
            getInternalParameterValue("freq") * getInternalParameterValue("modMul");
        mod.freq(modFreq);
        float carBaseFreq =
            getInternalParameterValue("freq") * getInternalParameterValue("carMul");
        float modScale =
            getInternalParameterValue("freq") * getInternalParameterValue("modMul");
        float amp = getInternalParameterValue("amplitude");
        while (io())
        {
            car.freq(carBaseFreq + mod() * mModEnv() * modScale);
            float s1 = car() * mAmpEnv() * amp;
            float s2;
            mEnvFollow(s1);
            mPan(s1, s1, s2);
            io.out(0) += s1;
            io.out(1) += s2;
        }
        if (mAmpEnv.done() && (mEnvFollow.value() < 0.001))
            free();
    }

    void onProcess(Graphics &g) override
    {
        g.pushMatrix();
        g.translate(getInternalParameterValue("freq") / 300 - 2,
                    getInternalParameterValue("modAmt") / 25 - 1, -4);
        float scaling = getInternalParameterValue("amplitude") * 1;
        g.scale(scaling, scaling, scaling * 1);
        g.color(HSV(getInternalParameterValue("modMul") / 20, 1,
                    mEnvFollow.value() * 10));
        g.draw(mMesh);
        g.popMatrix();
    }

    void onTriggerOn() override
    {
        mModEnv.levels()[0] = getInternalParameterValue("idx1");
        mModEnv.levels()[1] = getInternalParameterValue("idx2");
        mModEnv.levels()[2] = getInternalParameterValue("idx2");
        mModEnv.levels()[3] = getInternalParameterValue("idx3");

        mAmpEnv.lengths()[0] = getInternalParameterValue("attackTime");
        mModEnv.lengths()[0] = getInternalParameterValue("attackTime");

        mAmpEnv.lengths()[1] = 0.001;
        mModEnv.lengths()[1] = 0.001;

        mAmpEnv.lengths()[2] = getInternalParameterValue("releaseTime");
        mModEnv.lengths()[2] = getInternalParameterValue("releaseTime");
        mPan.pos(getInternalParameterValue("pan"));

        //        mModEnv.lengths()[1] = mAmpEnv.lengths()[1];

        mAmpEnv.reset();
        mModEnv.reset();
    }
    void onTriggerOff() override
    {
        mAmpEnv.triggerRelease();
        mModEnv.triggerRelease();
    }
};

class MiniSubWaves : public SynthVoice
{
public:
    // Unit generators
    float mNoiseMix;
    gam::Pan<> mPan;
    gam::ADSR<> mAmpEnv;
    gam::ADSR<> mFiltEnv;
    gam::EnvFollow<> mEnvFollow; // envelope follower to connect audio output to graphics
    gam::Saw<> mOsc0;
    gam::DWO<> mOsc1;
    gam::NoiseWhite<> mNoise;
    gam::Biquad<> mFilter;
    // Additional members
    Mesh mMesh;

    // Initialize voice. This function will nly be called once per voice
    void init() override
    {
        mAmpEnv.curve(0);               // linear segments
        mAmpEnv.levels(0, 1.0, 1.0, 0); // These tables are not normalized, so scale to 0.3
        mAmpEnv.sustainPoint(2);        // Make point 2 sustain until a release is issued

        mFiltEnv.curve(0);
        mFiltEnv.levels(0, 1.0, 1.0, 0);
        mFiltEnv.sustainPoint(2);

        // We have the mesh be a sphere
        addDisc(mMesh, 1.0, 30);

        createInternalTriggerParameter("amplitude", 0.3, 0.0, 1.0);
        createInternalTriggerParameter("frequency", 60, 20, 5000);
        createInternalTriggerParameter("oscMix", 0.5, 0.0, 1.0);
        createInternalTriggerParameter("noise", 0.0, 0.0, 1.0);
        createInternalTriggerParameter("ampEnvAtk", 0.1, 0.01, 2.0);
        createInternalTriggerParameter("ampEnvDec", 0.01, 0.01, 2.0);
        createInternalTriggerParameter("ampEnvSus", 0.8, 0.0, 1.0);
        createInternalTriggerParameter("ampEnvRel", 0.4, 0.05, 2.0);
        createInternalTriggerParameter("ampEnvCve", 4.0, -10.0, 10.0);
        createInternalTriggerParameter("filtEnvAtk", 0.1, 0.01, 2.0);
        createInternalTriggerParameter("filtEnvDec", 0.01, 0.01, 2.0);
        createInternalTriggerParameter("filtEnvSus", 0.8, 0.0, 1.0);
        createInternalTriggerParameter("filtEnvRel", 0.4, 0.05, 2.0);
        createInternalTriggerParameter("filtEnvCve", 4.0, -10.0, 10.0);
        createInternalTriggerParameter("filtEnvDpth", 0.0, -400, 4800);
        createInternalTriggerParameter("filtFreq", 2400.0, 10.0, 5000);
        createInternalTriggerParameter("filtRes", 0.1, 0.01, 10);
        createInternalTriggerParameter("pan", 0.0, -1.0, 1.0);
    }

    //

    virtual void onProcess(AudioIOData &io) override
    {
        updateFromParameters();
        float amp = getInternalParameterValue("amplitude");
        float filtFreq = getInternalParameterValue("filtFreq");
        float filtEnvDepth = getInternalParameterValue("filtEnvDpth");
        float oscMix = getInternalParameterValue("oscMix");
        float noiseMix = getInternalParameterValue("noise");
        while (io())
        {
            // mix oscillator with noise
            float osc0 = mOsc0();
            float osc1 = mOsc1.sqr();
            float mainOscMix = osc0 * (1 - oscMix) + osc1 * (oscMix);
            float noiseSamp = mNoise() * noiseMix;
            float s1 = mainOscMix * (1 - noiseMix) + noiseSamp;

            // apply main filter
            mFilter.freq(filtFreq + (mFiltEnv() * filtEnvDepth));
            s1 = mFilter(s1);

            // apply amplitude envelope
            s1 *= mAmpEnv() * amp;

            float s2;
            mPan(s1, s1, s2);
            io.out(0) += s1;
            io.out(1) += s2;
        }

        if (mAmpEnv.done() && (mEnvFollow.value() < 0.001f))
            free();
    }

    virtual void onProcess(Graphics &g)
    {
        float frequency = getInternalParameterValue("frequency");
        float amplitude = getInternalParameterValue("amplitude");
        g.pushMatrix();
        g.translate(amplitude, amplitude, -4);
        //g.scale(frequency/2000, frequency/4000, 1);
        float scaling = 0.1;
        g.scale(scaling * frequency / 200, scaling * frequency / 400, scaling * 1);
        g.color(mEnvFollow.value(), frequency / 1000, mEnvFollow.value() * 10, 0.4);
        g.draw(mMesh);
        g.popMatrix();
    }
    virtual void onTriggerOn() override
    {
        updateFromParameters();
        mAmpEnv.reset();
        mFiltEnv.reset();
    }

    virtual void onTriggerOff() override
    {
        mAmpEnv.triggerRelease();
        mFiltEnv.triggerRelease();
    }

    void updateFromParameters()
    {
        mOsc0.freq(getInternalParameterValue("frequency"));
        mOsc1.freq(getInternalParameterValue("frequency"));

        mAmpEnv.attack(getInternalParameterValue("ampEnvAtk"));
        mAmpEnv.decay(getInternalParameterValue("ampEnvDec"));
        mAmpEnv.sustain(getInternalParameterValue("ampEnvSus"));
        mAmpEnv.release(getInternalParameterValue("ampEnvRel"));
        mAmpEnv.curve(getInternalParameterValue("ampEnvCve"));

        mPan.pos(getInternalParameterValue("pan"));

        mFilter.freq(getInternalParameterValue("filtFreq"));
        mFilter.res(getInternalParameterValue("filtRes"));

        mFiltEnv.attack(getInternalParameterValue("filtEnvAtk"));
        mFiltEnv.decay(getInternalParameterValue("filtEnvDec"));
        mFiltEnv.sustain(getInternalParameterValue("filtEnvSus"));
        mFiltEnv.release(getInternalParameterValue("filtEnvRel"));
        mFiltEnv.curve(getInternalParameterValue("filtEnvCve"));
    }
};

// We make an app.
class MyApp : public App
{
public:
    // GUI manager for MiniSubWaves voices
    // The name provided determines the name of the directory
    // where the presets and sequences are stored
    SynthGUIManager<MiniSubWaves> synthManager{"MiniSubWaves"};

    // This function is called right after the window is created
    // It provides a grphics context to initialize ParameterGUI
    // It's also a good place to put things that should
    // happen once at startup.
    void onCreate() override
    {
        navControl().active(false); // Disable navigation via keyboard, since we
                                    // will be using keyboard for note triggering

        // Set sampling rate for Gamma objects from app's audio
        gam::sampleRate(audioIO().framesPerSecond());

        imguiInit();

        // give me hidpi scaling
        ImGuiIO &io = ImGui::GetIO();
        (void)io;
        ImGui::GetStyle().ScaleAllSizes(2);
        io.FontAllowUserScaling = true;
        io.FontGlobalScale = 2;

        // Play example sequence. Comment this line to start from scratch
        // synthManager.synthSequencer().playSequence("synth1.synthSequence");
        synthManager.synthRecorder().verbose(true);
    }

    // The audio callback function. Called when audio hardware requires data
    void onSound(AudioIOData &io) override
    {
        synthManager.render(io); // Render audio
    }

    void onAnimate(double dt) override
    {
        // The GUI is prepared here
        imguiBeginFrame();
        // Draw a window that contains the synth control panel
        synthManager.drawSynthControlPanel();
        imguiEndFrame();
    }

    // The graphics callback function.
    void onDraw(Graphics &g) override
    {
        g.clear();
        // Render the synth's graphics
        synthManager.render(g);

        // GUI is drawn here
        imguiDraw();
    }

    // Whenever a key is pressed, this function is called
    bool onKeyDown(Keyboard const &k) override
    {
        if (ParameterGUI::usingKeyboard())
        { // Ignore keys if GUI is using
            // keyboard
            return true;
        }

        switch (k.key())
        {
        case '1':
            std::cout << "1 pressed!" << std::endl;
            playSongGH(1.0, 60);
            return false;
        }

        // case '2':
        //     std::cout << "2 pressed!" << std::endl;
        //     playSongGH(1.0, 60);
        //     return false;

        // case '3':
        //     std::cout << "3 pressed!" << std::endl;
        //     playSongGH(1.0, 240);
        //     return false;

        // case '4':
        //     std::cout << "4 pressed!" << std::endl;
        //     playSongGH(1.0, 30);
        //     return false;

        // case 'q':
        //     std::cout << "q pressed!" << std::endl;
        //     playSongGH(pow(2.0, 7.0 / 12.0), 120);
        //     return false;

        // case 'w':
        //     std::cout << "w pressed!" << std::endl;
        //     playSongGH(pow(2.0, 7.0 / 12.0), 60);
        //     return false;

        // case 'e':
        //     std::cout << "e pressed!" << std::endl;
        //     playSongGH(pow(2.0, 7.0 / 12.0), 240);
        //     return false;

        // case 'r':
        //     std::cout << "r pressed!" << std::endl;
        //     playSongGH(pow(2.0, 7.0 / 12.0), 30);
        //     return false;

        // case 'a':
        //     std::cout << "q pressed!" << std::endl;
        //     playSongGH(1.0, 120, INSTR_FM);
        //     return false;

        // case 's':
        //     std::cout << "w pressed!" << std::endl;
        //     playSongGH(1.0, 60, INSTR_FM);
        //     return false;

        // case 'd':
        //     std::cout << "e pressed!" << std::endl;
        //     playSongGH(1.0, 240, INSTR_FM);
        //     return false;

        // case 'f':
        //     std::cout << "r pressed!" << std::endl;
        //     playSongGH(1.0, 30, INSTR_FM);
        //     return false;
        // }

        return true;
    }

    // Whenever a key is released this function is called
    bool
    onKeyUp(Keyboard const &k) override
    {
        // int midiNote = asciiToMIDI(k.key());
        // if (midiNote > 0)
        // {
        //   synthManager.triggerOff(midiNote);
        // }
        // return true;
        return true;
    }

    void onExit() override { imguiShutdown(); }

    // New code: a function to play a note A

    void playNote(float freq, float time, float duration = 0.5, float amp = 0.2, float attack = 0.1, float decay = 0.1, Instrument instrument = INSTR_MSCHORDS)
    {
        SynthVoice *voice;
        switch (instrument)
        {case INSTR_MSCHORDS:
            voice = synthManager.synth().getVoice<MiniSubWaves>();


            voice->setInternalParameterValue("amplitude", amp);
            voice->setInternalParameterValue("oscMix", 0.1);
            voice->setInternalParameterValue("frequency", freq);

            voice->setInternalParameterValue("ampEnvAtk", 0.1);
            voice->setInternalParameterValue("ampEnvDec", 0.3);
            voice->setInternalParameterValue("ampEnvSus", 0.6);
            voice->setInternalParameterValue("ampEnvRel", 0.2);

            voice->setInternalParameterValue("filtEnvAtk", 0.25);
            voice->setInternalParameterValue("filtEnvDec", 0.2);
            voice->setInternalParameterValue("filtEnvSus", 0.05);
            voice->setInternalParameterValue("filtEnvRel", 0.1);
            voice->setInternalParameterValue("filtEnvDpth", 1800);
            voice->setInternalParameterValue("filtEnvCve", 1.0);
            voice->setInternalParameterValue("filtFreq", 1150);
            voice->setInternalParameterValue("filtRes", 1.0);

            voice->setInternalParameterValue("pan", 0);
            break;

        case INSTR_KPS:
            voice = synthManager.synth().getVoice<KPSWaves>();


            voice->setInternalParameterValue("amplitude", amp);
            voice->setInternalParameterValue("oscMix", 0.23);
            voice->setInternalParameterValue("noise", 0.996);
            voice->setInternalParameterValue("frequency", freq);
            voice->setInternalParameterValue("ampEnvAtk", 0.315);
            voice->setInternalParameterValue("ampEnvDec", 1.342);
            voice->setInternalParameterValue("ampEnvSus", 0.651);
            voice->setInternalParameterValue("ampEnvRel", 0.4);

            voice->setInternalParameterValue("filtEnvAtk", 0.233);
            voice->setInternalParameterValue("filtEnvDec", 0.849);
            voice->setInternalParameterValue("filtEnvSus", 0.798);
            voice->setInternalParameterValue("filtEnvRel", 0.261);
            voice->setInternalParameterValue("filtEnvDpth", 1032);
            voice->setInternalParameterValue("filtEnvCve", 4.0);
            voice->setInternalParameterValue("filtFreq", 926);
            voice->setInternalParameterValue("filtRes", 0.1);

            voice->setInternalParameterValue("combDec", 0.849);
            voice->setInternalParameterValue("combDel", 0.002268);
            voice->setInternalParameterValue("combFbk", 0.314);
            voice->setInternalParameterValue("combFfw", 0.135);

            voice->setInternalParameterValue("pan", 0);
            break;
        

        case INSTR_MSBASS:
            voice = synthManager.synth().getVoice<MiniSubWaves>();

            voice->setInternalParameterValue("amplitude", amp);
            voice->setInternalParameterValue("oscMix", 0.7);
            voice->setInternalParameterValue("frequency", freq / 2);

            voice->setInternalParameterValue("ampEnvAtk", 0.01);
            voice->setInternalParameterValue("ampEnvDec", 0.35);
            voice->setInternalParameterValue("ampEnvSus", 0.6);
            voice->setInternalParameterValue("ampEnvRel", 0.2);

            voice->setInternalParameterValue("filtEnvAtk", 0.05);
            voice->setInternalParameterValue("filtEnvDec", 0.25);
            voice->setInternalParameterValue("filtEnvSus", 0.0);
            voice->setInternalParameterValue("filtEnvRel", 0.1);
            voice->setInternalParameterValue("filtEnvDpth", 1000);
            voice->setInternalParameterValue("filtEnvCve", -1.0);
            voice->setInternalParameterValue("filtFreq", 940);
            voice->setInternalParameterValue("filtRes", 2.6);

            break;


        case INSTR_FM:
            voice = synthManager.synth().getVoice<FM>();

            voice->setInternalParameterValue("amplitude", amp);
            voice->setInternalParameterValue("freq", freq);
            voice->setInternalParameterValue("attackTime", 0.1);
            voice->setInternalParameterValue("releaseTime", 0.1);
            voice->setInternalParameterValue("pan", 1.0);

            break;
        default:
            voice = nullptr;
            break;
        }
        synthManager.synthSequencer().addVoiceFromNow(voice, time, duration);
    }

    Sequence *sequenceGH_Chords(float offset = 1.0)
    {
        TimeSignature t;
        Sequence *result = new Sequence(t);

        result->addSequence(sequenceGH_ChordsPhrase1(), 0);
        result->addSequence(sequenceGH_ChordsPhrase1(), 3.5);

        return result;
    }

    Sequence *sequenceGH_Bass(float offset = 1.0)
    {
        TimeSignature t;
        Sequence *result = new Sequence(t);

        result->addSequence(sequenceGH_BassPhrase1(), 0);
        result->addSequence(sequenceGH_BassPhrase1(), 3.5);

        return result;
    }

    Sequence *sequenceGH_ChordsPhrase1(float offset = 1.0)
    {
        TimeSignature t;
        Sequence *result = new Sequence(t);

        std::cout << "bean" << note_freq(A4) << std::endl;

        result->add(Note(note_freq(Fs4), 0, 0.5, 0.3));
        result->add(Note(note_freq(B4), 0, 0.5, 0.3));

        result->add(Note(note_freq(B4), 0.5, 0.5, 0.3));
        result->add(Note(note_freq(Fs5), 0.5, 0.5, 0.3));

        result->add(Note(note_freq(Fs5), 1, 0.5, 0.3));
        result->add(Note(note_freq(Gs5), 1, 0.5, 0.3));

        result->add(Note(note_freq(Gs5), 1.5, 0.5, 0.3));
        result->add(Note(note_freq(Ds6), 1.5, 0.5, 0.3));

        result->add(Note(note_freq(Gs5), 2, 0.5, 0.3));
        result->add(Note(note_freq(Cs5), 2, 0.5, 0.3));

        return result;
    }

    Sequence *sequenceGH_BassPhrase1(float offset = 1.0)
    {
        TimeSignature t;
        Sequence *result = new Sequence(t);

        result->add(Note(note_freq(E3), 0, 0.5, 0.3));
        result->add(Note(note_freq(B3), 0.5, 0.5, 0.3));
        result->add(Note(note_freq(Gs3), 1, 1.0, 0.3));

        result->add(Note(note_freq(Ds4), 2.5, 0.5, 0.3));
        result->add(Note(note_freq(B3), 3, 0.5, 0.3));

        return result;
    }

    Sequence *sequenceGHPhrase2(float offset = 1.0)
    {
        TimeSignature t;
        Sequence *result = new Sequence(t);

        result->add(Note(E4 * offset, 0, 0.5, 0.1));
        result->add(Note(F4 * offset, 1, 0.5, 0.2));
        result->add(Note(G4 * offset, 2, 1.0, 0.3));

        return result;
    }

    Sequence *sequenceGHPhrase3(float offset = 1.0)
    {
        TimeSignature t;
        Sequence *result = new Sequence(t);

        result->add(Note(G4 * offset, 0, 0.25, 0.2));
        result->add(Note(A4 * offset, 0.5, 0.25, 0.3));
        result->add(Note(G4 * offset, 1, 0.25, 0.4));
        result->add(Note(F4 * offset, 1.5, 0.25, 0.45));
        result->add(Note(E4 * offset, 2, 0.5, 0.5));
        result->add(Note(C4 * offset, 3, 0.5, 0.25));

        return result;
    }

    Sequence *sequenceGHPhrase4(float offset = 1.0)
    {
        TimeSignature t;
        Sequence *result = new Sequence(t);

        result->add(Note(C4 * offset, 0, 0.5, 0.2));
        result->add(Note(G3 * offset, 1, 0.5, 0.1));
        result->add(Note(C4 * offset, 2, 1.0, 0.05));

        return result;
    }

    // bpm is beats per minute

    void playSequence(Sequence *s, float bpm, Instrument instrument = INSTR_MSCHORDS)
    {
        float secondsPerBeat = 60.0f / bpm;

        std::vector<Note> *notes = s->getNotes();

        for (auto &note : *notes)
        {
            playNote(
                note.getFreq(),
                note.getTime() * secondsPerBeat,
                note.getDuration() * secondsPerBeat,
                note.getAmp(),
                note.getAttack(),
                note.getDecay(),
                instrument);
        }
    }

    void playSongGH(float offset = 1.0, float bpm = 60.0)
    {
        std::cout << "playSongGH: offset=" << offset << " bpm=" << bpm << std::endl;

        playSequence(sequenceGH_Chords(), bpm, INSTR_KPS);

        playSequence(sequenceGH_Bass(), bpm, INSTR_MSBASS);
    }
};

int main()
{
    // Create app instance
    MyApp app;

    // Set up audio
    app.configureAudio(48000., 512, 2, 0);

    app.start();

    return 0;
}
