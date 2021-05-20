
# notes-halite5

##  improved subtractive synthesis

I wrote an alternative version of the subtractive synthesis demo that provides what I think is a better and more familiar subtractive synthesizer architecture

###  problems and areas for improvement in the existing demo

- envelopes:  The envelope duration parameter  Controls the duration of both envelopes, so they cannot be set separately. Furthermore, the filter envelope is controlled by two different envelopes for both the CF (cutoff frequency) and BW (bandwidth). In addition, there is no explicit control of the resonance boost at the cutoff frequency. Finally, since the envelopes are symmetrical, controlled by only a  start parameter, end parameter, and rise speed perimeter.  because of this, both envelopes rise and fall at the same rate, greatly limiting sound possibilities.

### minisubwaves: better subsyn

The subtractive synthesizer is based on the same general outline of the existing subtractive synthesizer demo, but fixes the issues outlined above. The new subtractive synthesizer has controls that are more like those of actual subtractive synthesizer instruments, with separate envelopes and resonant filters.

A general outline:
- mixable sawtooth/square/noise oscillators
- independent amplitude and filter ADSR envelopes
- resonant low pass filter with boost band at cutoff frequency
