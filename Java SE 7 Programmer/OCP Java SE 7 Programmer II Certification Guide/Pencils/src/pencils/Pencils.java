package pencils;

class Instrument {
	Instrument() {
		System.out.println("Instrument:constructor");
	}
}

class Pencils extends Instrument {
	public Pencils() {
		System.out.println("Pencil:constructor");
	}
	{
		System.out.println("Pencil:instance initializer");
	}
	
	public static void main(String[] args) {
		new Pencils();
	}
}
