
# notes-halite5

##  improved subtractive synthesis

I wrote an alternative version of the built in [subtractive synthesis demo](https://github.com/AlloSphere-Research-Group/allolib_playground/blob/master/tutorials/synthesis/08_SubSyn.cpp) that provides what I think is a better and more familiar subtractive synthesizer architecture

###  problems and areas for improvement in the existing demo

- The envelope duration parameter  Controls the duration of both envelopes, so they cannot be set separately. 
- The filter is controlled by two different envelopes for both the CF (cutoff frequency) and BW (bandwidth).
- There is no explicit control of the resonance boost at the cutoff frequency. 
- Since the envelopes are symmetrical, controlled by only a  start parameter, end parameter, and rise speed perimeter.  because of this, both envelopes rise and fall at the same rate, greatly limiting sound possibilities.

### minisubwaves: better subsyn

The subtractive synthesizer is based on the same general outline of the existing subtractive synthesizer demo, but fixes the issues outlined above. The new subtractive synthesizer has controls that are more like those of actual subtractive synthesizer instruments, with separate envelopes and resonant filters.

A general outline:
- mixable sawtooth/square/noise oscillators
- independent amplitude and filter ADSR envelopes
- resonant low pass filter with boost band at cutoff frequency

### code

The code for the demo can be found [here](https://github.com/allolib-s21/notes-halite5/blob/main/demos/18_AdvSubSyn.cpp).

## multi instrument sequencing

### summary

This is an expansion of pconrad's [frere jacque two voices code](https://github.com/allolib-s21/demo1-pconrad/blob/161e44305b9f070606bf4267c71e6298e47c1b8d/tutorials/allolib-s21/040_FrereJacquesTwoVoices.cpp) that allows sequencing notes and playing them back from separate instruments.
One important feature that code lacks is the ability to provide different sequences of notes for each instrument. This is important because in real pieces, it is rare that all the instruments are always playing the exact same notes.

### code
[my code](https://github.com/allolib-s21/notes-halite5/blob/main/demos/19_GrumpyHatBase.cpp) is an expansion on that demo, providing this feature, and demonstrating it with two separate instruments.

## karplus-strong demo

### summary

I made a basic implementation of Karplus-Strong synthesis to create a string-like sound using a delay.  this demonstration is implemented simply as an extension of my MiniSubWaves code, creating a variant that has a comb filter applied after the main resonant LP filter.
the result is some very rough sounds that are nevertheless quite reminiscent of physical string instruments.

### explanation
karplus strong  synthesis works by utilizing a noise burst that can be modulated by an envelope, that is then passed through a low pass filter. finally, the signal is  passed through a very short delay. Very short delays can be represented/simulated by comb filters. comb  filters create a pattern that strongly resembles the interference pattern caused by playing a signal along with itswlf a very short delay after. The delay time controls the pitch. Longer delays corresponds to lower pitches, while shorter delays correspond to higher pitches. The equation for the fundamental frequency of the string sound is given by `F0=Fs/Fs`, where `Fs` is the sampling frequency, `Fn` is the note frequency, and `F0` is the fundamental frequency. Since allolib  currently uses a sampling frequency of 44100 Hz, we can use this equation to calculate the delay required to create a note with the desired fundamental frequency. The expression for the delay (in seconds) is given by `D = (44100/Fn)/44100`. The demo uses this formula to automatically calculate the delay so that notes can be produced at the proper frequency by dynamically adjusting the delay time.

### code
[demo](https://github.com/allolib-s21/notes-halite5/blob/main/demos/22_AdvSubSynV3.cpp)

## putting everything together

Finally, we can put all these components together into a simple composition that utilize each of these elements. it will include our improved subtractive synthesis, our multi instrument sequencing code, and finally, our code to synthesize string like sounds using the Karplus Strong algorithm.

[demo](https://github.com/allolib-s21/notes-halite5/blob/main/demos/25_GrumpyKP.cpp)
