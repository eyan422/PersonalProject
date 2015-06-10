package twistIntalecasting;

class Ink{}
interface Printable {}
class ColorInk extends Ink implements Printable {}
class BlackInk extends Ink{}

class TwistInTaleCasting {
	public static void main(String args[]) {
		
		Printable printable = null;
		BlackInk blackInk = new BlackInk();
		printable = (Printable)blackInk;
		
	}
}
