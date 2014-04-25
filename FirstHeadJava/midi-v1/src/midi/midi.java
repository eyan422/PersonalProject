package midi;

import javax.sound.midi.*;
public class midi { 
	public static void main(String[] args) { 
		try 
		{
			Sequencer sequencer = MidiSystem.getSequencer(); 
			sequencer.open();
			Sequence seq = new Sequence(Sequence.PPQ, 4);
			Track track = seq.createTrack();
			
			for (int i = 5; i < 61; i+= 4) {
				track.add(midi.makeEvent(144,1,i,100,i));
				track.add(midi.makeEvent(128,1,i,100,i + 2));
			} // end loop
			
			sequencer.setSequence(seq);
			sequencer.setTempoInBPM(220);
			
			sequencer.start();
			
		} catch (Exception ex) {ex.printStackTrace();}
	} // close main
	
	public static MidiEvent makeEvent(int comd, int chan, int one, int two, int tick) {
		MidiEvent event = null;
		try 
		{
			ShortMessage a = new ShortMessage();
			a.setMessage(comd, chan, one, two);
			event = new MidiEvent(a, tick);
		}catch(Exception e) { }
		
		return event;
	}
} // close class
