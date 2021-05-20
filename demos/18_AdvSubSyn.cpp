#include <cstdio> // for printing to stdout

#include "Gamma/Analysis.h"
#include "Gamma/Effects.h"
#include "Gamma/Envelope.h"
#include "Gamma/Gamma.h"
#include "Gamma/Oscillator.h"
#include "Gamma/Types.h"
#include "al/app/al_App.hpp"
#include "al/graphics/al_Shapes.hpp"
#include "al/scene/al_PolySynth.hpp"
#include "al/scene/al_SynthSequencer.hpp"
#include "al/ui/al_ControlGUI.hpp"
#include "al/ui/al_Parameter.hpp"

using namespace gam;
using namespace al;
using namespace std;
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

class MyApp : public App
{
public:
    SynthGUIManager<MiniSubWaves> synthManager{"MiniSubWaves"};
    //    ParameterMIDI parameterMIDI;

    virtual void onInit() override
    {
        imguiInit();
        navControl().active(false); // Disable navigation via keyboard, since we
                                    // will be using keyboard for note triggering
        // Set sampling rate for Gamma objects from app's audio
        gam::sampleRate(audioIO().framesPerSecond());

        // give me hidpi scaling
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        ImGui::GetStyle().ScaleAllSizes(2);
        io.FontAllowUserScaling = true;
        io.FontGlobalScale = 2;
    }

    void onCreate() override
    {
        // Play example sequence. Comment this line to start from scratch
        //    synthManager.synthSequencer().playSequence("synth8.synthSequence");
        synthManager.synthRecorder().verbose(true);
    }

    void onSound(AudioIOData &io) override
    {
        synthManager.render(io); // Render audio
    }

    void onAnimate(double dt) override
    {
        imguiBeginFrame();
        synthManager.drawSynthControlPanel();
        imguiEndFrame();
    }

    void onDraw(Graphics &g) override
    {
        g.clear();
        synthManager.render(g);

        // Draw GUI
        imguiDraw();
    }

    bool onKeyDown(Keyboard const &k) override
    {
        if (ParameterGUI::usingKeyboard())
        { // Ignore keys if GUI is using them
            return true;
        }
        if (k.shift())
        {
            // If shift pressed then keyboard sets preset
            int presetNumber = asciiToIndex(k.key());
            synthManager.recallPreset(presetNumber);
        }
        else
        {
            // Otherwise trigger note for polyphonic synth
            int midiNote = asciiToMIDI(k.key());
            if (midiNote > 0)
            {
                synthManager.voice()->setInternalParameterValue(
                    "frequency", ::pow(2.f, (midiNote - 69.f) / 12.f) * 432.f);
                synthManager.triggerOn(midiNote);
            }
        }
        return true;
    }

    bool onKeyUp(Keyboard const &k) override
    {
        int midiNote = asciiToMIDI(k.key());
        if (midiNote > 0)
        {
            synthManager.triggerOff(midiNote);
        }
        return true;
    }

    void onExit() override { imguiShutdown(); }
};

int main()
{
    MyApp app;

    // Set up audio
    app.configureAudio(48000., 512, 2, 0);

    app.start();
}
