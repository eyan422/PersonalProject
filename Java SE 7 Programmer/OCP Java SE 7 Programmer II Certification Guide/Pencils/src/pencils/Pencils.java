package pencils;

class Instrument {
	Instrument() {
		System.out.println("Instrument:constructor");
	}
	{
		System.out.println("Instrument: initializer");
	}
	
	static {
		System.out.println("Instrument: init");
	}
}

class Pencils extends Instrument {
	public Pencils() {
		System.out.println("Pencil:constructor");
	}
	
	static {
		System.out.println("Pencils: init");
	}
	
	{
		System.out.println("Pencil:instance initializer");
	}
	
	public static void main(String[] args) {
		new Pencils();
	}
}
